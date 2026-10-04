#include "islisp_compiler.h"

typedef struct macro_entry {
    islisp_val name;
    islisp_val params;
    islisp_val body;
    struct macro_entry *next;
} macro_entry_t;

static macro_entry_t *g_user_macros = NULL;

void islisp_init_macros(void) {
    g_user_macros = NULL;
}

static void register_user_macro(islisp_val name, islisp_val params, islisp_val body) {
    macro_entry_t *m = (macro_entry_t*)malloc(sizeof(macro_entry_t));
    m->name = name;
    m->params = params;
    m->body = body;
    m->next = g_user_macros;
    g_user_macros = m;
}

static macro_entry_t* find_user_macro(islisp_val name) {
    macro_entry_t *m = g_user_macros;
    while (m) {
        if (m->name == name) return m;
        m = m->next;
    }
    return NULL;
}

/* Quasiquote / Backquote Expander */

islisp_val islisp_expand_backquote(islisp_val form) {
    if (!IS_CONS(form)) {
        return islisp_cons(islisp_intern("quote"), islisp_cons(form, ISLISP_NIL));
    }
    islisp_val head = CAR(form);
    if (head == islisp_intern("unquote")) {
        return CAR(CDR(form));
    }
    if (head == islisp_intern("unquote-splicing")) {
        islisp_error("unquote-splicing not in a list context");
        return ISLISP_NIL;
    }

    if (IS_CONS(head) && CAR(head) == islisp_intern("unquote-splicing")) {
        islisp_val spliced = CAR(CDR(head));
        islisp_val rest_exp = islisp_expand_backquote(CDR(form));
        return islisp_cons(islisp_intern("append"),
                           islisp_cons(spliced,
                                       islisp_cons(rest_exp, ISLISP_NIL)));
    }

    islisp_val car_exp = islisp_expand_backquote(head);
    islisp_val cdr_exp = islisp_expand_backquote(CDR(form));
    return islisp_cons(islisp_intern("cons"),
                       islisp_cons(car_exp,
                                   islisp_cons(cdr_exp, ISLISP_NIL)));
}

/* Macro evaluation helper for user-defined macros */
static islisp_val eval_macro_form(islisp_val form, islisp_val env) {
    if (IS_INT(form) || IS_FLOAT(form) || IS_CHAR(form) || IS_STRING(form) || IS_NIL(form) || IS_T(form)) {
        return form;
    }
    if (IS_SYMBOL(form)) {
        /* Lookup in local env */
        islisp_val p = env;
        while (IS_CONS(p)) {
            islisp_val binding = CAR(p);
            if (CAR(binding) == form) return CDR(binding);
            p = CDR(p);
        }
        return islisp_get_global(form);
    }
    if (IS_CONS(form)) {
        islisp_val op = CAR(form);
        islisp_val args = CDR(form);
        if (op == islisp_intern("quote")) {
            return CAR(args);
        }
        if (op == islisp_intern("list")) {
            islisp_val res = ISLISP_NIL;
            islisp_val *tail = &res;
            islisp_val p = args;
            while (IS_CONS(p)) {
                islisp_val v = eval_macro_form(CAR(p), env);
                islisp_val cell = islisp_cons(v, ISLISP_NIL);
                *tail = cell;
                tail = &(AS_CONS(cell)->cdr);
                p = CDR(p);
            }
            return res;
        }
        if (op == islisp_intern("cons")) {
            islisp_val a = eval_macro_form(CAR(args), env);
            islisp_val b = eval_macro_form(CAR(CDR(args)), env);
            return islisp_cons(a, b);
        }
        if (op == islisp_intern("car")) {
            return islisp_car(eval_macro_form(CAR(args), env));
        }
        if (op == islisp_intern("cdr")) {
            return islisp_cdr(eval_macro_form(CAR(args), env));
        }
        if (op == islisp_intern("append")) {
            int argc = 0;
            islisp_val p = args;
            while (IS_CONS(p)) { argc++; p = CDR(p); }
            islisp_val *argv = (islisp_val*)malloc(sizeof(islisp_val) * (argc > 0 ? argc : 1));
            p = args;
            for (int i = 0; i < argc; i++) {
                argv[i] = eval_macro_form(CAR(p), env);
                p = CDR(p);
            }
            islisp_val res = islisp_append(argc, argv);
            free(argv);
            return res;
        }
        if (op == islisp_intern("quasiquote")) {
            islisp_val expanded = islisp_expand_backquote(CAR(args));
            return eval_macro_form(expanded, env);
        }
        if (op == islisp_intern("progn")) {
            islisp_val res = ISLISP_NIL;
            islisp_val p = args;
            while (IS_CONS(p)) {
                res = eval_macro_form(CAR(p), env);
                p = CDR(p);
            }
            return res;
        }
        /* Fallback: funcall if runtime function */
        islisp_val fn = islisp_get_function(op);
        if (IS_CLOSURE(fn)) {
            int argc = 0;
            islisp_val p = args;
            while (IS_CONS(p)) { argc++; p = CDR(p); }
            islisp_val *argv = (islisp_val*)malloc(sizeof(islisp_val) * (argc > 0 ? argc : 1));
            p = args;
            for (int i = 0; i < argc; i++) {
                argv[i] = eval_macro_form(CAR(p), env);
                p = CDR(p);
            }
            islisp_val res = islisp_funcall_argv(fn, argc, argv);
            free(argv);
            return res;
        }
    }
    return form;
}

static islisp_val expand_user_macro(macro_entry_t *m, islisp_val args) {
    islisp_val env = ISLISP_NIL;
    islisp_val p_spec = m->params;
    islisp_val a = args;
    while (IS_CONS(p_spec)) {
        islisp_val param = CAR(p_spec);
        if (param == SYM_REST || param == SYM_COLON_REST) {
            islisp_val rest_var = CAR(CDR(p_spec));
            env = islisp_cons(islisp_cons(rest_var, a), env);
            break;
        }
        islisp_val arg_val = IS_CONS(a) ? CAR(a) : ISLISP_NIL;
        env = islisp_cons(islisp_cons(param, arg_val), env);
        p_spec = CDR(p_spec);
        if (IS_CONS(a)) a = CDR(a);
    }
    islisp_val res = ISLISP_NIL;
    islisp_val b = m->body;
    while (IS_CONS(b)) {
        res = eval_macro_form(CAR(b), env);
        b = CDR(b);
    }
    return res;
}

/* Standard Macro Transformers */

static islisp_val expand_and(islisp_val args) {
    if (IS_NIL(args)) return ISLISP_T;
    if (IS_NIL(CDR(args))) return CAR(args);
    return islisp_cons(islisp_intern("if"),
                       islisp_cons(CAR(args),
                                   islisp_cons(expand_and(CDR(args)),
                                               islisp_cons(ISLISP_NIL, ISLISP_NIL))));
}

static islisp_val expand_or(islisp_val args) {
    if (IS_NIL(args)) return ISLISP_NIL;
    if (IS_NIL(CDR(args))) return CAR(args);
    islisp_val tmp = islisp_gensym();
    return islisp_cons(islisp_intern("let"),
                       islisp_cons(islisp_cons(islisp_cons(tmp, islisp_cons(CAR(args), ISLISP_NIL)), ISLISP_NIL),
                                   islisp_cons(islisp_cons(islisp_intern("if"),
                                                           islisp_cons(tmp,
                                                                       islisp_cons(tmp,
                                                                                   islisp_cons(expand_or(CDR(args)), ISLISP_NIL)))),
                                               ISLISP_NIL)));
}

static islisp_val expand_cond(islisp_val clauses) {
    if (IS_NIL(clauses)) return ISLISP_NIL;
    islisp_val clause = CAR(clauses);
    if (!IS_CONS(clause)) islisp_error("cond: clause must be a list");
    islisp_val test = CAR(clause);
    islisp_val body = CDR(clause);
    if (IS_NIL(body)) {
        islisp_val tmp = islisp_gensym();
        return islisp_cons(islisp_intern("let"),
                           islisp_cons(islisp_cons(islisp_cons(tmp, islisp_cons(test, ISLISP_NIL)), ISLISP_NIL),
                                       islisp_cons(islisp_cons(islisp_intern("if"),
                                                               islisp_cons(tmp,
                                                                           islisp_cons(tmp,
                                                                                       islisp_cons(expand_cond(CDR(clauses)), ISLISP_NIL)))),
                                                   ISLISP_NIL)));
    }
    islisp_val then_expr = (IS_CONS(body) && IS_NIL(CDR(body))) ? CAR(body) : islisp_cons(islisp_intern("progn"), body);
    return islisp_cons(islisp_intern("if"),
                       islisp_cons(test,
                                   islisp_cons(then_expr,
                                               islisp_cons(expand_cond(CDR(clauses)), ISLISP_NIL))));
}

static islisp_val expand_case(islisp_val args, islisp_val pred) {
    if (!IS_CONS(args)) islisp_error("case: missing keyform");
    islisp_val keyform = CAR(args);
    islisp_val clauses = CDR(args);
    islisp_val key_var = islisp_gensym();

    islisp_val cond_clauses = ISLISP_NIL;
    islisp_val *cond_tail = &cond_clauses;

    islisp_val c = clauses;
    while (IS_CONS(c)) {
        islisp_val clause = CAR(c);
        islisp_val keys = CAR(clause);
        islisp_val body = CDR(clause);

        if (keys == SYM_T || keys == ISLISP_T) {
            islisp_val cond_clause = islisp_cons(ISLISP_T, body);
            islisp_val cell = islisp_cons(cond_clause, ISLISP_NIL);
            *cond_tail = cell;
            cond_tail = &(AS_CONS(cell)->cdr);
            break;
        }

        islisp_val or_args = ISLISP_NIL;
        islisp_val *or_tail = &or_args;
        islisp_val k = keys;
        while (IS_CONS(k)) {
            islisp_val key_val = CAR(k);
            islisp_val test_call;
            if (pred == ISLISP_NIL) {
                test_call = islisp_cons(islisp_intern("eql"),
                                        islisp_cons(key_var,
                                                    islisp_cons(islisp_cons(islisp_intern("quote"),
                                                                            islisp_cons(key_val, ISLISP_NIL)),
                                                                ISLISP_NIL)));
            } else {
                test_call = islisp_cons(islisp_intern("funcall"),
                                        islisp_cons(pred,
                                                    islisp_cons(key_var,
                                                                islisp_cons(islisp_cons(islisp_intern("quote"),
                                                                                        islisp_cons(key_val, ISLISP_NIL)),
                                                                            ISLISP_NIL))));
            }
            islisp_val cell = islisp_cons(test_call, ISLISP_NIL);
            *or_tail = cell;
            or_tail = &(AS_CONS(cell)->cdr);
            k = CDR(k);
        }
        islisp_val or_expr = islisp_cons(islisp_intern("or"), or_args);
        islisp_val cond_clause = islisp_cons(or_expr, body);
        islisp_val cell = islisp_cons(cond_clause, ISLISP_NIL);
        *cond_tail = cell;
        cond_tail = &(AS_CONS(cell)->cdr);
        c = CDR(c);
    }

    return islisp_cons(islisp_intern("let"),
                       islisp_cons(islisp_cons(islisp_cons(key_var, islisp_cons(keyform, ISLISP_NIL)), ISLISP_NIL),
                                   islisp_cons(islisp_cons(islisp_intern("cond"), cond_clauses),
                                               ISLISP_NIL)));
}

static islisp_val expand_while(islisp_val args) {
    if (!IS_CONS(args)) islisp_error("while: missing test");
    islisp_val test = CAR(args);
    islisp_val body = CDR(args);
    islisp_val tag_start = islisp_gensym();
    islisp_val tag_end = islisp_gensym();

    islisp_val tagbody_items = ISLISP_NIL;
    islisp_val *tail = &tagbody_items;

    /* tag_start */
    *tail = islisp_cons(tag_start, ISLISP_NIL);
    tail = &(AS_CONS(*tail)->cdr);

    /* (if (not test) (go tag_end)) */
    islisp_val exit_if = islisp_cons(islisp_intern("if"),
                                     islisp_cons(islisp_cons(islisp_intern("not"), islisp_cons(test, ISLISP_NIL)),
                                                 islisp_cons(islisp_cons(islisp_intern("go"), islisp_cons(tag_end, ISLISP_NIL)),
                                                             ISLISP_NIL)));
    *tail = islisp_cons(exit_if, ISLISP_NIL);
    tail = &(AS_CONS(*tail)->cdr);

    /* body forms */
    islisp_val b = body;
    while (IS_CONS(b)) {
        *tail = islisp_cons(CAR(b), ISLISP_NIL);
        tail = &(AS_CONS(*tail)->cdr);
        b = CDR(b);
    }

    /* (go tag_start) */
    *tail = islisp_cons(islisp_cons(islisp_intern("go"), islisp_cons(tag_start, ISLISP_NIL)), ISLISP_NIL);
    tail = &(AS_CONS(*tail)->cdr);

    /* tag_end */
    *tail = islisp_cons(tag_end, ISLISP_NIL);

    return islisp_cons(islisp_intern("tagbody"), tagbody_items);
}

static islisp_val expand_for(islisp_val args) {
    if (!IS_CONS(args) || !IS_CONS(CDR(args))) islisp_error("for: malformed syntax");
    islisp_val iter_specs = CAR(args);
    islisp_val end_spec = CAR(CDR(args));
    islisp_val body = CDR(CDR(args));

    islisp_val end_test = CAR(end_spec);
    islisp_val result_forms = CDR(end_spec);

    islisp_val tag_start = islisp_gensym();
    islisp_val tag_end = islisp_gensym();
    islisp_val res_var = islisp_gensym();

    /* Initial let bindings: ((res_var nil) (var init) ...) */
    islisp_val let_bindings = ISLISP_NIL;
    islisp_val *lb_tail = &let_bindings;

    *lb_tail = islisp_cons(islisp_cons(res_var, islisp_cons(ISLISP_NIL, ISLISP_NIL)), ISLISP_NIL);
    lb_tail = &(AS_CONS(*lb_tail)->cdr);

    /* Step variables and updates */
    islisp_val step_let_bindings = ISLISP_NIL;
    islisp_val *slb_tail = &step_let_bindings;
    islisp_val setq_forms = ISLISP_NIL;
    islisp_val *sq_tail = &setq_forms;

    islisp_val sp = iter_specs;
    while (IS_CONS(sp)) {
        islisp_val spec = CAR(sp);
        islisp_val var = CAR(spec);
        islisp_val init = CAR(CDR(spec));
        islisp_val step = IS_CONS(CDR(CDR(spec))) ? CAR(CDR(CDR(spec))) : var;

        *lb_tail = islisp_cons(islisp_cons(var, islisp_cons(init, ISLISP_NIL)), ISLISP_NIL);
        lb_tail = &(AS_CONS(*lb_tail)->cdr);

        islisp_val tmp_step = islisp_gensym();
        *slb_tail = islisp_cons(islisp_cons(tmp_step, islisp_cons(step, ISLISP_NIL)), ISLISP_NIL);
        slb_tail = &(AS_CONS(*slb_tail)->cdr);

        *sq_tail = islisp_cons(islisp_cons(islisp_intern("setq"),
                                           islisp_cons(var, islisp_cons(tmp_step, ISLISP_NIL))),
                               ISLISP_NIL);
        sq_tail = &(AS_CONS(*sq_tail)->cdr);

        sp = CDR(sp);
    }

    islisp_val tagbody_items = ISLISP_NIL;
    islisp_val *tb_tail = &tagbody_items;

    /* tag_start */
    *tb_tail = islisp_cons(tag_start, ISLISP_NIL);
    tb_tail = &(AS_CONS(*tb_tail)->cdr);

    /* (if end_test (progn (setq res_var (progn result_forms...)) (go tag_end))) */
    islisp_val res_expr = islisp_cons(islisp_intern("progn"), result_forms);
    islisp_val set_res = islisp_cons(islisp_intern("setq"), islisp_cons(res_var, islisp_cons(res_expr, ISLISP_NIL)));
    islisp_val go_end = islisp_cons(islisp_intern("go"), islisp_cons(tag_end, ISLISP_NIL));
    islisp_val exit_if = islisp_cons(islisp_intern("if"),
                                     islisp_cons(end_test,
                                                 islisp_cons(islisp_cons(islisp_intern("progn"),
                                                                         islisp_cons(set_res,
                                                                                     islisp_cons(go_end, ISLISP_NIL))),
                                                             ISLISP_NIL)));
    *tb_tail = islisp_cons(exit_if, ISLISP_NIL);
    tb_tail = &(AS_CONS(*tb_tail)->cdr);

    /* body forms */
    islisp_val b = body;
    while (IS_CONS(b)) {
        *tb_tail = islisp_cons(CAR(b), ISLISP_NIL);
        tb_tail = &(AS_CONS(*tb_tail)->cdr);
        b = CDR(b);
    }

    /* step updates */
    islisp_val step_block = islisp_cons(islisp_intern("let"),
                                        islisp_cons(step_let_bindings, setq_forms));
    *tb_tail = islisp_cons(step_block, ISLISP_NIL);
    tb_tail = &(AS_CONS(*tb_tail)->cdr);

    /* (go tag_start) */
    *tb_tail = islisp_cons(islisp_cons(islisp_intern("go"), islisp_cons(tag_start, ISLISP_NIL)), ISLISP_NIL);
    tb_tail = &(AS_CONS(*tb_tail)->cdr);

    /* tag_end */
    *tb_tail = islisp_cons(tag_end, ISLISP_NIL);

    islisp_val tagbody = islisp_cons(islisp_intern("tagbody"), tagbody_items);
    islisp_val let_form = islisp_cons(islisp_intern("let"),
                                      islisp_cons(let_bindings,
                                                  islisp_cons(tagbody,
                                                              islisp_cons(res_var, ISLISP_NIL))));

    return islisp_cons(islisp_intern("block"),
                       islisp_cons(SYM_NIL, islisp_cons(let_form, ISLISP_NIL)));
}

static islisp_val expand_setf(islisp_val args) {
    if (!IS_CONS(args) || !IS_CONS(CDR(args))) islisp_error("setf: requires place and value");
    islisp_val place = CAR(args);
    islisp_val val = CAR(CDR(args));

    if (IS_SYMBOL(place)) {
        return islisp_cons(islisp_intern("setq"), islisp_cons(place, islisp_cons(val, ISLISP_NIL)));
    }
    if (IS_CONS(place)) {
        islisp_val p_op = CAR(place);
        islisp_val p_args = CDR(place);
        if (p_op == islisp_intern("car")) {
            return islisp_cons(islisp_intern("set-car"), islisp_cons(val, p_args));
        }
        if (p_op == islisp_intern("cdr")) {
            return islisp_cons(islisp_intern("set-cdr"), islisp_cons(val, p_args));
        }
        if (p_op == islisp_intern("elt")) {
            return islisp_cons(islisp_intern("set-elt"), islisp_cons(val, p_args));
        }
        if (p_op == islisp_intern("aref")) {
            return islisp_cons(islisp_intern("set-aref"), islisp_cons(val, p_args));
        }
        if (p_op == islisp_intern("garef")) {
            return islisp_cons(islisp_intern("set-garef"), islisp_cons(val, p_args));
        }
        if (p_op == islisp_intern("property")) {
            /* (setf (property sym prop) val) -> (set-property val sym prop) */
            return islisp_cons(islisp_intern("set-property"), islisp_cons(val, p_args));
        }
        if (p_op == islisp_intern("dynamic")) {
            return islisp_cons(islisp_intern("set-dynamic"), islisp_cons(val, p_args));
        }
        if (p_op == islisp_intern("slot-value")) {
            return islisp_cons(islisp_intern("set-slot-value"), islisp_cons(val, p_args));
        }
        /* Generic writer function call ((setf p_op) val p_args...) */
        islisp_val writer_sym = islisp_cons(islisp_intern("setf"), islisp_cons(p_op, ISLISP_NIL));
        return islisp_cons(writer_sym, islisp_cons(val, p_args));
    }
    islisp_error("setf: invalid place");
    return ISLISP_NIL;
}

/* Recursive Macro Expander */

islisp_val islisp_macroexpand(islisp_val form) {
    if (!IS_CONS(form)) return form;
    islisp_val op = CAR(form);
    islisp_val args = CDR(form);

    if (op == islisp_intern("defmacro")) {
        islisp_val name = CAR(args);
        islisp_val params = CAR(CDR(args));
        islisp_val body = CDR(CDR(args));
        register_user_macro(name, params, body);
        return islisp_cons(islisp_intern("quote"), islisp_cons(name, ISLISP_NIL));
    }

    if (op == islisp_intern("quote")) {
        return form;
    }

    if (op == islisp_intern("defun")) {
        islisp_val name = CAR(args);
        islisp_val params = CAR(CDR(args));
        islisp_val body = CDR(CDR(args));
        islisp_val new_body = ISLISP_NIL;
        islisp_val *tail = &new_body;
        islisp_val p = body;
        while (IS_CONS(p)) {
            islisp_val exp = islisp_macroexpand(CAR(p));
            islisp_val cell = islisp_cons(exp, ISLISP_NIL);
            *tail = cell;
            tail = &(AS_CONS(cell)->cdr);
            p = CDR(p);
        }
        return islisp_cons(op, islisp_cons(name, islisp_cons(params, new_body)));
    }

    if (op == islisp_intern("lambda")) {
        islisp_val params = CAR(args);
        islisp_val body = CDR(args);
        islisp_val new_body = ISLISP_NIL;
        islisp_val *tail = &new_body;
        islisp_val p = body;
        while (IS_CONS(p)) {
            islisp_val exp = islisp_macroexpand(CAR(p));
            islisp_val cell = islisp_cons(exp, ISLISP_NIL);
            *tail = cell;
            tail = &(AS_CONS(cell)->cdr);
            p = CDR(p);
        }
        return islisp_cons(op, islisp_cons(params, new_body));
    }

    if (op == islisp_intern("let") || op == islisp_intern("let*")) {
        islisp_val bindings = CAR(args);
        islisp_val body = CDR(args);
        islisp_val new_bindings = ISLISP_NIL;
        islisp_val *b_tail = &new_bindings;
        islisp_val p = bindings;
        while (IS_CONS(p)) {
            islisp_val b = CAR(p);
            islisp_val var = CAR(b);
            islisp_val init = CAR(CDR(b));
            islisp_val exp_init = islisp_macroexpand(init);
            islisp_val cell = islisp_cons(islisp_cons(var, islisp_cons(exp_init, ISLISP_NIL)), ISLISP_NIL);
            *b_tail = cell;
            b_tail = &(AS_CONS(cell)->cdr);
            p = CDR(p);
        }
        islisp_val new_body = ISLISP_NIL;
        islisp_val *tail = &new_body;
        p = body;
        while (IS_CONS(p)) {
            islisp_val exp = islisp_macroexpand(CAR(p));
            islisp_val cell = islisp_cons(exp, ISLISP_NIL);
            *tail = cell;
            tail = &(AS_CONS(cell)->cdr);
            p = CDR(p);
        }
        return islisp_cons(op, islisp_cons(new_bindings, new_body));
    }

    if (op == islisp_intern("block")) {
        islisp_val name = CAR(args);
        islisp_val body = CDR(args);
        islisp_val new_body = ISLISP_NIL;
        islisp_val *tail = &new_body;
        islisp_val p = body;
        while (IS_CONS(p)) {
            islisp_val exp = islisp_macroexpand(CAR(p));
            islisp_val cell = islisp_cons(exp, ISLISP_NIL);
            *tail = cell;
            tail = &(AS_CONS(cell)->cdr);
            p = CDR(p);
        }
        return islisp_cons(op, islisp_cons(name, new_body));
    }

    if (op == islisp_intern("return-from")) {
        islisp_val name = CAR(args);
        islisp_val val = IS_CONS(CDR(args)) ? islisp_macroexpand(CAR(CDR(args))) : ISLISP_NIL;
        return islisp_cons(op, islisp_cons(name, islisp_cons(val, ISLISP_NIL)));
    }

    if (op == islisp_intern("tagbody")) {
        islisp_val new_items = ISLISP_NIL;
        islisp_val *tail = &new_items;
        islisp_val p = args;
        while (IS_CONS(p)) {
            islisp_val item = CAR(p);
            islisp_val exp_item = IS_SYMBOL(item) ? item : islisp_macroexpand(item);
            islisp_val cell = islisp_cons(exp_item, ISLISP_NIL);
            *tail = cell;
            tail = &(AS_CONS(cell)->cdr);
            p = CDR(p);
        }
        return islisp_cons(op, new_items);
    }

    if (op == islisp_intern("go")) {
        return form;
    }

    if (op == islisp_intern("function")) {
        islisp_val target = CAR(args);
        if (IS_CONS(target) && CAR(target) == islisp_intern("lambda")) {
            return islisp_cons(op, islisp_cons(islisp_macroexpand(target), ISLISP_NIL));
        }
        return form;
    }

    if (op == islisp_intern("defglobal") || op == islisp_intern("defconstant") || op == islisp_intern("defdynamic")) {
        islisp_val name = CAR(args);
        islisp_val val = CAR(CDR(args));
        return islisp_cons(op, islisp_cons(name, islisp_cons(islisp_macroexpand(val), ISLISP_NIL)));
    }

    macro_entry_t *user_m = find_user_macro(op);
    if (user_m) {
        islisp_val expanded = expand_user_macro(user_m, args);
        return islisp_macroexpand(expanded);
    }

    if (op == islisp_intern("quasiquote")) {
        return islisp_macroexpand(islisp_expand_backquote(CAR(args)));
    }
    if (op == islisp_intern("and")) {
        return islisp_macroexpand(expand_and(args));
    }
    if (op == islisp_intern("or")) {
        return islisp_macroexpand(expand_or(args));
    }
    if (op == islisp_intern("cond")) {
        return islisp_macroexpand(expand_cond(args));
    }
    if (op == islisp_intern("case")) {
        return islisp_macroexpand(expand_case(args, ISLISP_NIL));
    }
    if (op == islisp_intern("case-using")) {
        return islisp_macroexpand(expand_case(CDR(args), CAR(args)));
    }
    if (op == islisp_intern("while")) {
        return islisp_macroexpand(expand_while(args));
    }
    if (op == islisp_intern("for")) {
        return islisp_macroexpand(expand_for(args));
    }
    if (op == islisp_intern("setf")) {
        return islisp_macroexpand(expand_setf(args));
    }
    if (op == islisp_intern("ignore-errors")) {
        /* (with-handler (lambda (c) nil) (progn body...)) */
        islisp_val handler = islisp_cons(islisp_intern("lambda"),
                                         islisp_cons(islisp_cons(islisp_gensym(), ISLISP_NIL),
                                                     islisp_cons(ISLISP_NIL, ISLISP_NIL)));
        return islisp_macroexpand(islisp_cons(islisp_intern("with-handler"),
                                             islisp_cons(handler, args)));
    }
    if (op == islisp_intern("with-open-input-file")) {
        /* (with-open-input-file (name filename [class]) body...) */
        islisp_val spec = CAR(args);
        islisp_val name = CAR(spec);
        islisp_val filename = CAR(CDR(spec));
        islisp_val elem_class = IS_CONS(CDR(CDR(spec))) ? CAR(CDR(CDR(spec))) : ISLISP_NIL;
        islisp_val body = CDR(args);

        islisp_val open_call = islisp_cons(islisp_intern("open-input-file"),
                                           islisp_cons(filename, islisp_cons(elem_class, ISLISP_NIL)));
        islisp_val cleanup = islisp_cons(islisp_intern("close"), islisp_cons(name, ISLISP_NIL));

        return islisp_macroexpand(
            islisp_cons(islisp_intern("let"),
                        islisp_cons(islisp_cons(islisp_cons(name, islisp_cons(open_call, ISLISP_NIL)), ISLISP_NIL),
                                    islisp_cons(islisp_cons(islisp_intern("unwind-protect"),
                                                            islisp_cons(islisp_cons(islisp_intern("progn"), body),
                                                                        islisp_cons(cleanup, ISLISP_NIL))),
                                                ISLISP_NIL))));
    }
    if (op == islisp_intern("with-open-output-file")) {
        islisp_val spec = CAR(args);
        islisp_val name = CAR(spec);
        islisp_val filename = CAR(CDR(spec));
        islisp_val elem_class = IS_CONS(CDR(CDR(spec))) ? CAR(CDR(CDR(spec))) : ISLISP_NIL;
        islisp_val body = CDR(args);

        islisp_val open_call = islisp_cons(islisp_intern("open-output-file"),
                                           islisp_cons(filename, islisp_cons(elem_class, ISLISP_NIL)));
        islisp_val cleanup = islisp_cons(islisp_intern("close"), islisp_cons(name, ISLISP_NIL));

        return islisp_macroexpand(
            islisp_cons(islisp_intern("let"),
                        islisp_cons(islisp_cons(islisp_cons(name, islisp_cons(open_call, ISLISP_NIL)), ISLISP_NIL),
                                    islisp_cons(islisp_cons(islisp_intern("unwind-protect"),
                                                            islisp_cons(islisp_cons(islisp_intern("progn"), body),
                                                                        islisp_cons(cleanup, ISLISP_NIL))),
                                                ISLISP_NIL))));
    }
    if (op == islisp_intern("with-open-io-file")) {
        islisp_val spec = CAR(args);
        islisp_val name = CAR(spec);
        islisp_val filename = CAR(CDR(spec));
        islisp_val elem_class = IS_CONS(CDR(CDR(spec))) ? CAR(CDR(CDR(spec))) : ISLISP_NIL;
        islisp_val body = CDR(args);

        islisp_val open_call = islisp_cons(islisp_intern("open-io-file"),
                                           islisp_cons(filename, islisp_cons(elem_class, ISLISP_NIL)));
        islisp_val cleanup = islisp_cons(islisp_intern("close"), islisp_cons(name, ISLISP_NIL));

        return islisp_macroexpand(
            islisp_cons(islisp_intern("let"),
                        islisp_cons(islisp_cons(islisp_cons(name, islisp_cons(open_call, ISLISP_NIL)), ISLISP_NIL),
                                    islisp_cons(islisp_cons(islisp_intern("unwind-protect"),
                                                            islisp_cons(islisp_cons(islisp_intern("progn"), body),
                                                                        islisp_cons(cleanup, ISLISP_NIL))),
                                                ISLISP_NIL))));
    }

    /* Subexpressions */
    islisp_val new_head = op;
    islisp_val new_args = ISLISP_NIL;
    islisp_val *tail = &new_args;
    islisp_val p = args;
    while (IS_CONS(p)) {
        islisp_val exp = islisp_macroexpand(CAR(p));
        islisp_val cell = islisp_cons(exp, ISLISP_NIL);
        *tail = cell;
        tail = &(AS_CONS(cell)->cdr);
        p = CDR(p);
    }
    return islisp_cons(new_head, new_args);
}
