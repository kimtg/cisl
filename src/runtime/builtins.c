#include "islisp_runtime.h"

static void register_builtin(const char *name, islisp_fn_t fn, int min_args, int max_args) {
    islisp_val sym = islisp_intern(name);
    islisp_val cl = islisp_make_closure(fn, ISLISP_NIL, min_args, max_args, name);
    islisp_set_function(sym, cl);
}

/* Wrapper helpers */
#define WRAP_0(fn_name, call_expr) \
    static islisp_val c_##fn_name(islisp_val env, int argc, islisp_val *argv) { \
        (void)env; (void)argc; (void)argv; return call_expr; \
    }

#define WRAP_1(fn_name, call_expr) \
    static islisp_val c_##fn_name(islisp_val env, int argc, islisp_val *argv) { \
        (void)env; (void)argc; return call_expr(argv[0]); \
    }

#define WRAP_2(fn_name, call_expr) \
    static islisp_val c_##fn_name(islisp_val env, int argc, islisp_val *argv) { \
        (void)env; (void)argc; return call_expr(argv[0], argv[1]); \
    }

#define WRAP_3(fn_name, call_expr) \
    static islisp_val c_##fn_name(islisp_val env, int argc, islisp_val *argv) { \
        (void)env; (void)argc; return call_expr(argv[0], argv[1], argv[2]); \
    }

#define WRAP_N(fn_name, call_expr) \
    static islisp_val c_##fn_name(islisp_val env, int argc, islisp_val *argv) { \
        (void)env; return call_expr(argc, argv); \
    }

/* Math wrappers */
WRAP_N(add, islisp_add)
WRAP_N(sub, islisp_sub)
WRAP_N(mul, islisp_mul)
WRAP_N(div, islisp_div)
WRAP_2(quotient, islisp_quotient)
WRAP_2(remainder, islisp_remainder)
WRAP_2(num_div, islisp_num_div)
WRAP_2(num_mod, islisp_num_mod)
WRAP_1(abs, islisp_abs)
WRAP_N(min, islisp_min)
WRAP_N(max, islisp_max)
WRAP_1(reciprocal, islisp_reciprocal)
WRAP_1(exp, islisp_exp)
WRAP_1(log, islisp_log)
WRAP_2(expt, islisp_expt)
WRAP_1(sqrt, islisp_sqrt)
WRAP_1(sin, islisp_sin)
WRAP_1(cos, islisp_cos)
WRAP_1(tan, islisp_tan)
WRAP_1(atan, islisp_atan)
WRAP_2(atan2, islisp_atan2)
WRAP_1(sinh, islisp_sinh)
WRAP_1(cosh, islisp_cosh)
WRAP_1(tanh, islisp_tanh)
WRAP_1(atanh, islisp_atanh)
WRAP_1(floor, islisp_floor)
WRAP_1(ceiling, islisp_ceiling)
WRAP_1(truncate, islisp_truncate)
WRAP_1(round, islisp_round)
WRAP_2(gcd, islisp_gcd)
WRAP_2(lcm, islisp_lcm)
WRAP_1(isqrt, islisp_isqrt)
WRAP_1(float, islisp_float)
WRAP_1(parse_number, islisp_parse_number)

WRAP_2(num_eq, islisp_num_eq)
WRAP_2(num_neq, islisp_num_neq)
WRAP_2(num_lt, islisp_num_lt)
WRAP_2(num_lteq, islisp_num_lteq)
WRAP_2(num_gt, islisp_num_gt)
WRAP_2(num_gteq, islisp_num_gteq)

/* Predicate wrappers */
WRAP_2(eq, islisp_eq)
WRAP_2(eql, islisp_eql)
WRAP_2(equal, islisp_equal)
WRAP_1(not, islisp_not)
WRAP_1(null, islisp_null)
WRAP_1(symbolp, islisp_symbolp)
WRAP_1(numberp, islisp_numberp)
WRAP_1(integerp, islisp_integerp)
WRAP_1(floatp, islisp_floatp)
WRAP_1(characterp, islisp_characterp)
WRAP_1(consp, islisp_consp)
WRAP_1(listp, islisp_listp)
WRAP_1(basic_vector_p, islisp_basic_vector_p)
WRAP_1(general_vector_p, islisp_general_vector_p)
WRAP_1(stringp, islisp_stringp)
WRAP_1(streamp, islisp_streamp)
WRAP_1(functionp, islisp_functionp)
WRAP_1(generic_function_p, islisp_generic_function_p)
WRAP_2(instancep, islisp_instancep)
WRAP_2(subclassp, islisp_subclassp)
WRAP_1(class_of, islisp_class_of)

/* List wrappers */
WRAP_2(cons, islisp_cons)
WRAP_1(car, islisp_car)
WRAP_1(cdr, islisp_cdr)
WRAP_2(set_car, islisp_set_car)
WRAP_2(set_cdr, islisp_set_cdr)
static islisp_val c_create_list(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_create_list(argv[0], argc > 1 ? argv[1] : ISLISP_NIL);
}
WRAP_N(list, islisp_list)
WRAP_1(reverse, islisp_reverse)
WRAP_1(nreverse, islisp_nreverse)
WRAP_N(append, islisp_append)
WRAP_2(member, islisp_member)
WRAP_2(assoc, islisp_assoc)
WRAP_1(length, islisp_length)
WRAP_N(mapcar, islisp_mapcar)
WRAP_N(mapc, islisp_mapc)
WRAP_N(mapcan, islisp_mapcan)
WRAP_N(maplist, islisp_maplist)
WRAP_N(mapl, islisp_mapl)
WRAP_N(mapcon, islisp_mapcon)

/* Sequence, Vector, String wrappers */
WRAP_2(elt, islisp_elt)
WRAP_3(set_elt, islisp_set_elt)
WRAP_3(subseq, islisp_subseq)
WRAP_N(map_into, islisp_map_into)
static islisp_val c_create_vector(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_create_vector(argv[0], argc > 1 ? argv[1] : ISLISP_UNBOUND);
}
WRAP_N(vector, islisp_vector)
static islisp_val c_create_string(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_create_string(argv[0], argc > 1 ? argv[1] : ISLISP_UNBOUND);
}
WRAP_2(string_eq, islisp_string_eq)
WRAP_2(string_neq, islisp_string_neq)
WRAP_2(string_lt, islisp_string_lt)
WRAP_2(string_lteq, islisp_string_lteq)
WRAP_2(string_gt, islisp_string_gt)
WRAP_2(string_gteq, islisp_string_gteq)
static islisp_val c_char_index(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_char_index(argv[0], argv[1], argc > 2 ? argv[2] : ISLISP_UNBOUND);
}
static islisp_val c_string_index(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_string_index(argv[0], argv[1], argc > 2 ? argv[2] : ISLISP_UNBOUND);
}
WRAP_N(string_append, islisp_string_append)

/* Character wrappers */
WRAP_2(char_eq, islisp_char_eq)
WRAP_2(char_neq, islisp_char_neq)
WRAP_2(char_lt, islisp_char_lt)
WRAP_2(char_lteq, islisp_char_lteq)
WRAP_2(char_gt, islisp_char_gt)
WRAP_2(char_gteq, islisp_char_gteq)

/* Array wrappers */
static islisp_val c_create_array(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_create_array(argv[0], argc > 1 ? argv[1] : ISLISP_UNBOUND);
}
WRAP_N(aref, islisp_aref)
WRAP_N(garef, islisp_garef)
WRAP_N(set_aref, islisp_set_aref)
WRAP_N(set_garef, islisp_set_garef)
WRAP_1(array_dimensions, islisp_array_dimensions)
WRAP_1(basic_array_p, islisp_basic_array_p)
WRAP_1(basic_array_s_p, islisp_basic_array_s_p)
WRAP_1(general_array_s_p, islisp_general_array_s_p)

/* Coercion */
WRAP_2(convert, islisp_convert)

/* Symbols */
WRAP_0(gensym, islisp_gensym())
WRAP_1(symbol_name, islisp_symbol_name)
static islisp_val c_property(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_property(argv[0], argv[1], argc > 2 ? argv[2] : ISLISP_NIL);
}
WRAP_3(set_property, islisp_set_property)
WRAP_2(remove_property, islisp_remove_property)

/* Functions */
static islisp_val c_funcall(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_funcall_argv(argv[0], argc - 1, argv + 1);
}
static islisp_val c_apply(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_apply(argv[0], argc - 1, argv + 1);
}
WRAP_1(identity, islisp_identity)

/* Stream and I/O wrappers */
WRAP_0(standard_input, islisp_standard_input())
WRAP_0(standard_output, islisp_standard_output())
WRAP_0(error_output, islisp_error_output())

static islisp_val c_open_input_file(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_open_input_file(argv[0], argc > 1 ? argv[1] : ISLISP_NIL);
}
static islisp_val c_open_output_file(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_open_output_file(argv[0], argc > 1 ? argv[1] : ISLISP_NIL);
}
static islisp_val c_open_io_file(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_open_io_file(argv[0], argc > 1 ? argv[1] : ISLISP_NIL);
}
WRAP_1(close, islisp_close)
WRAP_1(finish_output, islisp_finish_output)
WRAP_1(create_string_input_stream, islisp_create_string_input_stream)
WRAP_0(create_string_output_stream, islisp_create_string_output_stream())
WRAP_1(get_output_stream_string, islisp_get_output_stream_string)

static islisp_val c_read(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    islisp_val s = argc > 0 ? argv[0] : ISLISP_UNBOUND;
    islisp_val err = argc > 1 ? argv[1] : ISLISP_UNBOUND;
    islisp_val eos = argc > 2 ? argv[2] : ISLISP_UNBOUND;
    return islisp_read(s, err, eos);
}
static islisp_val c_read_char(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    islisp_val s = argc > 0 ? argv[0] : ISLISP_UNBOUND;
    islisp_val err = argc > 1 ? argv[1] : ISLISP_UNBOUND;
    islisp_val eos = argc > 2 ? argv[2] : ISLISP_UNBOUND;
    return islisp_read_char(s, err, eos);
}
static islisp_val c_preview_char(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    islisp_val s = argc > 0 ? argv[0] : ISLISP_UNBOUND;
    islisp_val err = argc > 1 ? argv[1] : ISLISP_UNBOUND;
    islisp_val eos = argc > 2 ? argv[2] : ISLISP_UNBOUND;
    return islisp_preview_char(s, err, eos);
}
static islisp_val c_read_line(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    islisp_val s = argc > 0 ? argv[0] : ISLISP_UNBOUND;
    islisp_val err = argc > 1 ? argv[1] : ISLISP_UNBOUND;
    islisp_val eos = argc > 2 ? argv[2] : ISLISP_UNBOUND;
    return islisp_read_line(s, err, eos);
}
static islisp_val c_read_byte(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    islisp_val s = argc > 0 ? argv[0] : ISLISP_UNBOUND;
    islisp_val err = argc > 1 ? argv[1] : ISLISP_UNBOUND;
    islisp_val eos = argc > 2 ? argv[2] : ISLISP_UNBOUND;
    return islisp_read_byte(s, err, eos);
}
WRAP_2(write_byte, islisp_write_byte)

static islisp_val c_format(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_format(argv[0], argv[1], argc - 2, argv + 2);
}
WRAP_2(format_char, islisp_format_char)
WRAP_2(format_float, islisp_format_float)
WRAP_1(format_fresh_line, islisp_format_fresh_line)
static islisp_val c_format_integer(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_format_integer(argv[0], argv[1], argc > 2 ? argv[2] : ISLISP_UNBOUND);
}
static islisp_val c_format_object(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_format_object(argv[0], argv[1], argc > 2 ? argv[2] : ISLISP_UNBOUND);
}
WRAP_2(format_tab, islisp_format_tab)

WRAP_1(stream_ready_p, islisp_stream_ready_p)
WRAP_1(open_stream_p, islisp_open_stream_p)
WRAP_1(input_stream_p, islisp_input_stream_p)
WRAP_1(output_stream_p, islisp_output_stream_p)
WRAP_1(probe_file, islisp_probe_file)
WRAP_1(file_position, islisp_file_position)
WRAP_2(set_file_position, islisp_set_file_position)
WRAP_2(file_length, islisp_file_length)

/* Time */
WRAP_0(get_universal_time, islisp_get_universal_time())
WRAP_0(get_internal_run_time, islisp_get_internal_run_time())
WRAP_0(get_internal_real_time, islisp_get_internal_real_time())
WRAP_0(internal_time_units_per_second, islisp_internal_time_units_per_second())

/* ILOS */
static islisp_val c_create(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_create(argv[0], argc - 1, argv + 1);
}
static islisp_val c_class(islisp_val env, int argc, islisp_val *argv) {
    (void)env; (void)argc;
    if (IS_CLASS(argv[0])) return argv[0];
    return islisp_get_global(argv[0]);
}
WRAP_2(initialize_object, islisp_initialize_object)
WRAP_0(call_next_method, islisp_call_next_method())
WRAP_0(next_method_p, islisp_next_method_p())
WRAP_2(slot_value, islisp_slot_value)
WRAP_3(set_slot_value, islisp_set_slot_value)
WRAP_2(slot_boundp, islisp_slot_boundp)

/* Conditions */
static islisp_val c_signal_condition(islisp_val env, int argc, islisp_val *argv) {
    (void)env; (void)argc;
    return islisp_signal_condition(argv[0], IS_TRUE(argv[1]));
}
WRAP_1(condition_continuable, islisp_condition_continuable)
static islisp_val c_continue_condition(islisp_val env, int argc, islisp_val *argv) {
    (void)env;
    return islisp_continue_condition(argv[0], argc > 1 ? argv[1] : ISLISP_NIL);
}
WRAP_1(arithmetic_error_operation, islisp_arithmetic_error_operation)
WRAP_1(arithmetic_error_operands, islisp_arithmetic_error_operands)
WRAP_1(domain_error_object, islisp_domain_error_object)
WRAP_1(domain_error_expected_class, islisp_domain_error_expected_class)
WRAP_1(parse_error_string, islisp_parse_error_string)
WRAP_1(parse_error_expected_class, islisp_parse_error_expected_class)
WRAP_1(simple_error_format_string, islisp_simple_error_format_string)
WRAP_1(simple_error_format_arguments, islisp_simple_error_format_arguments)
WRAP_1(stream_error_stream, islisp_stream_error_stream)
WRAP_1(undefined_entity_name, islisp_undefined_entity_name)
WRAP_1(undefined_entity_namespace, islisp_undefined_entity_namespace)

/* GC Builtins */
static islisp_val c_gc(islisp_val env, int argc, islisp_val *argv) {
    (void)env; (void)argc; (void)argv;
    islisp_gc_collect();
    return ISLISP_T;
}

static islisp_val c_gc_stats(islisp_val env, int argc, islisp_val *argv) {
    (void)env; (void)argc; (void)argv;
    islisp_gc_stats_t s;
    islisp_gc_get_stats(&s);
    islisp_val res = ISLISP_NIL;
    res = islisp_cons(islisp_cons(islisp_intern("threshold"), TO_INT(s.threshold)), res);
    res = islisp_cons(islisp_cons(islisp_intern("freed-bytes"), TO_INT(s.freed_bytes)), res);
    res = islisp_cons(islisp_cons(islisp_intern("freed-objects"), TO_INT(s.freed_objects)), res);
    res = islisp_cons(islisp_cons(islisp_intern("collections"), TO_INT(s.collections)), res);
    res = islisp_cons(islisp_cons(islisp_intern("total-objects"), TO_INT(s.total_objects)), res);
    res = islisp_cons(islisp_cons(islisp_intern("total-allocated"), TO_INT(s.total_allocated)), res);
    res = islisp_cons(islisp_cons(islisp_intern("bytes-allocated"), TO_INT(s.bytes_allocated)), res);
    return res;
}

void islisp_register_builtins(void) {
    /* Math */
    register_builtin("+", c_add, 0, -1);
    register_builtin("-", c_sub, 1, -1);
    register_builtin("*", c_mul, 0, -1);
    register_builtin("/", c_div, 1, -1);
    register_builtin("quotient", c_quotient, 2, 2);
    register_builtin("remainder", c_remainder, 2, 2);
    register_builtin("div", c_num_div, 2, 2);
    register_builtin("mod", c_num_mod, 2, 2);
    register_builtin("abs", c_abs, 1, 1);
    register_builtin("min", c_min, 1, -1);
    register_builtin("max", c_max, 1, -1);
    register_builtin("reciprocal", c_reciprocal, 1, 1);
    register_builtin("exp", c_exp, 1, 1);
    register_builtin("log", c_log, 1, 1);
    register_builtin("expt", c_expt, 2, 2);
    register_builtin("sqrt", c_sqrt, 1, 1);
    register_builtin("sin", c_sin, 1, 1);
    register_builtin("cos", c_cos, 1, 1);
    register_builtin("tan", c_tan, 1, 1);
    register_builtin("atan", c_atan, 1, 1);
    register_builtin("atan2", c_atan2, 2, 2);
    register_builtin("sinh", c_sinh, 1, 1);
    register_builtin("cosh", c_cosh, 1, 1);
    register_builtin("tanh", c_tanh, 1, 1);
    register_builtin("atanh", c_atanh, 1, 1);
    register_builtin("floor", c_floor, 1, 1);
    register_builtin("ceiling", c_ceiling, 1, 1);
    register_builtin("truncate", c_truncate, 1, 1);
    register_builtin("round", c_round, 1, 1);
    register_builtin("gcd", c_gcd, 2, 2);
    register_builtin("lcm", c_lcm, 2, 2);
    register_builtin("isqrt", c_isqrt, 1, 1);
    register_builtin("float", c_float, 1, 1);
    register_builtin("parse-number", c_parse_number, 1, 1);

    register_builtin("=", c_num_eq, 2, 2);
    register_builtin("/=", c_num_neq, 2, 2);
    register_builtin("<", c_num_lt, 2, 2);
    register_builtin("<=", c_num_lteq, 2, 2);
    register_builtin(">", c_num_gt, 2, 2);
    register_builtin(">=", c_num_gteq, 2, 2);

    /* Predicates */
    register_builtin("eq", c_eq, 2, 2);
    register_builtin("eql", c_eql, 2, 2);
    register_builtin("equal", c_equal, 2, 2);
    register_builtin("not", c_not, 1, 1);
    register_builtin("null", c_null, 1, 1);
    register_builtin("symbolp", c_symbolp, 1, 1);
    register_builtin("numberp", c_numberp, 1, 1);
    register_builtin("integerp", c_integerp, 1, 1);
    register_builtin("floatp", c_floatp, 1, 1);
    register_builtin("characterp", c_characterp, 1, 1);
    register_builtin("consp", c_consp, 1, 1);
    register_builtin("listp", c_listp, 1, 1);
    register_builtin("basic-vector-p", c_basic_vector_p, 1, 1);
    register_builtin("general-vector-p", c_general_vector_p, 1, 1);
    register_builtin("stringp", c_stringp, 1, 1);
    register_builtin("streamp", c_streamp, 1, 1);
    register_builtin("functionp", c_functionp, 1, 1);
    register_builtin("generic-function-p", c_generic_function_p, 1, 1);
    register_builtin("instancep", c_instancep, 2, 2);
    register_builtin("subclassp", c_subclassp, 2, 2);
    register_builtin("class-of", c_class_of, 1, 1);

    /* Lists */
    register_builtin("cons", c_cons, 2, 2);
    register_builtin("car", c_car, 1, 1);
    register_builtin("cdr", c_cdr, 1, 1);
    register_builtin("set-car", c_set_car, 2, 2);
    register_builtin("set-cdr", c_set_cdr, 2, 2);
    register_builtin("create-list", c_create_list, 1, 2);
    register_builtin("list", c_list, 0, -1);
    register_builtin("reverse", c_reverse, 1, 1);
    register_builtin("nreverse", c_nreverse, 1, 1);
    register_builtin("append", c_append, 0, -1);
    register_builtin("member", c_member, 2, 2);
    register_builtin("assoc", c_assoc, 2, 2);
    register_builtin("length", c_length, 1, 1);
    register_builtin("mapcar", c_mapcar, 2, -1);
    register_builtin("mapc", c_mapc, 2, -1);
    register_builtin("mapcan", c_mapcan, 2, -1);
    register_builtin("maplist", c_maplist, 2, -1);
    register_builtin("mapl", c_mapl, 2, -1);
    register_builtin("mapcon", c_mapcon, 2, -1);

    /* Sequences & Strings */
    register_builtin("elt", c_elt, 2, 2);
    register_builtin("set-elt", c_set_elt, 3, 3);
    register_builtin("subseq", c_subseq, 3, 3);
    register_builtin("map-into", c_map_into, 2, -1);
    register_builtin("create-vector", c_create_vector, 1, 2);
    register_builtin("vector", c_vector, 0, -1);
    register_builtin("create-string", c_create_string, 1, 2);
    register_builtin("string=", c_string_eq, 2, 2);
    register_builtin("string/=", c_string_neq, 2, 2);
    register_builtin("string<", c_string_lt, 2, 2);
    register_builtin("string<=", c_string_lteq, 2, 2);
    register_builtin("string>", c_string_gt, 2, 2);
    register_builtin("string>=", c_string_gteq, 2, 2);
    register_builtin("char-index", c_char_index, 2, 3);
    register_builtin("string-index", c_string_index, 2, 3);
    register_builtin("string-append", c_string_append, 0, -1);

    /* Characters */
    register_builtin("char=", c_char_eq, 2, 2);
    register_builtin("char/=", c_char_neq, 2, 2);
    register_builtin("char<", c_char_lt, 2, 2);
    register_builtin("char<=", c_char_lteq, 2, 2);
    register_builtin("char>", c_char_gt, 2, 2);
    register_builtin("char>=", c_char_gteq, 2, 2);

    /* Arrays */
    register_builtin("create-array", c_create_array, 1, 2);
    register_builtin("aref", c_aref, 1, -1);
    register_builtin("garef", c_garef, 1, -1);
    register_builtin("set-aref", c_set_aref, 2, -1);
    register_builtin("set-garef", c_set_garef, 2, -1);
    register_builtin("array-dimensions", c_array_dimensions, 1, 1);
    register_builtin("basic-array-p", c_basic_array_p, 1, 1);
    register_builtin("basic-array*-p", c_basic_array_s_p, 1, 1);
    register_builtin("general-array*-p", c_general_array_s_p, 1, 1);

    /* Conversion */
    register_builtin("convert", c_convert, 2, 2);

    /* Symbols */
    register_builtin("gensym", c_gensym, 0, 0);
    register_builtin("symbol-name", c_symbol_name, 1, 1);
    register_builtin("property", c_property, 2, 3);
    register_builtin("set-property", c_set_property, 3, 3);
    register_builtin("remove-property", c_remove_property, 2, 2);

    /* Functions */
    register_builtin("funcall", c_funcall, 1, -1);
    register_builtin("apply", c_apply, 2, -1);
    register_builtin("identity", c_identity, 1, 1);

    /* Streams & I/O */
    register_builtin("standard-input", c_standard_input, 0, 0);
    register_builtin("standard-output", c_standard_output, 0, 0);
    register_builtin("error-output", c_error_output, 0, 0);
    register_builtin("open-input-file", c_open_input_file, 1, 2);
    register_builtin("open-output-file", c_open_output_file, 1, 2);
    register_builtin("open-io-file", c_open_io_file, 1, 2);
    register_builtin("close", c_close, 1, 1);
    register_builtin("finish-output", c_finish_output, 1, 1);
    register_builtin("create-string-input-stream", c_create_string_input_stream, 1, 1);
    register_builtin("create-string-output-stream", c_create_string_output_stream, 0, 0);
    register_builtin("get-output-stream-string", c_get_output_stream_string, 1, 1);
    register_builtin("read", c_read, 0, 3);
    register_builtin("read-char", c_read_char, 0, 3);
    register_builtin("preview-char", c_preview_char, 0, 3);
    register_builtin("read-line", c_read_line, 0, 3);
    register_builtin("read-byte", c_read_byte, 0, 3);
    register_builtin("write-byte", c_write_byte, 2, 2);
    register_builtin("format", c_format, 2, -1);
    register_builtin("format-char", c_format_char, 2, 2);
    register_builtin("format-float", c_format_float, 2, 2);
    register_builtin("format-fresh-line", c_format_fresh_line, 1, 1);
    register_builtin("format-integer", c_format_integer, 2, 3);
    register_builtin("format-object", c_format_object, 2, 3);
    register_builtin("format-tab", c_format_tab, 2, 2);
    register_builtin("stream-ready-p", c_stream_ready_p, 1, 1);
    register_builtin("open-stream-p", c_open_stream_p, 1, 1);
    register_builtin("input-stream-p", c_input_stream_p, 1, 1);
    register_builtin("output-stream-p", c_output_stream_p, 1, 1);
    register_builtin("probe-file", c_probe_file, 1, 1);
    register_builtin("file-position", c_file_position, 1, 1);
    register_builtin("set-file-position", c_set_file_position, 2, 2);
    register_builtin("file-length", c_file_length, 2, 2);

    /* Time */
    register_builtin("get-universal-time", c_get_universal_time, 0, 0);
    register_builtin("get-internal-run-time", c_get_internal_run_time, 0, 0);
    register_builtin("get-internal-real-time", c_get_internal_real_time, 0, 0);
    register_builtin("internal-time-units-per-second", c_internal_time_units_per_second, 0, 0);

    /* ILOS */
    register_builtin("create", c_create, 1, -1);
    register_builtin("class", c_class, 1, 1);
    register_builtin("initialize-object", c_initialize_object, 2, 2);
    register_builtin("call-next-method", c_call_next_method, 0, 0);
    register_builtin("next-method-p", c_next_method_p, 0, 0);
    register_builtin("slot-value", c_slot_value, 2, 2);
    register_builtin("set-slot-value", c_set_slot_value, 3, 3);
    register_builtin("slot-boundp", c_slot_boundp, 2, 2);

    /* Conditions */
    register_builtin("signal-condition", c_signal_condition, 2, 2);
    register_builtin("condition-continuable", c_condition_continuable, 1, 1);
    register_builtin("continue-condition", c_continue_condition, 1, 2);
    register_builtin("arithmetic-error-operation", c_arithmetic_error_operation, 1, 1);
    register_builtin("arithmetic-error-operands", c_arithmetic_error_operands, 1, 1);
    register_builtin("domain-error-object", c_domain_error_object, 1, 1);
    register_builtin("domain-error-expected-class", c_domain_error_expected_class, 1, 1);
    register_builtin("parse-error-string", c_parse_error_string, 1, 1);
    register_builtin("parse-error-expected-class", c_parse_error_expected_class, 1, 1);
    register_builtin("simple-error-format-string", c_simple_error_format_string, 1, 1);
    register_builtin("simple-error-format-arguments", c_simple_error_format_arguments, 1, 1);
    register_builtin("stream-error-stream", c_stream_error_stream, 1, 1);
    register_builtin("undefined-entity-name", c_undefined_entity_name, 1, 1);
    register_builtin("undefined-entity-namespace", c_undefined_entity_namespace, 1, 1);

    /* Garbage Collection */
    register_builtin("gc", c_gc, 0, 0);
    register_builtin("gc-stats", c_gc_stats, 0, 0);
}
