#include "islisp_runtime.h"
#include <ctype.h>
#include <stdarg.h>

/* Global symbols */
islisp_val SYM_T = 0;
islisp_val SYM_NIL = 0;
islisp_val SYM_REST = 0;
islisp_val SYM_COLON_REST = 0;

islisp_frame_t *g_top_frame = NULL;
islisp_val g_last_condition = ISLISP_NIL;

/* Garbage Collector Definitions */
#define ISLISP_GC_MAGIC 0x4953 /* 'IS' */
#define GC_OBJ_CONS     0
#define GC_OBJ_HEAP     1
#define GC_INITIAL_THRESHOLD (2 * 1024 * 1024) /* 2 MB */
#define GC_MIN_THRESHOLD     (1024 * 1024)     /* 1 MB */

typedef struct islisp_gc_header {
    struct islisp_gc_header *next;
    uint32_t size;
    uint8_t mark;
    uint8_t obj_type;
    uint16_t magic;
} islisp_gc_header_t;

typedef struct {
    void **entries;
    size_t capacity;
    size_t count;
} gc_hash_table_t;

static islisp_gc_header_t *g_gc_all_objects = NULL;
static gc_hash_table_t g_gc_table = { NULL, 0, 0 };
static uintptr_t g_gc_min_ptr = UINTPTR_MAX;
static uintptr_t g_gc_max_ptr = 0;
static void *g_stack_bottom = NULL;
static int g_gc_disabled = 0;

static size_t g_gc_bytes_allocated = 0;
static size_t g_gc_threshold = GC_INITIAL_THRESHOLD;
static size_t g_gc_stats_total_allocated = 0;
static size_t g_gc_stats_total_objects = 0;
static size_t g_gc_stats_collections = 0;
static size_t g_gc_stats_freed_objects = 0;
static size_t g_gc_stats_freed_bytes = 0;

/* Registered roots */
typedef struct islisp_gc_root_entry {
    islisp_val *ptr;
    struct islisp_gc_root_entry *next;
} islisp_gc_root_entry_t;

typedef struct islisp_gc_root_array_entry {
    islisp_val *arr;
    size_t count;
    struct islisp_gc_root_array_entry *next;
} islisp_gc_root_array_entry_t;

static islisp_gc_root_entry_t *g_gc_roots = NULL;
static islisp_gc_root_array_entry_t *g_gc_root_arrays = NULL;

#define SYMBOL_TABLE_SIZE 1024
static islisp_symbol_t *g_symbol_table[SYMBOL_TABLE_SIZE];
static uint64_t g_gensym_counter = 1;

static inline bool gc_hash_lookup(gc_hash_table_t *table, void *ptr) {
    if (!table->entries || table->count == 0) return false;
    size_t mask = table->capacity - 1;
    size_t idx = (((uintptr_t)ptr >> 4) * 2654435761u) & mask;
    while (table->entries[idx] != NULL) {
        if (table->entries[idx] == ptr) return true;
        idx = (idx + 1) & mask;
    }
    return false;
}

static void gc_hash_insert(gc_hash_table_t *table, void *ptr) {
    if (table->capacity == 0 || table->count * 2 >= table->capacity) {
        size_t new_cap = table->capacity == 0 ? 1024 : table->capacity * 2;
        void **new_entries = (void**)calloc(new_cap, sizeof(void*));
        size_t new_mask = new_cap - 1;
        if (table->entries) {
            for (size_t i = 0; i < table->capacity; i++) {
                void *p = table->entries[i];
                if (p) {
                    size_t idx = (((uintptr_t)p >> 4) * 2654435761u) & new_mask;
                    while (new_entries[idx] != NULL) {
                        idx = (idx + 1) & new_mask;
                    }
                    new_entries[idx] = p;
                }
            }
            free(table->entries);
        }
        table->entries = new_entries;
        table->capacity = new_cap;
    }
    size_t mask = table->capacity - 1;
    size_t idx = (((uintptr_t)ptr >> 4) * 2654435761u) & mask;
    while (table->entries[idx] != NULL) {
        if (table->entries[idx] == ptr) return;
        idx = (idx + 1) & mask;
    }
    table->entries[idx] = ptr;
    table->count++;
}

static void gc_hash_rebuild(gc_hash_table_t *table, islisp_gc_header_t *live_list) {
    size_t live_count = 0;
    islisp_gc_header_t *curr = live_list;
    while (curr) {
        live_count++;
        curr = curr->next;
    }
    size_t cap = 1024;
    while (cap < live_count * 2) {
        cap *= 2;
    }
    free(table->entries);
    table->entries = (void**)calloc(cap, sizeof(void*));
    table->capacity = cap;
    table->count = live_count;
    size_t mask = cap - 1;

    g_gc_min_ptr = UINTPTR_MAX;
    g_gc_max_ptr = 0;

    curr = live_list;
    while (curr) {
        void *ptr = (void*)(curr + 1);
        uintptr_t u = (uintptr_t)ptr;
        if (u < g_gc_min_ptr) g_gc_min_ptr = u;
        if (u > g_gc_max_ptr) g_gc_max_ptr = u;

        size_t idx = (((uintptr_t)ptr >> 4) * 2654435761u) & mask;
        while (table->entries[idx] != NULL) {
            idx = (idx + 1) & mask;
        }
        table->entries[idx] = ptr;
        curr = curr->next;
    }
}

static inline bool gc_is_valid_object(void *ptr) {
    uintptr_t u = (uintptr_t)ptr;
    if (u < g_gc_min_ptr || u > g_gc_max_ptr) return false;
    if ((u & 0x7) != 0) return false;
    return gc_hash_lookup(&g_gc_table, ptr);
}

static void* islisp_alloc_internal(size_t size, uint8_t obj_type) {
    if (g_gc_bytes_allocated + size >= g_gc_threshold && !g_gc_disabled) {
        islisp_gc_collect();
    }

    size_t aligned_size = (size + 7) & ~7;
    size_t total_size = sizeof(islisp_gc_header_t) + aligned_size;

    islisp_gc_header_t *hdr = (islisp_gc_header_t*)malloc(total_size);
    if (!hdr) {
        islisp_gc_collect();
        hdr = (islisp_gc_header_t*)malloc(total_size);
        if (!hdr) {
            fprintf(stderr, "Out of memory!\n");
            exit(1);
        }
    }

    hdr->size = (uint32_t)aligned_size;
    hdr->mark = 0;
    hdr->obj_type = obj_type;
    hdr->magic = ISLISP_GC_MAGIC;

    hdr->next = g_gc_all_objects;
    g_gc_all_objects = hdr;

    void *payload = (void*)(hdr + 1);
    memset(payload, 0, aligned_size);

    g_gc_bytes_allocated += aligned_size;
    g_gc_stats_total_allocated += aligned_size;
    g_gc_stats_total_objects++;

    uintptr_t u = (uintptr_t)payload;
    if (u < g_gc_min_ptr) g_gc_min_ptr = u;
    if (u > g_gc_max_ptr) g_gc_max_ptr = u;

    gc_hash_insert(&g_gc_table, payload);

    return payload;
}

void* islisp_alloc(size_t size) {
    return islisp_alloc_internal(size, GC_OBJ_HEAP);
}

void* islisp_alloc_cons(void) {
    return islisp_alloc_internal(sizeof(islisp_cons_t), GC_OBJ_CONS);
}

void islisp_gc_mark_object(void *ptr) {
    if (!ptr) return;
    islisp_gc_header_t *hdr = ((islisp_gc_header_t*)ptr) - 1;
    if (hdr->magic != ISLISP_GC_MAGIC) return;
    if (hdr->mark) return;
    hdr->mark = 1;

    if (hdr->obj_type == GC_OBJ_CONS) {
        islisp_cons_t *c = (islisp_cons_t*)ptr;
        while (c) {
            islisp_gc_mark_val(c->car);
            islisp_val next = c->cdr;
            if (IS_CONS(next)) {
                islisp_cons_t *next_c = AS_CONS(next);
                if (gc_is_valid_object(next_c)) {
                    islisp_gc_header_t *next_hdr = ((islisp_gc_header_t*)next_c) - 1;
                    if (next_hdr->magic == ISLISP_GC_MAGIC && !next_hdr->mark) {
                        next_hdr->mark = 1;
                        c = next_c;
                        continue;
                    }
                }
            }
            islisp_gc_mark_val(next);
            break;
        }
    } else {
        islisp_header_t *h = (islisp_header_t*)ptr;
        switch (h->type) {
            case TYPE_FLOAT:
            case TYPE_STRING:
            case TYPE_STREAM:
                break;
            case TYPE_SYMBOL: {
                islisp_symbol_t *sym = (islisp_symbol_t*)ptr;
                islisp_gc_mark_val(sym->global_val);
                islisp_gc_mark_val(sym->func_val);
                islisp_gc_mark_val(sym->dynamic_val);
                islisp_gc_mark_val(sym->plist);
                break;
            }
            case TYPE_VECTOR: {
                islisp_vector_t *vec = (islisp_vector_t*)ptr;
                if (vec->data) {
                    for (uint32_t i = 0; i < vec->header.len; i++) {
                        islisp_gc_mark_val(vec->data[i]);
                    }
                }
                break;
            }
            case TYPE_ARRAY: {
                islisp_array_t *arr = (islisp_array_t*)ptr;
                if (arr->data) {
                    for (uint32_t i = 0; i < arr->header.len; i++) {
                        islisp_gc_mark_val(arr->data[i]);
                    }
                }
                break;
            }
            case TYPE_CLOSURE: {
                islisp_closure_t *cl = (islisp_closure_t*)ptr;
                islisp_gc_mark_val(cl->env);
                break;
            }
            case TYPE_CLASS: {
                islisp_class_t *cls = (islisp_class_t*)ptr;
                islisp_gc_mark_val(cls->name);
                islisp_gc_mark_val(cls->cpl);
                islisp_gc_mark_val(cls->direct_supers);
                islisp_gc_mark_val(cls->direct_subclasses);
                islisp_gc_mark_val(cls->slots);
                break;
            }
            case TYPE_INSTANCE: {
                islisp_instance_t *inst = (islisp_instance_t*)ptr;
                islisp_gc_mark_val(inst->class_obj);
                if (inst->slots) {
                    for (int i = 0; i < inst->num_slots; i++) {
                        islisp_gc_mark_val(inst->slots[i]);
                    }
                }
                break;
            }
            case TYPE_GENERIC_FUNCTION: {
                islisp_generic_function_t *gf = (islisp_generic_function_t*)ptr;
                islisp_gc_mark_val(gf->name);
                islisp_method_t *m = gf->methods;
                while (m) {
                    islisp_gc_mark_object(m);
                    m = m->next;
                }
                break;
            }
            case TYPE_METHOD: {
                islisp_method_t *m = (islisp_method_t*)ptr;
                islisp_gc_mark_val(m->specializers);
                if (m->closure) islisp_gc_mark_object(m->closure);
                break;
            }
            case TYPE_CONDITION: {
                islisp_condition_t *cond = (islisp_condition_t*)ptr;
                islisp_gc_mark_val(cond->class_obj);
                islisp_gc_mark_val(cond->irritants);
                islisp_gc_mark_val(cond->extra_data);
                break;
            }
            default:
                break;
        }
    }
}

void islisp_gc_mark_val(islisp_val v) {
    if ((v & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT || v == 0) return;
    uintptr_t tag = v & ISLISP_TAG_PTR_MASK;
    void *ptr = NULL;
    if (tag == ISLISP_TAG_CONS) {
        ptr = (void*)(v ^ ISLISP_TAG_CONS);
    } else if (tag == ISLISP_TAG_HEAP) {
        ptr = (void*)v;
    } else {
        return;
    }
    if (gc_is_valid_object(ptr)) {
        islisp_gc_mark_object(ptr);
    }
}

static void gc_mark_word(uintptr_t w) {
    if ((w & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT || w == 0) return;
    void *ptr = NULL;
    uintptr_t tag = w & ISLISP_TAG_PTR_MASK;
    if (tag == ISLISP_TAG_CONS) {
        ptr = (void*)(w ^ ISLISP_TAG_CONS);
    } else if (tag == ISLISP_TAG_HEAP) {
        ptr = (void*)w;
    } else {
        return;
    }
    if (gc_is_valid_object(ptr)) {
        islisp_gc_mark_object(ptr);
    }
}

static void gc_sweep(void) {
    islisp_gc_header_t *live_head = NULL;
    islisp_gc_header_t *live_tail = NULL;
    islisp_gc_header_t *curr = g_gc_all_objects;
    size_t freed_count = 0;
    size_t freed_bytes = 0;
    size_t live_bytes = 0;

    while (curr) {
        islisp_gc_header_t *next = curr->next;
        if (!curr->mark) {
            void *payload = (void*)(curr + 1);
            if (curr->obj_type == GC_OBJ_HEAP) {
                islisp_header_t *h = (islisp_header_t*)payload;
                switch (h->type) {
                    case TYPE_STRING: {
                        islisp_string_t *s = (islisp_string_t*)payload;
                        if (s->data) free(s->data);
                        break;
                    }
                    case TYPE_VECTOR: {
                        islisp_vector_t *v = (islisp_vector_t*)payload;
                        if (v->data) free(v->data);
                        break;
                    }
                    case TYPE_ARRAY: {
                        islisp_array_t *a = (islisp_array_t*)payload;
                        if (a->dims) free(a->dims);
                        if (a->data) free(a->data);
                        break;
                    }
                    case TYPE_INSTANCE: {
                        islisp_instance_t *inst = (islisp_instance_t*)payload;
                        if (inst->slots) free(inst->slots);
                        break;
                    }
                    case TYPE_SYMBOL: {
                        islisp_symbol_t *sym = (islisp_symbol_t*)payload;
                        if (sym->name) free(sym->name);
                        break;
                    }
                    case TYPE_CLOSURE: {
                        islisp_closure_t *cl = (islisp_closure_t*)payload;
                        if (cl->name) free(cl->name);
                        break;
                    }
                    case TYPE_STREAM: {
                        islisp_stream_t *stm = (islisp_stream_t*)payload;
                        if (stm->file && stm->file != stdin && stm->file != stdout && stm->file != stderr && stm->is_open) {
                            fclose(stm->file);
                            stm->is_open = false;
                        }
                        if (stm->str_buf) free(stm->str_buf);
                        break;
                    }
                    case TYPE_CONDITION: {
                        islisp_condition_t *cond = (islisp_condition_t*)payload;
                        if (cond->message) free(cond->message);
                        break;
                    }
                    default:
                        break;
                }
            }
            freed_count++;
            freed_bytes += curr->size + sizeof(islisp_gc_header_t);
            free(curr);
        } else {
            live_bytes += curr->size;
            curr->mark = 0;
            curr->next = NULL;
            if (!live_head) {
                live_head = curr;
                live_tail = curr;
            } else {
                live_tail->next = curr;
                live_tail = curr;
            }
        }
        curr = next;
    }

    g_gc_all_objects = live_head;
    g_gc_bytes_allocated = live_bytes;
    g_gc_stats_collections++;
    g_gc_stats_freed_objects += freed_count;
    g_gc_stats_freed_bytes += freed_bytes;

    size_t new_threshold = live_bytes * 2;
    if (new_threshold < GC_MIN_THRESHOLD) new_threshold = GC_MIN_THRESHOLD;
    g_gc_threshold = new_threshold;

    gc_hash_rebuild(&g_gc_table, live_head);
}

void islisp_gc_collect(void) {
    if (g_gc_disabled) return;
    g_gc_disabled++;

    /* 1. Flush CPU registers onto stack */
    jmp_buf jmp;
    memset(&jmp, 0, sizeof(jmp));
    setjmp(jmp);

    /* 2. Clear marks on all objects */
    for (islisp_gc_header_t *h = g_gc_all_objects; h; h = h->next) {
        h->mark = 0;
    }

    /* 3. Mark Roots */
    /* 3a. Stack scanning */
    void *stack_top = __builtin_frame_address(0);
    if (g_stack_bottom != NULL) {
        uintptr_t *cur = (uintptr_t*)stack_top;
        uintptr_t *end = (uintptr_t*)g_stack_bottom;
        if (cur > end) {
            uintptr_t *tmp = cur;
            cur = end;
            end = tmp;
        }
        cur = (uintptr_t*)((uintptr_t)cur & ~(uintptr_t)7);
        end = (uintptr_t*)((uintptr_t)end & ~(uintptr_t)7);
        while (cur < end) {
            gc_mark_word(*cur);
            cur++;
        }
    }

    /* 3b. Registers in jmp_buf */
    uintptr_t *reg_cur = (uintptr_t*)&jmp;
    uintptr_t *reg_end = reg_cur + (sizeof(jmp) / (sizeof(uintptr_t)));
    while (reg_cur < reg_end) {
        gc_mark_word(*reg_cur);
        reg_cur++;
    }

    /* 3c. Active frames */
    for (islisp_frame_t *f = g_top_frame; f; f = f->prev) {
        islisp_gc_mark_val(f->tag);
        islisp_gc_mark_val(f->val);
        islisp_gc_mark_val(f->var);
    }

    /* 3d. Method context */
    islisp_gc_mark_method_ctx();

    /* 3e. Symbols */
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        islisp_symbol_t *sym = g_symbol_table[i];
        while (sym) {
            islisp_gc_mark_object(sym);
            sym = sym->next;
        }
    }

    /* 3f. Condition, global classes, well-known symbols */
    islisp_gc_mark_val(g_last_condition);
    islisp_gc_mark_val(SYM_T);
    islisp_gc_mark_val(SYM_NIL);
    islisp_gc_mark_val(SYM_REST);
    islisp_gc_mark_val(SYM_COLON_REST);
    islisp_gc_mark_global_classes();

    /* 3g. User-registered roots */
    for (islisp_gc_root_entry_t *r = g_gc_roots; r; r = r->next) {
        if (r->ptr) {
            islisp_gc_mark_val(*(r->ptr));
        }
    }
    for (islisp_gc_root_array_entry_t *ra = g_gc_root_arrays; ra; ra = ra->next) {
        if (ra->arr) {
            for (size_t i = 0; i < ra->count; i++) {
                islisp_gc_mark_val(ra->arr[i]);
            }
        }
    }

    /* 4. Sweep */
    gc_sweep();

    g_gc_disabled--;
}

void islisp_gc_init(void *stack_bottom) {
    if (stack_bottom) {
        g_stack_bottom = stack_bottom;
    }
    if (g_gc_threshold == 0) {
        g_gc_threshold = GC_INITIAL_THRESHOLD;
    }
}

void islisp_gc_enable(void) {
    if (g_gc_disabled > 0) g_gc_disabled--;
}

void islisp_gc_disable(void) {
    g_gc_disabled++;
}

void islisp_gc_set_threshold(size_t bytes) {
    g_gc_threshold = bytes < GC_MIN_THRESHOLD ? GC_MIN_THRESHOLD : bytes;
}

void islisp_gc_get_stats(islisp_gc_stats_t *out_stats) {
    if (!out_stats) return;
    out_stats->bytes_allocated = g_gc_bytes_allocated;
    out_stats->total_allocated = g_gc_stats_total_allocated;
    out_stats->total_objects = g_gc_stats_total_objects;
    out_stats->collections = g_gc_stats_collections;
    out_stats->freed_objects = g_gc_stats_freed_objects;
    out_stats->freed_bytes = g_gc_stats_freed_bytes;
    out_stats->threshold = g_gc_threshold;
}

void islisp_gc_register_root(islisp_val *ptr) {
    islisp_gc_root_entry_t *entry = (islisp_gc_root_entry_t*)malloc(sizeof(islisp_gc_root_entry_t));
    entry->ptr = ptr;
    entry->next = g_gc_roots;
    g_gc_roots = entry;
}

void islisp_gc_unregister_root(islisp_val *ptr) {
    islisp_gc_root_entry_t **cur = &g_gc_roots;
    while (*cur) {
        if ((*cur)->ptr == ptr) {
            islisp_gc_root_entry_t *to_free = *cur;
            *cur = (*cur)->next;
            free(to_free);
            return;
        }
        cur = &(*cur)->next;
    }
}

void islisp_gc_register_root_array(islisp_val *arr, size_t count) {
    islisp_gc_root_array_entry_t *entry = (islisp_gc_root_array_entry_t*)malloc(sizeof(islisp_gc_root_array_entry_t));
    entry->arr = arr;
    entry->count = count;
    entry->next = g_gc_root_arrays;
    g_gc_root_arrays = entry;
}

void islisp_gc_unregister_root_array(islisp_val *arr) {
    islisp_gc_root_array_entry_t **cur = &g_gc_root_arrays;
    while (*cur) {
        if ((*cur)->arr == arr) {
            islisp_gc_root_array_entry_t *to_free = *cur;
            *cur = (*cur)->next;
            free(to_free);
            return;
        }
        cur = &(*cur)->next;
    }
}

/* Object Constructors */

islisp_val islisp_cons(islisp_val a, islisp_val b) {
    islisp_cons_t *c = (islisp_cons_t*)islisp_alloc_cons();
    c->car = a;
    c->cdr = b;
    return TO_CONS(c);
}

islisp_val islisp_make_float(double d) {
    islisp_float_t *f = (islisp_float_t*)islisp_alloc(sizeof(islisp_float_t));
    f->header.type = TYPE_FLOAT;
    f->header.flags = 0;
    f->header.extra = 0;
    f->header.len = sizeof(double);
    f->val = d;
    return TO_HEAP(f);
}

islisp_val islisp_make_string(const char *s) {
    size_t len = s ? strlen(s) : 0;
    return islisp_make_string_len(s, len);
}

islisp_val islisp_make_string_len(const char *s, size_t len) {
    char *data = (char*)malloc(len + 1);
    if (s && len > 0) {
        memcpy(data, s, len);
    }
    data[len] = '\0';

    islisp_string_t *str = (islisp_string_t*)islisp_alloc(sizeof(islisp_string_t));
    str->header.type = TYPE_STRING;
    str->header.flags = 0;
    str->header.extra = 0;
    str->header.len = (uint32_t)len;
    str->data = data;
    return TO_HEAP(str);
}

islisp_val islisp_make_vector(int len, islisp_val init) {
    islisp_val *data = (islisp_val*)malloc(sizeof(islisp_val) * (len > 0 ? len : 1));
    for (int i = 0; i < len; i++) {
        data[i] = init;
    }

    islisp_vector_t *vec = (islisp_vector_t*)islisp_alloc(sizeof(islisp_vector_t));
    vec->header.type = TYPE_VECTOR;
    vec->header.flags = 0;
    vec->header.extra = 0;
    vec->header.len = (uint32_t)len;
    vec->data = data;
    return TO_HEAP(vec);
}

islisp_val islisp_make_closure(islisp_fn_t fn, islisp_val env, int min_args, int max_args, const char *name) {
    islisp_closure_t *cl = (islisp_closure_t*)islisp_alloc(sizeof(islisp_closure_t));
    cl->header.type = TYPE_CLOSURE;
    cl->header.flags = 0;
    cl->header.extra = 0;
    cl->header.len = 0;
    cl->fn = fn;
    cl->env = env;
    cl->min_args = min_args;
    cl->max_args = max_args;
    cl->name = strdup(name ? name : "anonymous");
    return TO_HEAP(cl);
}

/* Symbol Table */
static uint32_t hash_str(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

islisp_val islisp_intern(const char *name) {
    uint32_t h = hash_str(name) % SYMBOL_TABLE_SIZE;
    islisp_symbol_t *sym = g_symbol_table[h];
    while (sym) {
        if (strcmp(sym->name, name) == 0) {
            return TO_HEAP(sym);
        }
        sym = sym->next;
    }
    sym = (islisp_symbol_t*)islisp_alloc(sizeof(islisp_symbol_t));
    sym->header.type = TYPE_SYMBOL;
    sym->header.flags = 0;
    sym->header.extra = 0;
    sym->header.len = (uint32_t)strlen(name);
    sym->name = strdup(name);
    sym->global_val = ISLISP_UNBOUND;
    sym->func_val = ISLISP_UNBOUND;
    sym->dynamic_val = ISLISP_UNBOUND;
    sym->plist = ISLISP_NIL;
    sym->is_constant = false;
    sym->next = g_symbol_table[h];
    g_symbol_table[h] = sym;
    return TO_HEAP(sym);
}

islisp_val islisp_gensym(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "G%llu", (unsigned long long)(g_gensym_counter++));
    islisp_symbol_t *sym = (islisp_symbol_t*)islisp_alloc(sizeof(islisp_symbol_t));
    sym->header.type = TYPE_SYMBOL;
    sym->header.flags = 0;
    sym->header.extra = 0;
    sym->header.len = (uint32_t)strlen(buf);
    sym->name = strdup(buf);
    sym->global_val = ISLISP_UNBOUND;
    sym->func_val = ISLISP_UNBOUND;
    sym->dynamic_val = ISLISP_UNBOUND;
    sym->plist = ISLISP_NIL;
    sym->is_constant = false;
    sym->next = NULL;
    return TO_HEAP(sym);
}

islisp_val islisp_get_global(islisp_val sym) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("get_global: not a symbol");
        return ISLISP_NIL;
    }
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    if (s->global_val == ISLISP_UNBOUND) {
        islisp_error("Variable %s is unbound", s->name);
    }
    return s->global_val;
}

void islisp_set_global(islisp_val sym, islisp_val val) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("set_global: not a symbol");
        return;
    }
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    if (s->is_constant) {
        islisp_error("Cannot modify constant %s", s->name);
    }
    s->global_val = val;
}

islisp_val islisp_get_function(islisp_val sym) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("get_function: not a symbol");
        return ISLISP_NIL;
    }
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    if (s->func_val == ISLISP_UNBOUND) {
        islisp_error("Function %s is undefined", s->name);
    }
    return s->func_val;
}

void islisp_set_function(islisp_val sym, islisp_val val) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("set_function: not a symbol");
        return;
    }
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    s->func_val = val;
}

islisp_val islisp_get_dynamic(islisp_val sym) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("get_dynamic: not a symbol");
        return ISLISP_NIL;
    }
    /* Search active dynamic frames */
    islisp_frame_t *f = g_top_frame;
    while (f) {
        if (f->kind == FRAME_DYNAMIC && f->tag == sym) {
            return f->val;
        }
        f = f->prev;
    }
    /* Fallback to top-level dynamic binding */
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    if (s->dynamic_val == ISLISP_UNBOUND) {
        islisp_error("Dynamic variable %s is unbound", s->name);
    }
    return s->dynamic_val;
}

void islisp_set_dynamic(islisp_val sym, islisp_val val) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("set_dynamic: not a symbol");
        return;
    }
    islisp_frame_t *f = g_top_frame;
    while (f) {
        if (f->kind == FRAME_DYNAMIC && f->tag == sym) {
            f->val = val;
            return;
        }
        f = f->prev;
    }
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    s->dynamic_val = val;
}

islisp_val islisp_property(islisp_val sym, islisp_val prop, islisp_val default_val) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("property: not a symbol");
        return default_val;
    }
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    islisp_val p = s->plist;
    while (IS_CONS(p)) {
        islisp_val entry = CAR(p);
        if (IS_CONS(entry) && CAR(entry) == prop) {
            return CDR(entry);
        }
        p = CDR(p);
    }
    return default_val;
}

islisp_val islisp_set_property(islisp_val val, islisp_val sym, islisp_val prop) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("set-property: not a symbol");
        return val;
    }
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    islisp_val p = s->plist;
    while (IS_CONS(p)) {
        islisp_val entry = CAR(p);
        if (IS_CONS(entry) && CAR(entry) == prop) {
            AS_CONS(entry)->cdr = val;
            return val;
        }
        p = CDR(p);
    }
    s->plist = islisp_cons(islisp_cons(prop, val), s->plist);
    return val;
}

islisp_val islisp_remove_property(islisp_val sym, islisp_val prop) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("remove-property: not a symbol");
        return ISLISP_NIL;
    }
    islisp_symbol_t *s = (islisp_symbol_t*)sym;
    islisp_val p = s->plist;
    islisp_val prev = ISLISP_NIL;
    while (IS_CONS(p)) {
        islisp_val entry = CAR(p);
        if (IS_CONS(entry) && CAR(entry) == prop) {
            if (IS_NIL(prev)) {
                s->plist = CDR(p);
            } else {
                AS_CONS(prev)->cdr = CDR(p);
            }
            return CDR(entry);
        }
        prev = p;
        p = CDR(p);
    }
    return ISLISP_NIL;
}

islisp_val islisp_symbol_name(islisp_val sym) {
    if (!IS_SYMBOL(sym)) {
        islisp_error("symbol-name: not a symbol");
        return ISLISP_NIL;
    }
    return islisp_make_string(((islisp_symbol_t*)sym)->name);
}

/* Function Calling and Application */

islisp_val islisp_funcall_argv(islisp_val fn, int argc, islisp_val *argv) {
    if (!IS_CLOSURE(fn)) {
        islisp_error("funcall: object is not a function");
        return ISLISP_NIL;
    }
    islisp_closure_t *cl = (islisp_closure_t*)fn;
    if (cl->max_args == -1) {
        if (argc < cl->min_args) {
            islisp_error("Too few arguments to %s: expected at least %d, got %d", cl->name, cl->min_args, argc);
        }
        return cl->fn(cl->env, argc, argv);
    } else {
        if (argc < cl->min_args || argc > cl->max_args) {
            islisp_error("Wrong number of arguments to %s: expected %d, got %d", cl->name, cl->min_args, argc);
        }
        return cl->fn(cl->env, argc, argv);
    }
}

islisp_val islisp_funcall(islisp_val fn, int argc, ...) {
    islisp_val stack_argv[16];
    islisp_val *argv = NULL;
    if (argc > 0) {
        if (argc <= 16) {
            argv = stack_argv;
        } else {
            argv = (islisp_val*)malloc(sizeof(islisp_val) * argc);
            islisp_gc_register_root_array(argv, argc);
        }
        va_list ap;
        va_start(ap, argc);
        for (int i = 0; i < argc; i++) {
            argv[i] = va_arg(ap, islisp_val);
        }
        va_end(ap);
    }
    islisp_val res = islisp_funcall_argv(fn, argc, argv);
    if (argv && argv != stack_argv) {
        islisp_gc_unregister_root_array(argv);
        free(argv);
    }
    return res;
}

islisp_val islisp_apply(islisp_val fn, int argc, islisp_val *argv) {
    if (argc < 1) {
        islisp_error("apply: requires at least 2 arguments");
        return ISLISP_NIL;
    }
    islisp_val last = argv[argc - 1];
    int list_len = 0;
    islisp_val p = last;
    while (IS_CONS(p)) {
        list_len++;
        p = CDR(p);
    }
    if (!IS_NIL(p) && list_len == 0 && last != ISLISP_NIL) {
        islisp_error("apply: last argument must be a proper list");
        return ISLISP_NIL;
    }
    int total_args = (argc - 1) + list_len;
    islisp_val stack_argv[32];
    islisp_val *all_argv = NULL;
    if (total_args <= 32) {
        all_argv = stack_argv;
    } else {
        all_argv = (islisp_val*)malloc(sizeof(islisp_val) * total_args);
        islisp_gc_register_root_array(all_argv, total_args);
    }
    for (int i = 0; i < argc - 1; i++) {
        all_argv[i] = argv[i];
    }
    p = last;
    for (int i = 0; i < list_len; i++) {
        all_argv[argc - 1 + i] = CAR(p);
        p = CDR(p);
    }
    islisp_val res = islisp_funcall_argv(fn, total_args, all_argv);
    if (all_argv != stack_argv) {
        islisp_gc_unregister_root_array(all_argv);
        free(all_argv);
    }
    return res;
}

islisp_val islisp_identity(islisp_val obj) {
    return obj;
}

/* Predicates & Equality */

islisp_val islisp_eq(islisp_val a, islisp_val b) {
    return BOOL_VAL(a == b);
}

islisp_val islisp_eql(islisp_val a, islisp_val b) {
    if (a == b) return ISLISP_T;
    if (IS_INT(a) && IS_INT(b)) {
        return BOOL_VAL(AS_INT(a) == AS_INT(b));
    }
    if (IS_FLOAT(a) && IS_FLOAT(b)) {
        return BOOL_VAL(((islisp_float_t*)a)->val == ((islisp_float_t*)b)->val);
    }
    if (IS_CHAR(a) && IS_CHAR(b)) {
        return BOOL_VAL(AS_CHAR(a) == AS_CHAR(b));
    }
    return ISLISP_NIL;
}

islisp_val islisp_equal(islisp_val a, islisp_val b) {
    if (IS_TRUE(islisp_eql(a, b))) return ISLISP_T;
    if (IS_CONS(a) && IS_CONS(b)) {
        while (IS_CONS(a) && IS_CONS(b)) {
            if (!IS_TRUE(islisp_equal(CAR(a), CAR(b)))) return ISLISP_NIL;
            a = CDR(a);
            b = CDR(b);
        }
        return islisp_equal(a, b);
    }
    if (IS_STRING(a) && IS_STRING(b)) {
        islisp_string_t *sa = (islisp_string_t*)a;
        islisp_string_t *sb = (islisp_string_t*)b;
        if (sa->header.len != sb->header.len) return ISLISP_NIL;
        return BOOL_VAL(strcmp(sa->data, sb->data) == 0);
    }
    if (IS_VECTOR(a) && IS_VECTOR(b)) {
        islisp_vector_t *va = (islisp_vector_t*)a;
        islisp_vector_t *vb = (islisp_vector_t*)b;
        if (va->header.len != vb->header.len) return ISLISP_NIL;
        for (uint32_t i = 0; i < va->header.len; i++) {
            if (!IS_TRUE(islisp_equal(va->data[i], vb->data[i]))) return ISLISP_NIL;
        }
        return ISLISP_T;
    }
    return ISLISP_NIL;
}

islisp_val islisp_not(islisp_val x) {
    return BOOL_VAL(IS_NIL(x));
}

islisp_val islisp_null(islisp_val x) {
    return BOOL_VAL(IS_NIL(x));
}

islisp_val islisp_symbolp(islisp_val x) {
    return BOOL_VAL(IS_SYMBOL(x) || IS_NIL(x) || IS_T(x));
}

islisp_val islisp_numberp(islisp_val x) {
    return BOOL_VAL(IS_INT(x) || IS_FLOAT(x));
}

islisp_val islisp_integerp(islisp_val x) {
    return BOOL_VAL(IS_INT(x));
}

islisp_val islisp_floatp(islisp_val x) {
    return BOOL_VAL(IS_FLOAT(x));
}

islisp_val islisp_characterp(islisp_val x) {
    return BOOL_VAL(IS_CHAR(x));
}

islisp_val islisp_consp(islisp_val x) {
    return BOOL_VAL(IS_CONS(x));
}

islisp_val islisp_listp(islisp_val x) {
    return BOOL_VAL(IS_CONS(x) || IS_NIL(x));
}

islisp_val islisp_basic_vector_p(islisp_val x) {
    return BOOL_VAL(IS_VECTOR(x) || IS_STRING(x));
}

islisp_val islisp_general_vector_p(islisp_val x) {
    return BOOL_VAL(IS_VECTOR(x));
}

islisp_val islisp_stringp(islisp_val x) {
    return BOOL_VAL(IS_STRING(x));
}

islisp_val islisp_streamp(islisp_val x) {
    return BOOL_VAL(IS_STREAM(x));
}

islisp_val islisp_functionp(islisp_val x) {
    return BOOL_VAL(IS_CLOSURE(x) || IS_GENERIC(x));
}

islisp_val islisp_generic_function_p(islisp_val x) {
    return BOOL_VAL(IS_GENERIC(x));
}
