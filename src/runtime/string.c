#include "islisp_runtime.h"

/* Character Operations */

islisp_val islisp_char_eq(islisp_val c1, islisp_val c2) {
    if (!IS_CHAR(c1) || !IS_CHAR(c2)) islisp_error("char=: arguments must be characters");
    return BOOL_VAL(AS_CHAR(c1) == AS_CHAR(c2));
}

islisp_val islisp_char_neq(islisp_val c1, islisp_val c2) {
    if (!IS_CHAR(c1) || !IS_CHAR(c2)) islisp_error("char/=: arguments must be characters");
    return BOOL_VAL(AS_CHAR(c1) != AS_CHAR(c2));
}

islisp_val islisp_char_lt(islisp_val c1, islisp_val c2) {
    if (!IS_CHAR(c1) || !IS_CHAR(c2)) islisp_error("char<: arguments must be characters");
    return BOOL_VAL(AS_CHAR(c1) < AS_CHAR(c2));
}

islisp_val islisp_char_lteq(islisp_val c1, islisp_val c2) {
    if (!IS_CHAR(c1) || !IS_CHAR(c2)) islisp_error("char<=: arguments must be characters");
    return BOOL_VAL(AS_CHAR(c1) <= AS_CHAR(c2));
}

islisp_val islisp_char_gt(islisp_val c1, islisp_val c2) {
    if (!IS_CHAR(c1) || !IS_CHAR(c2)) islisp_error("char>: arguments must be characters");
    return BOOL_VAL(AS_CHAR(c1) > AS_CHAR(c2));
}

islisp_val islisp_char_gteq(islisp_val c1, islisp_val c2) {
    if (!IS_CHAR(c1) || !IS_CHAR(c2)) islisp_error("char>=: arguments must be characters");
    return BOOL_VAL(AS_CHAR(c1) >= AS_CHAR(c2));
}

/* String Operations */

islisp_val islisp_create_string(islisp_val len_val, islisp_val init_ch) {
    if (!IS_INT(len_val) || AS_INT(len_val) < 0) {
        islisp_error("create-string: length must be a non-negative integer");
    }
    int64_t len = AS_INT(len_val);
    char fill = ' ';
    if (init_ch != ISLISP_UNBOUND && IS_CHAR(init_ch)) {
        fill = (char)AS_CHAR(init_ch);
    }
    islisp_val s = islisp_make_string_len(NULL, (size_t)len);
    islisp_string_t *str = (islisp_string_t*)s;
    memset(str->data, fill, (size_t)len);
    str->data[len] = '\0';
    return s;
}

islisp_val islisp_string_eq(islisp_val s1, islisp_val s2) {
    if (!IS_STRING(s1) || !IS_STRING(s2)) islisp_error("string=: arguments must be strings");
    return BOOL_VAL(strcmp(((islisp_string_t*)s1)->data, ((islisp_string_t*)s2)->data) == 0);
}

islisp_val islisp_string_neq(islisp_val s1, islisp_val s2) {
    if (!IS_STRING(s1) || !IS_STRING(s2)) islisp_error("string/=: arguments must be strings");
    return BOOL_VAL(strcmp(((islisp_string_t*)s1)->data, ((islisp_string_t*)s2)->data) != 0);
}

islisp_val islisp_string_lt(islisp_val s1, islisp_val s2) {
    if (!IS_STRING(s1) || !IS_STRING(s2)) islisp_error("string<: arguments must be strings");
    return BOOL_VAL(strcmp(((islisp_string_t*)s1)->data, ((islisp_string_t*)s2)->data) < 0);
}

islisp_val islisp_string_lteq(islisp_val s1, islisp_val s2) {
    if (!IS_STRING(s1) || !IS_STRING(s2)) islisp_error("string<=: arguments must be strings");
    return BOOL_VAL(strcmp(((islisp_string_t*)s1)->data, ((islisp_string_t*)s2)->data) <= 0);
}

islisp_val islisp_string_gt(islisp_val s1, islisp_val s2) {
    if (!IS_STRING(s1) || !IS_STRING(s2)) islisp_error("string>: arguments must be strings");
    return BOOL_VAL(strcmp(((islisp_string_t*)s1)->data, ((islisp_string_t*)s2)->data) > 0);
}

islisp_val islisp_string_gteq(islisp_val s1, islisp_val s2) {
    if (!IS_STRING(s1) || !IS_STRING(s2)) islisp_error("string>=: arguments must be strings");
    return BOOL_VAL(strcmp(((islisp_string_t*)s1)->data, ((islisp_string_t*)s2)->data) >= 0);
}

islisp_val islisp_char_index(islisp_val ch, islisp_val str_val, islisp_val start_val) {
    if (!IS_CHAR(ch)) islisp_error("char-index: first argument must be a character");
    if (!IS_STRING(str_val)) islisp_error("char-index: second argument must be a string");
    int64_t start = 0;
    if (start_val != ISLISP_UNBOUND) {
        if (!IS_INT(start_val)) islisp_error("char-index: start must be an integer");
        start = AS_INT(start_val);
    }
    islisp_string_t *str = (islisp_string_t*)str_val;
    if (start < 0 || (uint32_t)start > str->header.len) {
        islisp_error("char-index: start index out of bounds");
    }
    char target = (char)AS_CHAR(ch);
    for (uint32_t i = (uint32_t)start; i < str->header.len; i++) {
        if (str->data[i] == target) {
            return TO_INT(i);
        }
    }
    return ISLISP_NIL;
}

islisp_val islisp_string_index(islisp_val sub_val, islisp_val str_val, islisp_val start_val) {
    if (!IS_STRING(sub_val) || !IS_STRING(str_val)) {
        islisp_error("string-index: arguments must be strings");
    }
    int64_t start = 0;
    if (start_val != ISLISP_UNBOUND) {
        if (!IS_INT(start_val)) islisp_error("string-index: start must be an integer");
        start = AS_INT(start_val);
    }
    islisp_string_t *sub = (islisp_string_t*)sub_val;
    islisp_string_t *str = (islisp_string_t*)str_val;
    if (start < 0 || (uint32_t)start > str->header.len) {
        islisp_error("string-index: start index out of bounds");
    }
    char *found = strstr(str->data + start, sub->data);
    if (!found) return ISLISP_NIL;
    return TO_INT(found - str->data);
}

islisp_val islisp_string_append(int argc, islisp_val *argv) {
    size_t total_len = 0;
    for (int i = 0; i < argc; i++) {
        if (!IS_STRING(argv[i])) islisp_error("string-append: argument is not a string");
        total_len += ((islisp_string_t*)argv[i])->header.len;
    }
    islisp_val res = islisp_make_string_len(NULL, total_len);
    islisp_string_t *res_str = (islisp_string_t*)res;
    size_t offset = 0;
    for (int i = 0; i < argc; i++) {
        islisp_string_t *s = (islisp_string_t*)argv[i];
        memcpy(res_str->data + offset, s->data, s->header.len);
        offset += s->header.len;
    }
    res_str->data[total_len] = '\0';
    return res;
}

/* Vectors */

islisp_val islisp_create_vector(islisp_val len_val, islisp_val init) {
    if (!IS_INT(len_val) || AS_INT(len_val) < 0) {
        islisp_error("create-vector: length must be a non-negative integer");
    }
    return islisp_make_vector((int)AS_INT(len_val), init == ISLISP_UNBOUND ? ISLISP_NIL : init);
}

islisp_val islisp_vector(int argc, islisp_val *argv) {
    islisp_val v = islisp_make_vector(argc, ISLISP_NIL);
    islisp_vector_t *vec = (islisp_vector_t*)v;
    for (int i = 0; i < argc; i++) {
        vec->data[i] = argv[i];
    }
    return v;
}

/* Arrays */

islisp_val islisp_create_array(islisp_val dims_val, islisp_val init) {
    if (!islisp_listp(dims_val)) islisp_error("create-array: dimensions must be a list");
    int ndims = (int)AS_INT(islisp_length(dims_val));
    if (ndims == 0) islisp_error("create-array: dimensions cannot be empty");

    int *dims = (int*)malloc(sizeof(int) * ndims);
    islisp_val p = dims_val;
    size_t total_size = 1;
    for (int i = 0; i < ndims; i++) {
        islisp_val d = CAR(p);
        if (!IS_INT(d) || AS_INT(d) < 0) {
            free(dims);
            islisp_error("create-array: dimension must be a non-negative integer");
        }
        dims[i] = (int)AS_INT(d);
        total_size *= dims[i];
        p = CDR(p);
    }

    islisp_array_t *arr = (islisp_array_t*)islisp_alloc(sizeof(islisp_array_t));
    arr->header.type = TYPE_ARRAY;
    arr->header.flags = 0;
    arr->header.extra = 0;
    arr->header.len = (uint32_t)total_size;
    arr->ndims = ndims;
    arr->dims = dims;
    arr->data = (islisp_val*)malloc(sizeof(islisp_val) * (total_size > 0 ? total_size : 1));
    islisp_val fill = (init == ISLISP_UNBOUND) ? ISLISP_NIL : init;
    for (size_t i = 0; i < total_size; i++) {
        arr->data[i] = fill;
    }
    return TO_HEAP(arr);
}

static size_t array_index(islisp_array_t *arr, int argc, islisp_val *argv) {
    if (argc != arr->ndims) {
        islisp_error("Array dimension mismatch: expected %d indices, got %d", arr->ndims, argc);
    }
    size_t idx = 0;
    size_t stride = 1;
    for (int i = arr->ndims - 1; i >= 0; i--) {
        if (!IS_INT(argv[i])) islisp_error("Array index must be an integer");
        int64_t cur = AS_INT(argv[i]);
        if (cur < 0 || cur >= arr->dims[i]) {
            islisp_error("Array index %lld out of range [0, %d)", (long long)cur, arr->dims[i]);
        }
        idx += cur * stride;
        stride *= arr->dims[i];
    }
    return idx;
}

islisp_val islisp_aref(int argc, islisp_val *argv) {
    if (argc < 1) islisp_error("aref: requires at least array argument");
    islisp_val arr_val = argv[0];
    if (IS_ARRAY(arr_val)) {
        islisp_array_t *arr = (islisp_array_t*)arr_val;
        size_t idx = array_index(arr, argc - 1, argv + 1);
        return arr->data[idx];
    }
    if (IS_VECTOR(arr_val) || IS_STRING(arr_val)) {
        if (argc != 2) islisp_error("aref: 1D sequence requires 1 index");
        return islisp_elt(arr_val, argv[1]);
    }
    islisp_error("aref: first argument is not an array");
    return ISLISP_NIL;
}

islisp_val islisp_garef(int argc, islisp_val *argv) {
    return islisp_aref(argc, argv);
}

islisp_val islisp_set_aref(int argc, islisp_val *argv) {
    if (argc < 2) islisp_error("set-aref: requires at least value and array arguments");
    islisp_val val = argv[0];
    islisp_val arr_val = argv[1];
    if (IS_ARRAY(arr_val)) {
        islisp_array_t *arr = (islisp_array_t*)arr_val;
        size_t idx = array_index(arr, argc - 2, argv + 2);
        arr->data[idx] = val;
        return val;
    }
    if (IS_VECTOR(arr_val) || IS_STRING(arr_val)) {
        if (argc != 3) islisp_error("set-aref: 1D sequence requires 1 index");
        return islisp_set_elt(val, arr_val, argv[2]);
    }
    islisp_error("set-aref: second argument is not an array");
    return val;
}

islisp_val islisp_set_garef(int argc, islisp_val *argv) {
    return islisp_set_aref(argc, argv);
}

islisp_val islisp_array_dimensions(islisp_val arr_val) {
    if (IS_ARRAY(arr_val)) {
        islisp_array_t *arr = (islisp_array_t*)arr_val;
        islisp_val res = ISLISP_NIL;
        for (int i = arr->ndims - 1; i >= 0; i--) {
            res = islisp_cons(TO_INT(arr->dims[i]), res);
        }
        return res;
    }
    if (IS_VECTOR(arr_val)) {
        return islisp_cons(TO_INT(((islisp_vector_t*)arr_val)->header.len), ISLISP_NIL);
    }
    if (IS_STRING(arr_val)) {
        return islisp_cons(TO_INT(((islisp_string_t*)arr_val)->header.len), ISLISP_NIL);
    }
    islisp_error("array-dimensions: argument is not an array");
    return ISLISP_NIL;
}

islisp_val islisp_basic_array_p(islisp_val x) {
    return BOOL_VAL(IS_ARRAY(x) || IS_VECTOR(x) || IS_STRING(x));
}

islisp_val islisp_basic_array_s_p(islisp_val x) {
    return BOOL_VAL(IS_ARRAY(x));
}

islisp_val islisp_general_array_s_p(islisp_val x) {
    return BOOL_VAL(IS_ARRAY(x));
}

/* Type Coercion: convert */

islisp_val islisp_convert(islisp_val obj, islisp_val target_class) {
    if (IS_CHAR(obj)) {
        if (target_class == CLASS_INTEGER) return TO_INT(AS_CHAR(obj));
        if (target_class == CLASS_STRING) {
            char buf[2] = { (char)AS_CHAR(obj), '\0' };
            return islisp_make_string(buf);
        }
        if (target_class == CLASS_SYMBOL) {
            char buf[2] = { (char)AS_CHAR(obj), '\0' };
            return islisp_intern(buf);
        }
    } else if (IS_INT(obj)) {
        if (target_class == CLASS_CHARACTER) return TO_CHAR((int)AS_INT(obj));
        if (target_class == CLASS_FLOAT) return islisp_make_float((double)AS_INT(obj));
        if (target_class == CLASS_STRING) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lld", (long long)AS_INT(obj));
            return islisp_make_string(buf);
        }
    } else if (IS_FLOAT(obj)) {
        if (target_class == CLASS_INTEGER) return TO_INT((int64_t)((islisp_float_t*)obj)->val);
        if (target_class == CLASS_STRING) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.16g", ((islisp_float_t*)obj)->val);
            return islisp_make_string(buf);
        }
    } else if (IS_SYMBOL(obj)) {
        if (target_class == CLASS_STRING) return islisp_symbol_name(obj);
    } else if (IS_STRING(obj)) {
        islisp_string_t *str = (islisp_string_t*)obj;
        if (target_class == CLASS_SYMBOL) return islisp_intern(str->data);
        if (target_class == CLASS_LIST) {
            islisp_val res = ISLISP_NIL;
            for (int i = (int)str->header.len - 1; i >= 0; i--) {
                res = islisp_cons(TO_CHAR((unsigned char)str->data[i]), res);
            }
            return res;
        }
        if (target_class == CLASS_GENERAL_VECTOR) {
            islisp_val vec = islisp_make_vector(str->header.len, ISLISP_NIL);
            islisp_vector_t *v = (islisp_vector_t*)vec;
            for (uint32_t i = 0; i < str->header.len; i++) {
                v->data[i] = TO_CHAR((unsigned char)str->data[i]);
            }
            return vec;
        }
    } else if (IS_VECTOR(obj)) {
        islisp_vector_t *vec = (islisp_vector_t*)obj;
        if (target_class == CLASS_LIST) {
            islisp_val res = ISLISP_NIL;
            for (int i = (int)vec->header.len - 1; i >= 0; i--) {
                res = islisp_cons(vec->data[i], res);
            }
            return res;
        }
        if (target_class == CLASS_STRING) {
            islisp_val s = islisp_make_string_len(NULL, vec->header.len);
            islisp_string_t *str = (islisp_string_t*)s;
            for (uint32_t i = 0; i < vec->header.len; i++) {
                if (!IS_CHAR(vec->data[i])) islisp_error("convert: vector element is not a character");
                str->data[i] = (char)AS_CHAR(vec->data[i]);
            }
            str->data[vec->header.len] = '\0';
            return s;
        }
    } else if (IS_CONS(obj) || IS_NIL(obj)) {
        if (target_class == CLASS_GENERAL_VECTOR) {
            int len = (int)AS_INT(islisp_length(obj));
            islisp_val vec = islisp_make_vector(len, ISLISP_NIL);
            islisp_vector_t *v = (islisp_vector_t*)vec;
            islisp_val p = obj;
            for (int i = 0; i < len; i++) {
                v->data[i] = CAR(p);
                p = CDR(p);
            }
            return vec;
        }
        if (target_class == CLASS_STRING) {
            int len = (int)AS_INT(islisp_length(obj));
            islisp_val s = islisp_make_string_len(NULL, len);
            islisp_string_t *str = (islisp_string_t*)s;
            islisp_val p = obj;
            for (int i = 0; i < len; i++) {
                islisp_val c = CAR(p);
                if (!IS_CHAR(c)) islisp_error("convert: list element is not a character");
                str->data[i] = (char)AS_CHAR(c);
                p = CDR(p);
            }
            str->data[len] = '\0';
            return s;
        }
    }

    islisp_error("convert: cannot convert object to target class");
    return ISLISP_NIL;
}
