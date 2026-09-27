#include "islisp_runtime.h"

typedef struct islisp_method_context {
    struct islisp_method_context *prev;
    islisp_method_t *next_primary;
    islisp_method_t *next_around;
    int argc;
    islisp_val *argv;
} islisp_method_context_t;

static islisp_method_context_t *g_method_ctx = NULL;

void islisp_gc_mark_method_ctx(void) {
    islisp_method_context_t *ctx = g_method_ctx;
    while (ctx) {
        islisp_method_t *m = ctx->next_primary;
        while (m) {
            islisp_gc_mark_object(m);
            m = m->next;
        }
        m = ctx->next_around;
        while (m) {
            islisp_gc_mark_object(m);
            m = m->next;
        }
        if (ctx->argv) {
            for (int i = 0; i < ctx->argc; i++) {
                islisp_gc_mark_val(ctx->argv[i]);
            }
        }
        ctx = ctx->prev;
    }
}

static islisp_val make_class(const char *name_str, islisp_val supers, islisp_val slots, bool is_abstract) {
    islisp_val sym = islisp_intern(name_str);
    islisp_class_t *c = (islisp_class_t*)islisp_alloc(sizeof(islisp_class_t));
    c->header.type = TYPE_CLASS;
    c->header.flags = 0;
    c->header.extra = 0;
    c->header.len = 0;
    c->name = sym;
    c->direct_supers = supers;
    c->direct_subclasses = ISLISP_NIL;
    c->slots = slots;
    c->is_abstract = is_abstract;

    /* Compute CPL */
    islisp_val cpl = islisp_cons(TO_HEAP(c), ISLISP_NIL);
    islisp_val *cpl_tail = &(AS_CONS(cpl)->cdr);
    islisp_val p = supers;
    while (IS_CONS(p)) {
        islisp_val sup = CAR(p);
        if (IS_SYMBOL(sup)) sup = islisp_get_global(sup);
        if (IS_CLASS(sup)) {
            islisp_val sup_cpl = ((islisp_class_t*)sup)->cpl;
            while (IS_CONS(sup_cpl)) {
                islisp_val sc = CAR(sup_cpl);
                if (IS_NIL(islisp_member(sc, cpl))) {
                    islisp_val cell = islisp_cons(sc, ISLISP_NIL);
                    *cpl_tail = cell;
                    cpl_tail = &(AS_CONS(cell)->cdr);
                }
                sup_cpl = CDR(sup_cpl);
            }
        }
        p = CDR(p);
    }

    /* Compute merged slots */
    islisp_val all_slots = ISLISP_NIL;
    islisp_val *as_tail = &all_slots;

    p = cpl;
    while (IS_CONS(p)) {
        islisp_val sc = CAR(p);
        islisp_val sc_slots = (sc == TO_HEAP(c)) ? slots : (IS_CLASS(sc) ? ((islisp_class_t*)sc)->slots : ISLISP_NIL);
        while (IS_CONS(sc_slots)) {
            islisp_val s = CAR(sc_slots);
            islisp_val sname = IS_CONS(s) ? CAR(s) : s;
            bool exists = false;
            islisp_val q = all_slots;
            while (IS_CONS(q)) {
                islisp_val es = CAR(q);
                if ((IS_CONS(es) ? CAR(es) : es) == sname) {
                    exists = true;
                    break;
                }
                q = CDR(q);
            }
            if (!exists) {
                islisp_val cell = islisp_cons(s, ISLISP_NIL);
                *as_tail = cell;
                as_tail = &(AS_CONS(cell)->cdr);
            }
            sc_slots = CDR(sc_slots);
        }
        p = CDR(p);
    }
    c->slots = all_slots;
    c->cpl = cpl;
    islisp_set_global(sym, TO_HEAP(c));
    return TO_HEAP(c);
}

static bool g_runtime_initialized = false;

void islisp_init_runtime(void) {
    if (g_runtime_initialized) return;
    g_runtime_initialized = true;

    volatile int dummy = 0;
    islisp_gc_init((void*)&dummy);

    /* Initialize basic symbols */
    SYM_T = islisp_intern("t");
    SYM_NIL = islisp_intern("nil");
    SYM_REST = islisp_intern("&rest");
    SYM_COLON_REST = islisp_intern(":rest");

    islisp_set_global(SYM_T, ISLISP_T);
    ((islisp_symbol_t*)SYM_T)->is_constant = true;
    islisp_set_global(SYM_NIL, ISLISP_NIL);
    ((islisp_symbol_t*)SYM_NIL)->is_constant = true;

    /* Predefined constants */
    islisp_val sym_pi = islisp_intern("*pi*");
    islisp_set_global(sym_pi, islisp_make_float(3.14159265358979323846));
    ((islisp_symbol_t*)sym_pi)->is_constant = true;

    islisp_val sym_most_pos = islisp_intern("*most-positive-float*");
    islisp_set_global(sym_most_pos, islisp_make_float(1.7976931348623157e+308));
    ((islisp_symbol_t*)sym_most_pos)->is_constant = true;

    islisp_val sym_most_neg = islisp_intern("*most-negative-float*");
    islisp_set_global(sym_most_neg, islisp_make_float(-1.7976931348623157e+308));
    ((islisp_symbol_t*)sym_most_neg)->is_constant = true;

    /* Setup class hierarchy */
    CLASS_OBJECT = make_class("<object>", ISLISP_NIL, ISLISP_NIL, true);
    CLASS_STANDARD_OBJECT = make_class("<standard-object>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, false);

    CLASS_NUMBER = make_class("<number>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, true);
    CLASS_INTEGER = make_class("<integer>", islisp_cons(CLASS_NUMBER, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_FLOAT = make_class("<float>", islisp_cons(CLASS_NUMBER, ISLISP_NIL), ISLISP_NIL, false);

    CLASS_CHARACTER = make_class("<character>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_SYMBOL = make_class("<symbol>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, false);

    CLASS_LIST = make_class("<list>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, true);
    CLASS_CONS = make_class("<cons>", islisp_cons(CLASS_LIST, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_NULL = make_class("<null>", islisp_cons(CLASS_SYMBOL, islisp_cons(CLASS_LIST, ISLISP_NIL)), ISLISP_NIL, false);

    CLASS_BASIC_ARRAY = make_class("<basic-array>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, true);
    CLASS_BASIC_ARRAY_STAR = make_class("<basic-array*>", islisp_cons(CLASS_BASIC_ARRAY, ISLISP_NIL), ISLISP_NIL, true);
    CLASS_GENERAL_ARRAY_STAR = make_class("<general-array*>", islisp_cons(CLASS_BASIC_ARRAY_STAR, ISLISP_NIL), ISLISP_NIL, false);

    CLASS_BASIC_VECTOR = make_class("<basic-vector>", islisp_cons(CLASS_BASIC_ARRAY, ISLISP_NIL), ISLISP_NIL, true);
    CLASS_GENERAL_VECTOR = make_class("<general-vector>", islisp_cons(CLASS_BASIC_VECTOR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_STRING = make_class("<string>", islisp_cons(CLASS_BASIC_VECTOR, ISLISP_NIL), ISLISP_NIL, false);

    CLASS_STREAM = make_class("<stream>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_FUNCTION = make_class("<function>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_GENERIC_FUNCTION = make_class("<generic-function>", islisp_cons(CLASS_FUNCTION, ISLISP_NIL), ISLISP_NIL, true);
    CLASS_STANDARD_GENERIC_FUNCTION = make_class("<standard-generic-function>", islisp_cons(CLASS_GENERIC_FUNCTION, ISLISP_NIL), ISLISP_NIL, false);

    CLASS_SERIOUS_CONDITION = make_class("<serious-condition>", islisp_cons(CLASS_OBJECT, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_ERROR = make_class("<error>", islisp_cons(CLASS_SERIOUS_CONDITION, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_SIMPLE_ERROR = make_class("<simple-error>", islisp_cons(CLASS_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_ARITHMETIC_ERROR = make_class("<arithmetic-error>", islisp_cons(CLASS_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_DIVISION_BY_ZERO = make_class("<division-by-zero>", islisp_cons(CLASS_ARITHMETIC_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_FLOATING_POINT_OVERFLOW = make_class("<floating-point-overflow>", islisp_cons(CLASS_ARITHMETIC_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_FLOATING_POINT_UNDERFLOW = make_class("<floating-point-underflow>", islisp_cons(CLASS_ARITHMETIC_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_CONTROL_ERROR = make_class("<control-error>", islisp_cons(CLASS_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_DOMAIN_ERROR = make_class("<domain-error>", islisp_cons(CLASS_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_PARSE_ERROR = make_class("<parse-error>", islisp_cons(CLASS_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_PROGRAM_ERROR = make_class("<program-error>", islisp_cons(CLASS_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_STREAM_ERROR = make_class("<stream-error>", islisp_cons(CLASS_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_END_OF_STREAM = make_class("<end-of-stream>", islisp_cons(CLASS_STREAM_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_STORAGE_EXHAUSTED = make_class("<storage-exhausted>", islisp_cons(CLASS_SERIOUS_CONDITION, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_UNDEFINED_ENTITY = make_class("<undefined-entity>", islisp_cons(CLASS_ERROR, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_UNBOUND_VARIABLE = make_class("<unbound-variable>", islisp_cons(CLASS_UNDEFINED_ENTITY, ISLISP_NIL), ISLISP_NIL, false);
    CLASS_UNDEFINED_FUNCTION = make_class("<undefined-function>", islisp_cons(CLASS_UNDEFINED_ENTITY, ISLISP_NIL), ISLISP_NIL, false);

    islisp_register_builtins();
}

islisp_val islisp_class_of(islisp_val obj) {
    if (IS_INT(obj)) return CLASS_INTEGER;
    if (IS_FLOAT(obj)) return CLASS_FLOAT;
    if (IS_CHAR(obj)) return CLASS_CHARACTER;
    if (IS_NIL(obj)) return CLASS_NULL;
    if (IS_SYMBOL(obj) || IS_T(obj)) return CLASS_SYMBOL;
    if (IS_CONS(obj)) return CLASS_CONS;
    if (IS_STRING(obj)) return CLASS_STRING;
    if (IS_VECTOR(obj)) return CLASS_GENERAL_VECTOR;
    if (IS_ARRAY(obj)) return CLASS_GENERAL_ARRAY_STAR;
    if (IS_CLOSURE(obj)) return CLASS_FUNCTION;
    if (IS_GENERIC(obj)) return CLASS_STANDARD_GENERIC_FUNCTION;
    if (IS_STREAM(obj)) return CLASS_STREAM;
    if (IS_CLASS(obj)) return CLASS_OBJECT;
    if (IS_INSTANCE(obj)) return ((islisp_instance_t*)obj)->class_obj;
    if (IS_CONDITION(obj)) return ((islisp_condition_t*)obj)->class_obj;
    return CLASS_OBJECT;
}

islisp_val islisp_subclassp(islisp_val c1, islisp_val c2) {
    if (IS_SYMBOL(c1)) c1 = islisp_get_global(c1);
    if (IS_SYMBOL(c2)) c2 = islisp_get_global(c2);
    if (!IS_CLASS(c1) || !IS_CLASS(c2)) return ISLISP_NIL;
    islisp_val cpl = ((islisp_class_t*)c1)->cpl;
    return IS_NIL(islisp_member(c2, cpl)) ? ISLISP_NIL : ISLISP_T;
}

islisp_val islisp_instancep(islisp_val obj, islisp_val class_obj) {
    if (IS_SYMBOL(class_obj)) class_obj = islisp_get_global(class_obj);
    islisp_val obj_class = islisp_class_of(obj);
    return islisp_subclassp(obj_class, class_obj);
}

/* Slot descriptors: list of (slot-name :initarg ... :initform ... :reader ... :writer ...) */

islisp_val islisp_defclass(islisp_val name, islisp_val supers, islisp_val slot_specs, bool is_abstract) {
    const char *name_str = IS_SYMBOL(name) ? ((islisp_symbol_t*)name)->name : "anonymous-class";
    if (IS_NIL(supers)) {
        supers = islisp_cons(CLASS_STANDARD_OBJECT, ISLISP_NIL);
    }
    islisp_val cls = make_class(name_str, supers, slot_specs, is_abstract);
    return name;
}

/* Instance Creation */

islisp_val islisp_create(islisp_val class_obj, int argc, islisp_val *argv) {
    if (IS_SYMBOL(class_obj)) class_obj = islisp_get_global(class_obj);
    if (!IS_CLASS(class_obj)) islisp_error("create: first argument must be a class");
    islisp_class_t *c = (islisp_class_t*)class_obj;
    if (c->is_abstract) islisp_error("create: cannot instantiate abstract class %s", ((islisp_symbol_t*)c->name)->name);

    int num_slots = (int)AS_INT(islisp_length(c->slots));
    islisp_instance_t *inst = (islisp_instance_t*)islisp_alloc(sizeof(islisp_instance_t));
    inst->header.type = TYPE_INSTANCE;
    inst->header.flags = 0;
    inst->header.extra = 0;
    inst->header.len = 0;
    inst->class_obj = class_obj;
    inst->num_slots = num_slots;
    inst->slots = (islisp_val*)malloc(sizeof(islisp_val) * (num_slots > 0 ? num_slots : 1));
    for (int i = 0; i < num_slots; i++) {
        inst->slots[i] = ISLISP_UNBOUND;
    }

    /* Process initargs from argv */
    for (int i = 0; i < argc - 1; i += 2) {
        islisp_val initarg = argv[i];
        islisp_val val = argv[i + 1];
        /* Find matching slot */
        int s_idx = 0;
        islisp_val p = c->slots;
        while (IS_CONS(p)) {
            islisp_val s = CAR(p);
            if (IS_CONS(s)) {
                islisp_val opts = CDR(s);
                while (IS_CONS(opts)) {
                    if (CAR(opts) == islisp_intern(":initarg") && IS_CONS(CDR(opts))) {
                        if (CAR(CDR(opts)) == initarg) {
                            inst->slots[s_idx] = val;
                            break;
                        }
                    }
                    opts = CDR(opts);
                }
            }
            s_idx++;
            p = CDR(p);
        }
    }

    islisp_val initargs_list = ISLISP_NIL;
    for (int i = argc - 1; i >= 0; i--) {
        initargs_list = islisp_cons(argv[i], initargs_list);
    }
    islisp_initialize_object(TO_HEAP(inst), initargs_list);
    return TO_HEAP(inst);
}

islisp_val islisp_initialize_object(islisp_val instance, islisp_val initargs) {
    (void)initargs;
    return instance;
}

static int find_slot_index(islisp_instance_t *inst, islisp_val slot_name) {
    islisp_class_t *c = (islisp_class_t*)inst->class_obj;
    int idx = 0;
    islisp_val p = c->slots;
    while (IS_CONS(p)) {
        islisp_val s = CAR(p);
        islisp_val sname = IS_CONS(s) ? CAR(s) : s;
        if (sname == slot_name) {
            return idx;
        }
        idx++;
        p = CDR(p);
    }
    return -1;
}

islisp_val islisp_slot_value(islisp_val instance, islisp_val slot_name) {
    if (!IS_INSTANCE(instance)) islisp_error("slot-value: first argument must be an instance");
    islisp_instance_t *inst = (islisp_instance_t*)instance;
    int idx = find_slot_index(inst, slot_name);
    if (idx < 0 || idx >= inst->num_slots) {
        islisp_error("slot-value: slot %s not found in instance", IS_SYMBOL(slot_name) ? ((islisp_symbol_t*)slot_name)->name : "?");
    }
    if (inst->slots[idx] == ISLISP_UNBOUND) {
        islisp_error("slot-value: slot %s is unbound", IS_SYMBOL(slot_name) ? ((islisp_symbol_t*)slot_name)->name : "?");
    }
    return inst->slots[idx];
}

islisp_val islisp_set_slot_value(islisp_val val, islisp_val instance, islisp_val slot_name) {
    if (!IS_INSTANCE(instance)) islisp_error("set-slot-value: second argument must be an instance");
    islisp_instance_t *inst = (islisp_instance_t*)instance;
    int idx = find_slot_index(inst, slot_name);
    if (idx < 0 || idx >= inst->num_slots) {
        islisp_error("set-slot-value: slot %s not found in instance", IS_SYMBOL(slot_name) ? ((islisp_symbol_t*)slot_name)->name : "?");
    }
    inst->slots[idx] = val;
    return val;
}

islisp_val islisp_slot_boundp(islisp_val instance, islisp_val slot_name) {
    if (!IS_INSTANCE(instance)) return ISLISP_NIL;
    islisp_instance_t *inst = (islisp_instance_t*)instance;
    int idx = find_slot_index(inst, slot_name);
    if (idx < 0 || idx >= inst->num_slots) return ISLISP_NIL;
    return BOOL_VAL(inst->slots[idx] != ISLISP_UNBOUND);
}

static bool method_more_specific(islisp_method_t *m1, islisp_method_t *m2) {
    islisp_val s1 = m1->specializers;
    islisp_val s2 = m2->specializers;
    while (IS_CONS(s1) && IS_CONS(s2)) {
        islisp_val c1 = CAR(s1);
        islisp_val c2 = CAR(s2);
        if (IS_SYMBOL(c1)) c1 = islisp_get_global(c1);
        if (IS_SYMBOL(c2)) c2 = islisp_get_global(c2);
        if (c1 != c2) {
            if (IS_TRUE(islisp_subclassp(c1, c2))) return true;
            if (IS_TRUE(islisp_subclassp(c2, c1))) return false;
        }
        s1 = CDR(s1);
        s2 = CDR(s2);
    }
    return false;
}

static islisp_method_t* sort_methods(islisp_method_t *head) {
    if (!head || !head->next) return head;
    islisp_method_t *sorted = NULL;
    while (head) {
        islisp_method_t *curr = head;
        head = head->next;
        if (!sorted || method_more_specific(curr, sorted)) {
            curr->next = sorted;
            sorted = curr;
        } else {
            islisp_method_t *p = sorted;
            while (p->next && !method_more_specific(curr, p->next)) {
                p = p->next;
            }
            curr->next = p->next;
            p->next = curr;
        }
    }
    return sorted;
}

static islisp_method_t* reverse_methods(islisp_method_t *head) {
    islisp_method_t *prev = NULL;
    while (head) {
        islisp_method_t *next = head->next;
        head->next = prev;
        prev = head;
        head = next;
    }
    return prev;
}

static islisp_val gf_dispatch(islisp_val env, int argc, islisp_val *argv) {
    islisp_generic_function_t *gf = (islisp_generic_function_t*)env;
    islisp_method_t *m = gf->methods;

    /* Collect applicable primary and around methods */
    islisp_method_t *applicable_primaries = NULL;
    islisp_method_t **prim_tail = &applicable_primaries;
    islisp_method_t *applicable_befores = NULL;
    islisp_method_t **bef_tail = &applicable_befores;
    islisp_method_t *applicable_afters = NULL;
    islisp_method_t **aft_tail = &applicable_afters;
    islisp_method_t *applicable_arounds = NULL;
    islisp_method_t **ard_tail = &applicable_arounds;

    while (m) {
        bool matches = true;
        islisp_val spec_list = m->specializers;
        int arg_i = 0;
        while (IS_CONS(spec_list) && arg_i < argc) {
            islisp_val spec_class = CAR(spec_list);
            if (!IS_TRUE(islisp_instancep(argv[arg_i], spec_class))) {
                matches = false;
                break;
            }
            spec_list = CDR(spec_list);
            arg_i++;
        }
        if (matches) {
            islisp_method_t *copy = (islisp_method_t*)islisp_alloc(sizeof(islisp_method_t));
            memcpy(copy, m, sizeof(islisp_method_t));
            copy->header.type = TYPE_METHOD;
            copy->next = NULL;
            if (m->qualifier == METHOD_PRIMARY) {
                *prim_tail = copy;
                prim_tail = &copy->next;
            } else if (m->qualifier == METHOD_BEFORE) {
                *bef_tail = copy;
                bef_tail = &copy->next;
            } else if (m->qualifier == METHOD_AFTER) {
                *aft_tail = copy;
                aft_tail = &copy->next;
            } else if (m->qualifier == METHOD_AROUND) {
                *ard_tail = copy;
                ard_tail = &copy->next;
            }
        }
        m = m->next;
    }

    applicable_primaries = sort_methods(applicable_primaries);
    applicable_befores = sort_methods(applicable_befores);
    applicable_arounds = sort_methods(applicable_arounds);
    applicable_afters = reverse_methods(sort_methods(applicable_afters));

    if (!applicable_primaries && !applicable_arounds) {
        islisp_error("No applicable method found for generic function %s", IS_SYMBOL(gf->name) ? ((islisp_symbol_t*)gf->name)->name : "?");
    }

    islisp_method_context_t ctx;
    ctx.prev = g_method_ctx;
    ctx.next_primary = applicable_primaries;
    ctx.next_around = applicable_arounds;
    ctx.argc = argc;
    ctx.argv = argv;
    g_method_ctx = &ctx;

    /* Execute befores */
    islisp_method_t *b = applicable_befores;
    while (b) {
        b->closure->fn(b->closure->env, argc, argv);
        b = b->next;
    }

    islisp_val res = ISLISP_NIL;
    if (applicable_arounds) {
        islisp_method_t *cur_ard = ctx.next_around;
        ctx.next_around = cur_ard->next;
        res = cur_ard->closure->fn(cur_ard->closure->env, argc, argv);
    } else if (applicable_primaries) {
        islisp_method_t *cur_prim = ctx.next_primary;
        ctx.next_primary = cur_prim->next;
        res = cur_prim->closure->fn(cur_prim->closure->env, argc, argv);
    }

    /* Execute afters */
    islisp_method_t *a = applicable_afters;
    while (a) {
        a->closure->fn(a->closure->env, argc, argv);
        a = a->next;
    }

    g_method_ctx = ctx.prev;
    return res;
}

islisp_val islisp_defgeneric(islisp_val name, int num_req, bool has_rest) {
    islisp_generic_function_t *gf = (islisp_generic_function_t*)islisp_alloc(sizeof(islisp_generic_function_t));
    gf->header.type = TYPE_GENERIC_FUNCTION;
    gf->header.flags = 0;
    gf->header.extra = 0;
    gf->header.len = 0;
    gf->name = name;
    gf->num_required = num_req;
    gf->has_rest = has_rest;
    gf->methods = NULL;

    islisp_val cl = islisp_make_closure(gf_dispatch, TO_HEAP(gf), num_req, has_rest ? -1 : num_req, IS_SYMBOL(name) ? ((islisp_symbol_t*)name)->name : "generic");
    islisp_set_function(name, cl);
    return name;
}

islisp_val islisp_defmethod(islisp_val gf_name, islisp_method_qualifier_t qual, islisp_val specializers, bool has_rest, islisp_closure_t *cl) {
    islisp_val fn_val = islisp_get_function(gf_name);
    if (!IS_CLOSURE(fn_val)) {
        /* Auto-define generic function if not defined yet */
        int num_req = (int)AS_INT(islisp_length(specializers));
        islisp_defgeneric(gf_name, num_req, has_rest);
        fn_val = islisp_get_function(gf_name);
    }
    islisp_closure_t *gf_cl = (islisp_closure_t*)fn_val;
    islisp_generic_function_t *gf = (islisp_generic_function_t*)gf_cl->env;

    islisp_method_t *m = (islisp_method_t*)islisp_alloc(sizeof(islisp_method_t));
    m->header.type = TYPE_METHOD;
    m->qualifier = qual;
    m->specializers = specializers;
    m->has_rest = has_rest;
    m->closure = cl;
    m->next = gf->methods;
    gf->methods = m;
    return gf_name;
}

islisp_val islisp_call_next_method(void) {
    if (!g_method_ctx) islisp_error("call-next-method called outside of method context");
    if (g_method_ctx->next_around) {
        islisp_method_t *cur = g_method_ctx->next_around;
        g_method_ctx->next_around = cur->next;
        return cur->closure->fn(cur->closure->env, g_method_ctx->argc, g_method_ctx->argv);
    }
    if (g_method_ctx->next_primary) {
        islisp_method_t *cur = g_method_ctx->next_primary;
        g_method_ctx->next_primary = cur->next;
        return cur->closure->fn(cur->closure->env, g_method_ctx->argc, g_method_ctx->argv);
    }
    islisp_error("call-next-method: no next method available");
    return ISLISP_NIL;
}

islisp_val islisp_next_method_p(void) {
    if (!g_method_ctx) return ISLISP_NIL;
    return BOOL_VAL(g_method_ctx->next_around != NULL || g_method_ctx->next_primary != NULL);
}
