#include "islisp_runtime.h"

islisp_val islisp_car(islisp_val c) {
    if (!IS_CONS(c)) {
        islisp_error("car: argument must be a cons, got %s", IS_NIL(c) ? "nil" : "non-cons");
    }
    return CAR(c);
}

islisp_val islisp_cdr(islisp_val c) {
    if (!IS_CONS(c)) {
        islisp_error("cdr: argument must be a cons, got %s", IS_NIL(c) ? "nil" : "non-cons");
    }
    return CDR(c);
}

islisp_val islisp_set_car(islisp_val val, islisp_val c) {
    if (!IS_CONS(c)) {
        islisp_error("set-car: argument must be a cons");
    }
    AS_CONS(c)->car = val;
    return val;
}

islisp_val islisp_set_cdr(islisp_val val, islisp_val c) {
    if (!IS_CONS(c)) {
        islisp_error("set-cdr: argument must be a cons");
    }
    AS_CONS(c)->cdr = val;
    return val;
}

islisp_val islisp_create_list(islisp_val len_val, islisp_val init) {
    if (!IS_INT(len_val) || AS_INT(len_val) < 0) {
        islisp_error("create-list: length must be a non-negative integer");
    }
    int64_t n = AS_INT(len_val);
    islisp_val res = ISLISP_NIL;
    for (int64_t i = 0; i < n; i++) {
        res = islisp_cons(init, res);
    }
    return res;
}

islisp_val islisp_list(int argc, islisp_val *argv) {
    islisp_val res = ISLISP_NIL;
    for (int i = argc - 1; i >= 0; i--) {
        res = islisp_cons(argv[i], res);
    }
    return res;
}

islisp_val islisp_reverse(islisp_val list) {
    if (!islisp_listp(list)) {
        islisp_error("reverse: argument must be a list");
    }
    islisp_val res = ISLISP_NIL;
    while (IS_CONS(list)) {
        res = islisp_cons(CAR(list), res);
        list = CDR(list);
    }
    return res;
}

islisp_val islisp_nreverse(islisp_val list) {
    if (!islisp_listp(list)) {
        islisp_error("nreverse: argument must be a list");
    }
    islisp_val prev = ISLISP_NIL;
    islisp_val curr = list;
    while (IS_CONS(curr)) {
        islisp_val next = CDR(curr);
        AS_CONS(curr)->cdr = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

islisp_val islisp_append(int argc, islisp_val *argv) {
    if (argc == 0) return ISLISP_NIL;
    if (argc == 1) return argv[0];

    islisp_val head = ISLISP_NIL;
    islisp_val *tail = &head;

    for (int i = 0; i < argc - 1; i++) {
        islisp_val p = argv[i];
        while (IS_CONS(p)) {
            islisp_val new_cell = islisp_cons(CAR(p), ISLISP_NIL);
            *tail = new_cell;
            tail = &(AS_CONS(new_cell)->cdr);
            p = CDR(p);
        }
    }
    *tail = argv[argc - 1];
    return head;
}

islisp_val islisp_member(islisp_val obj, islisp_val list) {
    if (!islisp_listp(list)) {
        islisp_error("member: second argument must be a list");
    }
    while (IS_CONS(list)) {
        if (IS_TRUE(islisp_eql(obj, CAR(list)))) {
            return list;
        }
        list = CDR(list);
    }
    return ISLISP_NIL;
}

islisp_val islisp_assoc(islisp_val obj, islisp_val alist) {
    if (!islisp_listp(alist)) {
        islisp_error("assoc: second argument must be a list");
    }
    while (IS_CONS(alist)) {
        islisp_val pair = CAR(alist);
        if (!IS_CONS(pair)) {
            islisp_error("assoc: element is not a cons");
        }
        if (IS_TRUE(islisp_eql(obj, CAR(pair)))) {
            return pair;
        }
        alist = CDR(alist);
    }
    return ISLISP_NIL;
}

islisp_val islisp_length(islisp_val seq) {
    if (IS_NIL(seq)) return TO_INT(0);
    if (IS_CONS(seq)) {
        int64_t len = 0;
        while (IS_CONS(seq)) {
            len++;
            seq = CDR(seq);
        }
        return TO_INT(len);
    }
    if (IS_VECTOR(seq)) {
        return TO_INT(((islisp_vector_t*)seq)->header.len);
    }
    if (IS_STRING(seq)) {
        return TO_INT(((islisp_string_t*)seq)->header.len);
    }
    islisp_error("length: argument is not a sequence");
    return TO_INT(0);
}

islisp_val islisp_elt(islisp_val seq, islisp_val idx_val) {
    if (!IS_INT(idx_val) || AS_INT(idx_val) < 0) {
        islisp_error("elt: index must be a non-negative integer");
    }
    int64_t idx = AS_INT(idx_val);
    if (IS_CONS(seq)) {
        islisp_val p = seq;
        for (int64_t i = 0; i < idx; i++) {
            if (!IS_CONS(p)) {
                islisp_error("elt: index-out-of-range");
            }
            p = CDR(p);
        }
        if (!IS_CONS(p)) {
            islisp_error("elt: index-out-of-range");
        }
        return CAR(p);
    }
    if (IS_VECTOR(seq)) {
        islisp_vector_t *vec = (islisp_vector_t*)seq;
        if (idx >= vec->header.len) {
            islisp_error("elt: index-out-of-range");
        }
        return vec->data[idx];
    }
    if (IS_STRING(seq)) {
        islisp_string_t *str = (islisp_string_t*)seq;
        if (idx >= str->header.len) {
            islisp_error("elt: index-out-of-range");
        }
        return TO_CHAR((unsigned char)str->data[idx]);
    }
    islisp_error("elt: argument is not a sequence");
    return ISLISP_NIL;
}

islisp_val islisp_set_elt(islisp_val val, islisp_val seq, islisp_val idx_val) {
    if (!IS_INT(idx_val) || AS_INT(idx_val) < 0) {
        islisp_error("set-elt: index must be a non-negative integer");
    }
    int64_t idx = AS_INT(idx_val);
    if (IS_CONS(seq)) {
        islisp_val p = seq;
        for (int64_t i = 0; i < idx; i++) {
            if (!IS_CONS(p)) islisp_error("set-elt: index-out-of-range");
            p = CDR(p);
        }
        if (!IS_CONS(p)) islisp_error("set-elt: index-out-of-range");
        AS_CONS(p)->car = val;
        return val;
    }
    if (IS_VECTOR(seq)) {
        islisp_vector_t *vec = (islisp_vector_t*)seq;
        if (idx >= vec->header.len) islisp_error("set-elt: index-out-of-range");
        vec->data[idx] = val;
        return val;
    }
    if (IS_STRING(seq)) {
        if (!IS_CHAR(val)) islisp_error("set-elt: value must be a character for string sequence");
        islisp_string_t *str = (islisp_string_t*)seq;
        if (idx >= str->header.len) islisp_error("set-elt: index-out-of-range");
        str->data[idx] = (char)AS_CHAR(val);
        return val;
    }
    islisp_error("set-elt: argument is not a sequence");
    return val;
}

islisp_val islisp_subseq(islisp_val seq, islisp_val start_val, islisp_val end_val) {
    if (!IS_INT(start_val) || !IS_INT(end_val)) {
        islisp_error("subseq: start and end must be integers");
    }
    int64_t start = AS_INT(start_val);
    int64_t end = AS_INT(end_val);
    if (start < 0 || end < start) {
        islisp_error("subseq: invalid indices");
    }
    int64_t count = end - start;

    if (IS_CONS(seq) || IS_NIL(seq)) {
        islisp_val p = seq;
        for (int64_t i = 0; i < start; i++) {
            if (!IS_CONS(p)) islisp_error("subseq: index out of range");
            p = CDR(p);
        }
        islisp_val head = ISLISP_NIL;
        islisp_val *tail = &head;
        for (int64_t i = 0; i < count; i++) {
            if (!IS_CONS(p)) islisp_error("subseq: index out of range");
            islisp_val cell = islisp_cons(CAR(p), ISLISP_NIL);
            *tail = cell;
            tail = &(AS_CONS(cell)->cdr);
            p = CDR(p);
        }
        return head;
    }
    if (IS_VECTOR(seq)) {
        islisp_vector_t *vec = (islisp_vector_t*)seq;
        if (end > vec->header.len) islisp_error("subseq: index out of range");
        islisp_val res = islisp_make_vector((int)count, ISLISP_NIL);
        islisp_vector_t *res_v = (islisp_vector_t*)res;
        for (int64_t i = 0; i < count; i++) {
            res_v->data[i] = vec->data[start + i];
        }
        return res;
    }
    if (IS_STRING(seq)) {
        islisp_string_t *str = (islisp_string_t*)seq;
        if (end > str->header.len) islisp_error("subseq: index out of range");
        islisp_val res = islisp_make_string_len(str->data + start, (size_t)count);
        return res;
    }
    islisp_error("subseq: argument is not a sequence");
    return ISLISP_NIL;
}

islisp_val islisp_map_into(int argc, islisp_val *argv) {
    if (argc < 2) islisp_error("map-into: requires at least destination and function");
    islisp_val dest = argv[0];
    islisp_val fn = argv[1];
    int num_seqs = argc - 2;

    int64_t min_len = AS_INT(islisp_length(dest));
    for (int i = 0; i < num_seqs; i++) {
        int64_t l = AS_INT(islisp_length(argv[2 + i]));
        if (l < min_len) min_len = l;
    }

    islisp_val *call_args = (islisp_val*)malloc(sizeof(islisp_val) * (num_seqs > 0 ? num_seqs : 1));
    for (int64_t i = 0; i < min_len; i++) {
        islisp_val idx_val = TO_INT(i);
        for (int j = 0; j < num_seqs; j++) {
            call_args[j] = islisp_elt(argv[2 + j], idx_val);
        }
        islisp_val res = islisp_funcall_argv(fn, num_seqs, call_args);
        islisp_set_elt(res, dest, idx_val);
    }
    free(call_args);
    return dest;
}

/* Mapping Functions */

islisp_val islisp_mapcar(int argc, islisp_val *argv) {
    if (argc < 2) islisp_error("mapcar: requires function and at least one list");
    islisp_val fn = argv[0];
    int num_lists = argc - 1;
    islisp_val *lists = (islisp_val*)malloc(sizeof(islisp_val) * num_lists);
    for (int i = 0; i < num_lists; i++) lists[i] = argv[1 + i];

    islisp_val head = ISLISP_NIL;
    islisp_val *tail = &head;
    islisp_val *args = (islisp_val*)malloc(sizeof(islisp_val) * num_lists);

    while (1) {
        for (int i = 0; i < num_lists; i++) {
            if (!IS_CONS(lists[i])) goto done;
            args[i] = CAR(lists[i]);
            lists[i] = CDR(lists[i]);
        }
        islisp_val val = islisp_funcall_argv(fn, num_lists, args);
        islisp_val cell = islisp_cons(val, ISLISP_NIL);
        *tail = cell;
        tail = &(AS_CONS(cell)->cdr);
    }
done:
    free(lists);
    free(args);
    return head;
}

islisp_val islisp_mapc(int argc, islisp_val *argv) {
    if (argc < 2) islisp_error("mapc: requires function and at least one list");
    islisp_val fn = argv[0];
    int num_lists = argc - 1;
    islisp_val *lists = (islisp_val*)malloc(sizeof(islisp_val) * num_lists);
    for (int i = 0; i < num_lists; i++) lists[i] = argv[1 + i];
    islisp_val *args = (islisp_val*)malloc(sizeof(islisp_val) * num_lists);

    while (1) {
        for (int i = 0; i < num_lists; i++) {
            if (!IS_CONS(lists[i])) goto done;
            args[i] = CAR(lists[i]);
            lists[i] = CDR(lists[i]);
        }
        islisp_funcall_argv(fn, num_lists, args);
    }
done:
    free(lists);
    free(args);
    return argv[1];
}

islisp_val islisp_mapcan(int argc, islisp_val *argv) {
    islisp_val mapped = islisp_mapcar(argc, argv);
    /* nconc result */
    islisp_val head = ISLISP_NIL;
    islisp_val *tail = &head;
    while (IS_CONS(mapped)) {
        islisp_val sub = CAR(mapped);
        while (IS_CONS(sub)) {
            islisp_val cell = islisp_cons(CAR(sub), ISLISP_NIL);
            *tail = cell;
            tail = &(AS_CONS(cell)->cdr);
            sub = CDR(sub);
        }
        mapped = CDR(mapped);
    }
    return head;
}

islisp_val islisp_maplist(int argc, islisp_val *argv) {
    if (argc < 2) islisp_error("maplist: requires function and at least one list");
    islisp_val fn = argv[0];
    int num_lists = argc - 1;
    islisp_val *lists = (islisp_val*)malloc(sizeof(islisp_val) * num_lists);
    for (int i = 0; i < num_lists; i++) lists[i] = argv[1 + i];

    islisp_val head = ISLISP_NIL;
    islisp_val *tail = &head;
    islisp_val *args = (islisp_val*)malloc(sizeof(islisp_val) * num_lists);

    while (1) {
        for (int i = 0; i < num_lists; i++) {
            if (!IS_CONS(lists[i])) goto done;
            args[i] = lists[i];
        }
        islisp_val val = islisp_funcall_argv(fn, num_lists, args);
        islisp_val cell = islisp_cons(val, ISLISP_NIL);
        *tail = cell;
        tail = &(AS_CONS(cell)->cdr);
        for (int i = 0; i < num_lists; i++) {
            lists[i] = CDR(lists[i]);
        }
    }
done:
    free(lists);
    free(args);
    return head;
}

islisp_val islisp_mapl(int argc, islisp_val *argv) {
    if (argc < 2) islisp_error("mapl: requires function and at least one list");
    islisp_val fn = argv[0];
    int num_lists = argc - 1;
    islisp_val *lists = (islisp_val*)malloc(sizeof(islisp_val) * num_lists);
    for (int i = 0; i < num_lists; i++) lists[i] = argv[1 + i];
    islisp_val *args = (islisp_val*)malloc(sizeof(islisp_val) * num_lists);

    while (1) {
        for (int i = 0; i < num_lists; i++) {
            if (!IS_CONS(lists[i])) goto done;
            args[i] = lists[i];
        }
        islisp_funcall_argv(fn, num_lists, args);
        for (int i = 0; i < num_lists; i++) {
            lists[i] = CDR(lists[i]);
        }
    }
done:
    free(lists);
    free(args);
    return argv[1];
}

islisp_val islisp_mapcon(int argc, islisp_val *argv) {
    islisp_val mapped = islisp_maplist(argc, argv);
    islisp_val head = ISLISP_NIL;
    islisp_val *tail = &head;
    while (IS_CONS(mapped)) {
        islisp_val sub = CAR(mapped);
        while (IS_CONS(sub)) {
            islisp_val cell = islisp_cons(CAR(sub), ISLISP_NIL);
            *tail = cell;
            tail = &(AS_CONS(cell)->cdr);
            sub = CDR(sub);
        }
        mapped = CDR(mapped);
    }
    return head;
}
