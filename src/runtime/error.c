#include "islisp_runtime.h"
#include <stdarg.h>

/* Condition Classes definitions (initialized in init_runtime) */
islisp_val CLASS_OBJECT = 0;
islisp_val CLASS_STANDARD_OBJECT = 0;
islisp_val CLASS_NUMBER = 0;
islisp_val CLASS_INTEGER = 0;
islisp_val CLASS_FLOAT = 0;
islisp_val CLASS_CHARACTER = 0;
islisp_val CLASS_SYMBOL = 0;
islisp_val CLASS_LIST = 0;
islisp_val CLASS_CONS = 0;
islisp_val CLASS_NULL = 0;
islisp_val CLASS_BASIC_ARRAY = 0;
islisp_val CLASS_BASIC_ARRAY_STAR = 0;
islisp_val CLASS_GENERAL_ARRAY_STAR = 0;
islisp_val CLASS_BASIC_VECTOR = 0;
islisp_val CLASS_GENERAL_VECTOR = 0;
islisp_val CLASS_STRING = 0;
islisp_val CLASS_STREAM = 0;
islisp_val CLASS_FUNCTION = 0;
islisp_val CLASS_GENERIC_FUNCTION = 0;
islisp_val CLASS_STANDARD_GENERIC_FUNCTION = 0;
islisp_val CLASS_SERIOUS_CONDITION = 0;
islisp_val CLASS_ERROR = 0;
islisp_val CLASS_ARITHMETIC_ERROR = 0;
islisp_val CLASS_DIVISION_BY_ZERO = 0;
islisp_val CLASS_FLOATING_POINT_OVERFLOW = 0;
islisp_val CLASS_FLOATING_POINT_UNDERFLOW = 0;
islisp_val CLASS_CONTROL_ERROR = 0;
islisp_val CLASS_DOMAIN_ERROR = 0;
islisp_val CLASS_PARSE_ERROR = 0;
islisp_val CLASS_PROGRAM_ERROR = 0;
islisp_val CLASS_SIMPLE_ERROR = 0;
islisp_val CLASS_STREAM_ERROR = 0;
islisp_val CLASS_END_OF_STREAM = 0;
islisp_val CLASS_STORAGE_EXHAUSTED = 0;
islisp_val CLASS_UNDEFINED_ENTITY = 0;
islisp_val CLASS_UNBOUND_VARIABLE = 0;
islisp_val CLASS_UNDEFINED_FUNCTION = 0;

void islisp_gc_mark_global_classes(void) {
    islisp_gc_mark_val(CLASS_OBJECT);
    islisp_gc_mark_val(CLASS_STANDARD_OBJECT);
    islisp_gc_mark_val(CLASS_NUMBER);
    islisp_gc_mark_val(CLASS_INTEGER);
    islisp_gc_mark_val(CLASS_FLOAT);
    islisp_gc_mark_val(CLASS_CHARACTER);
    islisp_gc_mark_val(CLASS_SYMBOL);
    islisp_gc_mark_val(CLASS_LIST);
    islisp_gc_mark_val(CLASS_CONS);
    islisp_gc_mark_val(CLASS_NULL);
    islisp_gc_mark_val(CLASS_BASIC_ARRAY);
    islisp_gc_mark_val(CLASS_BASIC_ARRAY_STAR);
    islisp_gc_mark_val(CLASS_GENERAL_ARRAY_STAR);
    islisp_gc_mark_val(CLASS_BASIC_VECTOR);
    islisp_gc_mark_val(CLASS_GENERAL_VECTOR);
    islisp_gc_mark_val(CLASS_STRING);
    islisp_gc_mark_val(CLASS_STREAM);
    islisp_gc_mark_val(CLASS_FUNCTION);
    islisp_gc_mark_val(CLASS_GENERIC_FUNCTION);
    islisp_gc_mark_val(CLASS_STANDARD_GENERIC_FUNCTION);
    islisp_gc_mark_val(CLASS_SERIOUS_CONDITION);
    islisp_gc_mark_val(CLASS_ERROR);
    islisp_gc_mark_val(CLASS_ARITHMETIC_ERROR);
    islisp_gc_mark_val(CLASS_DIVISION_BY_ZERO);
    islisp_gc_mark_val(CLASS_FLOATING_POINT_OVERFLOW);
    islisp_gc_mark_val(CLASS_FLOATING_POINT_UNDERFLOW);
    islisp_gc_mark_val(CLASS_CONTROL_ERROR);
    islisp_gc_mark_val(CLASS_DOMAIN_ERROR);
    islisp_gc_mark_val(CLASS_PARSE_ERROR);
    islisp_gc_mark_val(CLASS_PROGRAM_ERROR);
    islisp_gc_mark_val(CLASS_SIMPLE_ERROR);
    islisp_gc_mark_val(CLASS_STREAM_ERROR);
    islisp_gc_mark_val(CLASS_END_OF_STREAM);
    islisp_gc_mark_val(CLASS_STORAGE_EXHAUSTED);
    islisp_gc_mark_val(CLASS_UNDEFINED_ENTITY);
    islisp_gc_mark_val(CLASS_UNBOUND_VARIABLE);
    islisp_gc_mark_val(CLASS_UNDEFINED_FUNCTION);
}

/* Frames management */

void islisp_push_frame(islisp_frame_t *frame) {
    frame->prev = g_top_frame;
    g_top_frame = frame;
}

void islisp_pop_frame(islisp_frame_t *frame) {
    if (g_top_frame == frame) {
        g_top_frame = frame->prev;
    }
}

static islisp_frame_t *g_unwind_target = NULL;
static islisp_val g_unwind_val = ISLISP_NIL;

void islisp_continue_unwind(void) {
    islisp_frame_t *f = g_top_frame;
    while (f && f != g_unwind_target) {
        if (f->kind == FRAME_UNWIND) {
            longjmp(f->jmp, 1);
        }
        f = f->prev;
    }
    if (g_unwind_target) {
        islisp_frame_t *t = g_unwind_target;
        t->val = g_unwind_val;
        g_top_frame = t;
        g_unwind_target = NULL;
        longjmp(t->jmp, 1);
    }
}

void islisp_throw(islisp_val tag, islisp_val val) {
    islisp_frame_t *f = g_top_frame;
    while (f) {
        if (f->kind == FRAME_CATCH && f->tag == tag) {
            g_unwind_target = f;
            g_unwind_val = val;
            islisp_continue_unwind();
            return;
        }
        f = f->prev;
    }
    islisp_error("throw: no corresponding catch tag found");
}

void islisp_return_from(islisp_val name, islisp_val val) {
    islisp_frame_t *f = g_top_frame;
    while (f) {
        if (f->kind == FRAME_BLOCK && f->tag == name) {
            g_unwind_target = f;
            g_unwind_val = val;
            islisp_continue_unwind();
            return;
        }
        f = f->prev;
    }
    islisp_error("return-from: block %s is not active", IS_SYMBOL(name) ? ((islisp_symbol_t*)name)->name : "unknown");
}

/* Conditions & Errors */

islisp_val islisp_make_condition(islisp_val class_obj, const char *msg, islisp_val irritants, bool continuable) {
    islisp_condition_t *cond = (islisp_condition_t*)islisp_alloc(sizeof(islisp_condition_t));
    cond->header.type = TYPE_CONDITION;
    cond->header.flags = 0;
    cond->header.extra = 0;
    cond->header.len = 0;
    cond->class_obj = class_obj;
    cond->message = msg ? strdup(msg) : "error";
    cond->irritants = irritants;
    cond->continuable = continuable;
    cond->extra_data = ISLISP_NIL;
    return TO_HEAP(cond);
}

islisp_val islisp_signal_condition(islisp_val condition, bool continuable) {
    g_last_condition = condition;
    islisp_frame_t *f = g_top_frame;
    while (f) {
        if (f->kind == FRAME_HANDLER) {
            islisp_val handler = f->var;
            /* Temporarily pop handler frame during invocation to avoid recursive looping */
            islisp_frame_t *saved_top = g_top_frame;
            g_top_frame = f->prev;
            islisp_val res = islisp_funcall(handler, 1, condition);
            g_top_frame = saved_top;
            if (continuable) return res;
        }
        f = f->prev;
    }
    /* Unhandled condition */
    islisp_condition_t *c = (islisp_condition_t*)condition;
    fprintf(stderr, "\n*** ERROR: %s\n", c->message ? c->message : "Unhandled condition");
    exit(1);
    return ISLISP_NIL;
}

islisp_val islisp_error(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    islisp_val cond = islisp_make_condition(CLASS_SIMPLE_ERROR, buf, ISLISP_NIL, false);
    return islisp_signal_condition(cond, false);
}

islisp_val islisp_cerror(const char *cont_fmt, const char *fmt, ...) {
    (void)cont_fmt;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    islisp_val cond = islisp_make_condition(CLASS_SIMPLE_ERROR, buf, ISLISP_NIL, true);
    return islisp_signal_condition(cond, true);
}

islisp_val islisp_continue_condition(islisp_val condition, islisp_val value) {
    if (!IS_CONDITION(condition)) islisp_error("continue-condition: argument must be a condition");
    islisp_condition_t *c = (islisp_condition_t*)condition;
    if (!c->continuable) islisp_error("continue-condition: condition is not continuable");
    return value;
}

islisp_val islisp_condition_continuable(islisp_val condition) {
    if (!IS_CONDITION(condition)) return ISLISP_NIL;
    return BOOL_VAL(((islisp_condition_t*)condition)->continuable);
}

/* Condition Accessors */

islisp_val islisp_arithmetic_error_operation(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("arithmetic-error-operation: not a condition");
    return ((islisp_condition_t*)condition)->extra_data;
}

islisp_val islisp_arithmetic_error_operands(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("arithmetic-error-operands: not a condition");
    return ((islisp_condition_t*)condition)->irritants;
}

islisp_val islisp_domain_error_object(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("domain-error-object: not a condition");
    return ((islisp_condition_t*)condition)->irritants;
}

islisp_val islisp_domain_error_expected_class(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("domain-error-expected-class: not a condition");
    return ((islisp_condition_t*)condition)->extra_data;
}

islisp_val islisp_parse_error_string(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("parse-error-string: not a condition");
    return ((islisp_condition_t*)condition)->irritants;
}

islisp_val islisp_parse_error_expected_class(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("parse-error-expected-class: not a condition");
    return ((islisp_condition_t*)condition)->extra_data;
}

islisp_val islisp_simple_error_format_string(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("simple-error-format-string: not a condition");
    return islisp_make_string(((islisp_condition_t*)condition)->message);
}

islisp_val islisp_simple_error_format_arguments(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("simple-error-format-arguments: not a condition");
    return ((islisp_condition_t*)condition)->irritants;
}

islisp_val islisp_stream_error_stream(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("stream-error-stream: not a condition");
    return ((islisp_condition_t*)condition)->irritants;
}

islisp_val islisp_undefined_entity_name(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("undefined-entity-name: not a condition");
    return ((islisp_condition_t*)condition)->irritants;
}

islisp_val islisp_undefined_entity_namespace(islisp_val condition) {
    if (!IS_CONDITION(condition)) islisp_error("undefined-entity-namespace: not a condition");
    return ((islisp_condition_t*)condition)->extra_data;
}

/* Time Functions */

islisp_val islisp_get_universal_time(void) {
    /* Seconds since 1900-01-01 */
    time_t now = time(NULL);
    /* Offset between 1900 and 1970 is 2208988800 seconds */
    int64_t univ = (int64_t)now + 2208988800LL;
    return TO_INT(univ);
}

islisp_val islisp_get_internal_run_time(void) {
    clock_t c = clock();
    return TO_INT((int64_t)c);
}

islisp_val islisp_get_internal_real_time(void) {
    clock_t c = clock();
    return TO_INT((int64_t)c);
}

islisp_val islisp_internal_time_units_per_second(void) {
    return TO_INT((int64_t)CLOCKS_PER_SEC);
}
