#include "islisp_compiler.h"

islisp_val islisp_read_file(const char *filename) {
    islisp_val str_fn = islisp_make_string(filename);
    islisp_val stream = islisp_open_input_file(str_fn, ISLISP_NIL);
    if (!IS_STREAM(stream)) {
        return ISLISP_NIL;
    }
    islisp_val forms = ISLISP_NIL;
    islisp_val *tail = &forms;

    while (1) {
        islisp_val form = islisp_read(stream, ISLISP_NIL, ISLISP_EOF);
        if (form == ISLISP_EOF) break;
        islisp_val cell = islisp_cons(form, ISLISP_NIL);
        *tail = cell;
        tail = &(AS_CONS(cell)->cdr);
    }
    islisp_close(stream);
    return forms;
}

islisp_val islisp_read_from_string(const char *str) {
    islisp_val s = islisp_make_string(str);
    islisp_val stream = islisp_create_string_input_stream(s);
    islisp_val forms = ISLISP_NIL;
    islisp_val *tail = &forms;

    while (1) {
        islisp_val form = islisp_read(stream, ISLISP_NIL, ISLISP_EOF);
        if (form == ISLISP_EOF) break;
        islisp_val cell = islisp_cons(form, ISLISP_NIL);
        *tail = cell;
        tail = &(AS_CONS(cell)->cdr);
    }
    return forms;
}
