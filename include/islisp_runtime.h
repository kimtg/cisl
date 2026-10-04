#ifndef ISLISP_RUNTIME_H
#define ISLISP_RUNTIME_H

#include "islisp.h"

/* Structure definitions */

typedef struct islisp_float {
    islisp_header_t header;
    double val;
} islisp_float_t;

typedef struct islisp_string {
    islisp_header_t header;
    char *data;
} islisp_string_t;

typedef struct islisp_symbol {
    islisp_header_t header;
    char *name;
    islisp_val global_val;
    islisp_val func_val;
    islisp_val dynamic_val;
    islisp_val plist;
    bool is_constant;
    struct islisp_symbol *next;
} islisp_symbol_t;

typedef struct islisp_vector {
    islisp_header_t header;
    islisp_val *data;
} islisp_vector_t;

typedef struct islisp_array {
    islisp_header_t header;
    int ndims;
    int *dims;
    islisp_val *data;
} islisp_array_t;

typedef islisp_val (*islisp_fn_t)(islisp_val env, int argc, islisp_val *args);

typedef struct islisp_closure {
    islisp_header_t header;
    islisp_fn_t fn;
    islisp_val env;
    int min_args;
    int max_args; /* -1 means rest parameter */
    char *name;
} islisp_closure_t;

typedef enum {
    STREAM_FILE,
    STREAM_STRING_IN,
    STREAM_STRING_OUT
} islisp_stream_kind_t;

typedef struct islisp_stream {
    islisp_header_t header;
    islisp_stream_kind_t kind;
    FILE *file;
    char *str_buf;
    size_t str_cap;
    size_t str_len;
    size_t str_pos;
    bool is_input;
    bool is_output;
    bool is_open;
    int unread_buf[16];
    int unread_count;
} islisp_stream_t;

typedef struct islisp_class {
    islisp_header_t header;
    islisp_val name;             /* symbol */
    islisp_val cpl;              /* list of classes */
    islisp_val direct_supers;    /* list of classes */
    islisp_val direct_subclasses;/* list of classes */
    islisp_val slots;            /* list of slot descriptors */
    bool is_abstract;
} islisp_class_t;

typedef struct islisp_instance {
    islisp_header_t header;
    islisp_val class_obj;
    int num_slots;
    islisp_val *slots;
} islisp_instance_t;

typedef enum {
    METHOD_PRIMARY = 0,
    METHOD_BEFORE,
    METHOD_AFTER,
    METHOD_AROUND
} islisp_method_qualifier_t;

typedef struct islisp_method {
    islisp_header_t header;
    islisp_method_qualifier_t qualifier;
    islisp_val specializers; /* list of classes */
    bool has_rest;
    islisp_closure_t *closure;
    struct islisp_method *next;
} islisp_method_t;

typedef struct islisp_generic_function {
    islisp_header_t header;
    islisp_val name;
    int num_required;
    bool has_rest;
    islisp_method_t *methods;
} islisp_generic_function_t;

typedef struct islisp_condition {
    islisp_header_t header;
    islisp_val class_obj;
    char *message;
    islisp_val irritants;
    bool continuable;
    islisp_val extra_data;
} islisp_condition_t;

typedef enum {
    FRAME_CATCH = 1,
    FRAME_BLOCK,
    FRAME_UNWIND,
    FRAME_DYNAMIC,
    FRAME_HANDLER
} islisp_frame_kind_t;

typedef struct islisp_frame {
    islisp_frame_kind_t kind;
    struct islisp_frame *prev;
    jmp_buf jmp;
    islisp_val tag;       /* catch tag or block name symbol */
    islisp_val val;       /* return value / old dynamic val */
    islisp_val var;       /* dynamic variable symbol or handler function */
    void (*cleanup_fn)(void *arg);
    void *cleanup_arg;
} islisp_frame_t;

/* Global runtime variables */
extern islisp_val SYM_T;
extern islisp_val SYM_NIL;
extern islisp_val SYM_REST;
extern islisp_val SYM_COLON_REST;
extern islisp_frame_t *g_top_frame;
extern islisp_val g_last_condition;

/* Predefined Class Objects */
extern islisp_val CLASS_OBJECT;
extern islisp_val CLASS_STANDARD_OBJECT;
extern islisp_val CLASS_NUMBER;
extern islisp_val CLASS_INTEGER;
extern islisp_val CLASS_FLOAT;
extern islisp_val CLASS_CHARACTER;
extern islisp_val CLASS_SYMBOL;
extern islisp_val CLASS_LIST;
extern islisp_val CLASS_CONS;
extern islisp_val CLASS_NULL;
extern islisp_val CLASS_BASIC_ARRAY;
extern islisp_val CLASS_BASIC_ARRAY_STAR;
extern islisp_val CLASS_GENERAL_ARRAY_STAR;
extern islisp_val CLASS_BASIC_VECTOR;
extern islisp_val CLASS_GENERAL_VECTOR;
extern islisp_val CLASS_STRING;
extern islisp_val CLASS_STREAM;
extern islisp_val CLASS_FUNCTION;
extern islisp_val CLASS_GENERIC_FUNCTION;
extern islisp_val CLASS_STANDARD_GENERIC_FUNCTION;
extern islisp_val CLASS_SERIOUS_CONDITION;
extern islisp_val CLASS_ERROR;
extern islisp_val CLASS_ARITHMETIC_ERROR;
extern islisp_val CLASS_DIVISION_BY_ZERO;
extern islisp_val CLASS_FLOATING_POINT_OVERFLOW;
extern islisp_val CLASS_FLOATING_POINT_UNDERFLOW;
extern islisp_val CLASS_CONTROL_ERROR;
extern islisp_val CLASS_DOMAIN_ERROR;
extern islisp_val CLASS_PARSE_ERROR;
extern islisp_val CLASS_PROGRAM_ERROR;
extern islisp_val CLASS_SIMPLE_ERROR;
extern islisp_val CLASS_STREAM_ERROR;
extern islisp_val CLASS_END_OF_STREAM;
extern islisp_val CLASS_STORAGE_EXHAUSTED;
extern islisp_val CLASS_UNDEFINED_ENTITY;
extern islisp_val CLASS_UNBOUND_VARIABLE;
extern islisp_val CLASS_UNDEFINED_FUNCTION;

/* Memory & Object Creation */
void islisp_init_runtime(void);
void islisp_register_builtins(void);
void* islisp_alloc(size_t size);
void* islisp_alloc_cons(void);
islisp_val islisp_make_float(double d);
islisp_val islisp_make_string(const char *s);
islisp_val islisp_make_string_len(const char *s, size_t len);
islisp_val islisp_make_vector(int len, islisp_val init);
islisp_val islisp_make_closure(islisp_fn_t fn, islisp_val env, int min_args, int max_args, const char *name);
islisp_val islisp_cons(islisp_val a, islisp_val b);

/* Garbage Collection API */
typedef struct {
    size_t bytes_allocated;
    size_t total_allocated;
    size_t total_objects;
    size_t collections;
    size_t freed_objects;
    size_t freed_bytes;
    size_t threshold;
} islisp_gc_stats_t;

void islisp_gc_init(void *stack_bottom);
void islisp_gc_collect(void);
void islisp_gc_enable(void);
void islisp_gc_disable(void);
void islisp_gc_set_threshold(size_t bytes);
void islisp_gc_get_stats(islisp_gc_stats_t *out_stats);
void islisp_gc_mark_val(islisp_val v);
void islisp_gc_mark_object(void *ptr);
void islisp_gc_register_root(islisp_val *ptr);
void islisp_gc_unregister_root(islisp_val *ptr);
void islisp_gc_register_root_array(islisp_val *arr, size_t count);
void islisp_gc_unregister_root_array(islisp_val *arr);
void islisp_gc_mark_method_ctx(void);
void islisp_gc_mark_global_classes(void);

/* Symbol Table */
islisp_val islisp_intern(const char *name);
islisp_val islisp_gensym(void);
islisp_val islisp_get_global(islisp_val sym);
void islisp_set_global(islisp_val sym, islisp_val val);
islisp_val islisp_get_function(islisp_val sym);
void islisp_set_function(islisp_val sym, islisp_val val);
islisp_val islisp_get_dynamic(islisp_val sym);
void islisp_set_dynamic(islisp_val sym, islisp_val val);
islisp_val islisp_property(islisp_val sym, islisp_val prop, islisp_val default_val);
islisp_val islisp_set_property(islisp_val val, islisp_val sym, islisp_val prop);
islisp_val islisp_remove_property(islisp_val sym, islisp_val prop);
islisp_val islisp_symbol_name(islisp_val sym);

/* Function Invocation */
islisp_val islisp_funcall(islisp_val fn, int argc, ...);
islisp_val islisp_funcall_argv(islisp_val fn, int argc, islisp_val *argv);
islisp_val islisp_apply(islisp_val fn, int argc, islisp_val *argv);
islisp_val islisp_identity(islisp_val obj);

/* Non-local exits & Conditions */
void islisp_push_frame(islisp_frame_t *frame);
void islisp_pop_frame(islisp_frame_t *frame);
void islisp_throw(islisp_val tag, islisp_val val);
void islisp_return_from(islisp_val name, islisp_val val);
void islisp_continue_unwind(void);
islisp_val islisp_signal_condition(islisp_val condition, bool continuable);
islisp_val islisp_error(const char *fmt, ...);
islisp_val islisp_cerror(const char *cont_fmt, const char *fmt, ...);
islisp_val islisp_continue_condition(islisp_val condition, islisp_val value);
islisp_val islisp_condition_continuable(islisp_val condition);
islisp_val islisp_make_condition(islisp_val class_obj, const char *msg, islisp_val irritants, bool continuable);

/* Condition Accessors */
islisp_val islisp_arithmetic_error_operation(islisp_val condition);
islisp_val islisp_arithmetic_error_operands(islisp_val condition);
islisp_val islisp_domain_error_object(islisp_val condition);
islisp_val islisp_domain_error_expected_class(islisp_val condition);
islisp_val islisp_parse_error_string(islisp_val condition);
islisp_val islisp_parse_error_expected_class(islisp_val condition);
islisp_val islisp_simple_error_format_string(islisp_val condition);
islisp_val islisp_simple_error_format_arguments(islisp_val condition);
islisp_val islisp_stream_error_stream(islisp_val condition);
islisp_val islisp_undefined_entity_name(islisp_val condition);
islisp_val islisp_undefined_entity_namespace(islisp_val condition);

/* Arithmetic & Math */
islisp_val islisp_add(int argc, islisp_val *argv);
islisp_val islisp_sub(int argc, islisp_val *argv);
islisp_val islisp_mul(int argc, islisp_val *argv);
islisp_val islisp_div(int argc, islisp_val *argv);
islisp_val islisp_quotient(islisp_val dividend, islisp_val divisor);
islisp_val islisp_remainder(islisp_val dividend, islisp_val divisor);
islisp_val islisp_num_div(islisp_val z1, islisp_val z2);
islisp_val islisp_num_mod(islisp_val z1, islisp_val z2);
islisp_val islisp_abs(islisp_val x);
islisp_val islisp_min(int argc, islisp_val *argv);
islisp_val islisp_max(int argc, islisp_val *argv);
islisp_val islisp_reciprocal(islisp_val x);
islisp_val islisp_exp(islisp_val x);
islisp_val islisp_log(islisp_val x);
islisp_val islisp_expt(islisp_val x1, islisp_val x2);
islisp_val islisp_sqrt(islisp_val x);
islisp_val islisp_sin(islisp_val x);
islisp_val islisp_cos(islisp_val x);
islisp_val islisp_tan(islisp_val x);
islisp_val islisp_atan(islisp_val x);
islisp_val islisp_atan2(islisp_val y, islisp_val x);
islisp_val islisp_sinh(islisp_val x);
islisp_val islisp_cosh(islisp_val x);
islisp_val islisp_tanh(islisp_val x);
islisp_val islisp_atanh(islisp_val x);
islisp_val islisp_floor(islisp_val x);
islisp_val islisp_ceiling(islisp_val x);
islisp_val islisp_truncate(islisp_val x);
islisp_val islisp_round(islisp_val x);
islisp_val islisp_gcd(islisp_val z1, islisp_val z2);
islisp_val islisp_lcm(islisp_val z1, islisp_val z2);
islisp_val islisp_isqrt(islisp_val z);
islisp_val islisp_float(islisp_val x);
islisp_val islisp_parse_number(islisp_val str);

/* Arithmetic Comparisons */
islisp_val islisp_num_eq(islisp_val a, islisp_val b);
islisp_val islisp_num_neq(islisp_val a, islisp_val b);
islisp_val islisp_num_lt(islisp_val a, islisp_val b);
islisp_val islisp_num_lteq(islisp_val a, islisp_val b);
islisp_val islisp_num_gt(islisp_val a, islisp_val b);
islisp_val islisp_num_gteq(islisp_val a, islisp_val b);

/* Predicates */
islisp_val islisp_eq(islisp_val a, islisp_val b);
islisp_val islisp_eql(islisp_val a, islisp_val b);
islisp_val islisp_equal(islisp_val a, islisp_val b);
islisp_val islisp_not(islisp_val x);
islisp_val islisp_null(islisp_val x);
islisp_val islisp_symbolp(islisp_val x);
islisp_val islisp_numberp(islisp_val x);
islisp_val islisp_integerp(islisp_val x);
islisp_val islisp_floatp(islisp_val x);
islisp_val islisp_characterp(islisp_val x);
islisp_val islisp_consp(islisp_val x);
islisp_val islisp_listp(islisp_val x);
islisp_val islisp_basic_vector_p(islisp_val x);
islisp_val islisp_general_vector_p(islisp_val x);
islisp_val islisp_stringp(islisp_val x);
islisp_val islisp_streamp(islisp_val x);
islisp_val islisp_functionp(islisp_val x);
islisp_val islisp_generic_function_p(islisp_val x);
islisp_val islisp_instancep(islisp_val obj, islisp_val class_obj);
islisp_val islisp_subclassp(islisp_val c1, islisp_val c2);
islisp_val islisp_class_of(islisp_val obj);

/* Conses & Lists */
islisp_val islisp_car(islisp_val c);
islisp_val islisp_cdr(islisp_val c);
islisp_val islisp_set_car(islisp_val val, islisp_val c);
islisp_val islisp_set_cdr(islisp_val val, islisp_val c);
islisp_val islisp_create_list(islisp_val len, islisp_val init);
islisp_val islisp_list(int argc, islisp_val *argv);
islisp_val islisp_reverse(islisp_val list);
islisp_val islisp_nreverse(islisp_val list);
islisp_val islisp_append(int argc, islisp_val *argv);
islisp_val islisp_member(islisp_val obj, islisp_val list);
islisp_val islisp_assoc(islisp_val obj, islisp_val alist);
islisp_val islisp_length(islisp_val seq);
islisp_val islisp_mapcar(int argc, islisp_val *argv);
islisp_val islisp_mapc(int argc, islisp_val *argv);
islisp_val islisp_mapcan(int argc, islisp_val *argv);
islisp_val islisp_maplist(int argc, islisp_val *argv);
islisp_val islisp_mapl(int argc, islisp_val *argv);
islisp_val islisp_mapcon(int argc, islisp_val *argv);

/* Sequences, Vectors, Strings */
islisp_val islisp_elt(islisp_val seq, islisp_val idx);
islisp_val islisp_set_elt(islisp_val val, islisp_val seq, islisp_val idx);
islisp_val islisp_subseq(islisp_val seq, islisp_val start, islisp_val end);
islisp_val islisp_map_into(int argc, islisp_val *argv);
islisp_val islisp_create_vector(islisp_val len, islisp_val init);
islisp_val islisp_vector(int argc, islisp_val *argv);
islisp_val islisp_create_string(islisp_val len, islisp_val init_ch);
islisp_val islisp_string_eq(islisp_val s1, islisp_val s2);
islisp_val islisp_string_neq(islisp_val s1, islisp_val s2);
islisp_val islisp_string_lt(islisp_val s1, islisp_val s2);
islisp_val islisp_string_lteq(islisp_val s1, islisp_val s2);
islisp_val islisp_string_gt(islisp_val s1, islisp_val s2);
islisp_val islisp_string_gteq(islisp_val s1, islisp_val s2);
islisp_val islisp_char_index(islisp_val ch, islisp_val str, islisp_val start);
islisp_val islisp_string_index(islisp_val sub, islisp_val str, islisp_val start);
islisp_val islisp_string_append(int argc, islisp_val *argv);

/* Character Operations */
islisp_val islisp_char_eq(islisp_val c1, islisp_val c2);
islisp_val islisp_char_neq(islisp_val c1, islisp_val c2);
islisp_val islisp_char_lt(islisp_val c1, islisp_val c2);
islisp_val islisp_char_lteq(islisp_val c1, islisp_val c2);
islisp_val islisp_char_gt(islisp_val c1, islisp_val c2);
islisp_val islisp_char_gteq(islisp_val c1, islisp_val c2);

/* Arrays */
islisp_val islisp_create_array(islisp_val dims, islisp_val init);
islisp_val islisp_aref(int argc, islisp_val *argv);
islisp_val islisp_garef(int argc, islisp_val *argv);
islisp_val islisp_set_aref(int argc, islisp_val *argv);
islisp_val islisp_set_garef(int argc, islisp_val *argv);
islisp_val islisp_array_dimensions(islisp_val arr);
islisp_val islisp_basic_array_p(islisp_val arr);
islisp_val islisp_basic_array_s_p(islisp_val arr);
islisp_val islisp_general_array_s_p(islisp_val arr);

/* Type Coercion */
islisp_val islisp_convert(islisp_val obj, islisp_val target_class);

/* Streams & I/O */
islisp_val islisp_standard_input(void);
islisp_val islisp_standard_output(void);
islisp_val islisp_error_output(void);
islisp_val islisp_open_input_file(islisp_val filename, islisp_val elem_class);
islisp_val islisp_open_output_file(islisp_val filename, islisp_val elem_class);
islisp_val islisp_open_io_file(islisp_val filename, islisp_val elem_class);
islisp_val islisp_close(islisp_val stream);
islisp_val islisp_finish_output(islisp_val stream);
islisp_val islisp_create_string_input_stream(islisp_val str);
islisp_val islisp_create_string_output_stream(void);
islisp_val islisp_get_output_stream_string(islisp_val stream);
islisp_val islisp_read(islisp_val stream, islisp_val eos_err, islisp_val eos_val);
islisp_val islisp_read_char(islisp_val stream, islisp_val eos_err, islisp_val eos_val);
islisp_val islisp_preview_char(islisp_val stream, islisp_val eos_err, islisp_val eos_val);
islisp_val islisp_read_line(islisp_val stream, islisp_val eos_err, islisp_val eos_val);
islisp_val islisp_read_byte(islisp_val stream, islisp_val eos_err, islisp_val eos_val);
islisp_val islisp_write_byte(islisp_val byte_val, islisp_val stream);
islisp_val islisp_format(islisp_val stream, islisp_val fmt_str, int argc, islisp_val *argv);
islisp_val islisp_format_char(islisp_val stream, islisp_val ch);
islisp_val islisp_format_float(islisp_val stream, islisp_val f);
islisp_val islisp_format_fresh_line(islisp_val stream);
islisp_val islisp_format_integer(islisp_val stream, islisp_val n, islisp_val radix);
islisp_val islisp_format_object(islisp_val stream, islisp_val obj, islisp_val escape_p);
islisp_val islisp_format_tab(islisp_val stream, islisp_val col);
islisp_val islisp_stream_ready_p(islisp_val stream);
islisp_val islisp_open_stream_p(islisp_val stream);
islisp_val islisp_input_stream_p(islisp_val stream);
islisp_val islisp_output_stream_p(islisp_val stream);
islisp_val islisp_probe_file(islisp_val filename);
islisp_val islisp_file_position(islisp_val stream);
islisp_val islisp_set_file_position(islisp_val stream, islisp_val pos);
islisp_val islisp_file_length(islisp_val filename, islisp_val elem_class);

/* Time */
islisp_val islisp_get_universal_time(void);
islisp_val islisp_get_internal_run_time(void);
islisp_val islisp_get_internal_real_time(void);
islisp_val islisp_internal_time_units_per_second(void);

/* ILOS (ISLisp Object System) */
islisp_val islisp_defclass(islisp_val name, islisp_val supers, islisp_val slot_specs, bool is_abstract);
islisp_val islisp_defgeneric(islisp_val name, int num_req, bool has_rest);
islisp_val islisp_defmethod(islisp_val gf_name, islisp_method_qualifier_t qual, islisp_val specializers, bool has_rest, islisp_closure_t *fn);
islisp_val islisp_create(islisp_val class_obj, int argc, islisp_val *argv);
islisp_val islisp_initialize_object(islisp_val instance, islisp_val initargs);
islisp_val islisp_call_next_method(void);
islisp_val islisp_next_method_p(void);
islisp_val islisp_slot_value(islisp_val instance, islisp_val slot_name);
islisp_val islisp_set_slot_value(islisp_val val, islisp_val instance, islisp_val slot_name);
islisp_val islisp_slot_boundp(islisp_val instance, islisp_val slot_name);

/* Fast inlined arithmetic, comparisons, and predicates */
static inline islisp_val islisp_fast_add(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return TO_INT(AS_INT(a) + AS_INT(b));
    }
    islisp_val argv[2] = {a, b};
    return islisp_add(2, argv);
}

static inline islisp_val islisp_fast_sub(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return TO_INT(AS_INT(a) - AS_INT(b));
    }
    islisp_val argv[2] = {a, b};
    return islisp_sub(2, argv);
}

static inline islisp_val islisp_fast_mul(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return TO_INT(AS_INT(a) * AS_INT(b));
    }
    islisp_val argv[2] = {a, b};
    return islisp_mul(2, argv);
}

static inline islisp_val islisp_fast_div(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        int64_t ib = AS_INT(b);
        if (__builtin_expect(ib != 0, 1)) {
            int64_t ia = AS_INT(a);
            if (ia % ib == 0) return TO_INT(ia / ib);
        }
    }
    return islisp_quotient(a, b);
}

static inline islisp_val islisp_fast_neg(islisp_val a) {
    if (__builtin_expect(IS_INT(a), 1)) {
        return TO_INT(-AS_INT(a));
    }
    islisp_val argv[1] = {a};
    return islisp_sub(1, argv);
}

static inline islisp_val islisp_fast_lt(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return (AS_INT(a) < AS_INT(b)) ? ISLISP_T : ISLISP_NIL;
    }
    return islisp_num_lt(a, b);
}

static inline islisp_val islisp_fast_lteq(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return (AS_INT(a) <= AS_INT(b)) ? ISLISP_T : ISLISP_NIL;
    }
    return islisp_num_lteq(a, b);
}

static inline islisp_val islisp_fast_gt(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return (AS_INT(a) > AS_INT(b)) ? ISLISP_T : ISLISP_NIL;
    }
    return islisp_num_gt(a, b);
}

static inline islisp_val islisp_fast_gteq(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return (AS_INT(a) >= AS_INT(b)) ? ISLISP_T : ISLISP_NIL;
    }
    return islisp_num_gteq(a, b);
}

static inline islisp_val islisp_fast_eq(islisp_val a, islisp_val b) {
    return (a == b) ? ISLISP_T : ISLISP_NIL;
}

static inline islisp_val islisp_fast_num_eq(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return (a == b) ? ISLISP_T : ISLISP_NIL;
    }
    return islisp_num_eq(a, b);
}

static inline islisp_val islisp_fast_num_neq(islisp_val a, islisp_val b) {
    if (__builtin_expect((a & b & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT, 1)) {
        return (a != b) ? ISLISP_T : ISLISP_NIL;
    }
    return islisp_num_neq(a, b);
}

static inline islisp_val islisp_fast_car(islisp_val c) {
    if (__builtin_expect(IS_CONS(c), 1)) {
        return CAR(c);
    }
    return islisp_car(c);
}

static inline islisp_val islisp_fast_cdr(islisp_val c) {
    if (__builtin_expect(IS_CONS(c), 1)) {
        return CDR(c);
    }
    return islisp_cdr(c);
}

static inline islisp_val islisp_fast_not(islisp_val v) {
    return (v == ISLISP_NIL) ? ISLISP_T : ISLISP_NIL;
}

static inline islisp_val islisp_fast_consp(islisp_val v) {
    return IS_CONS(v) ? ISLISP_T : ISLISP_NIL;
}

static inline islisp_val islisp_fast_integerp(islisp_val v) {
    return IS_INT(v) ? ISLISP_T : ISLISP_NIL;
}

#endif /* ISLISP_RUNTIME_H */
