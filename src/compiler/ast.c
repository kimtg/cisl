#include "islisp_compiler.h"

static int g_var_seq = 1;
static int g_fn_seq = 1;
static int g_block_seq = 1;
static int g_tag_seq = 1;
static int g_catch_seq = 1;
static int g_unwind_seq = 1;

static bool is_lexical_var(comp_env_t *env, islisp_val sym) {
    comp_env_t *e = env;
    while (e) {
        if (IS_TRUE(islisp_member(sym, e->vars))) return true;
        e = e->parent;
    }
    return false;
}

static ast_node_t* alloc_ast(ast_type_t type, islisp_val raw) {
    ast_node_t *n = (ast_node_t*)malloc(sizeof(ast_node_t));
    memset(n, 0, sizeof(ast_node_t));
    n->type = type;
    n->raw_sexpr = raw;
    return n;
}

ast_node_t* islisp_parse_ast(islisp_val form, comp_env_t *env) {
    if (IS_INT(form) || IS_FLOAT(form) || IS_CHAR(form) || IS_STRING(form) || IS_NIL(form) || IS_T(form)) {
        ast_node_t *n = alloc_ast(AST_LITERAL, form);
        n->as.lit.val = form;
        return n;
    }

    if (IS_SYMBOL(form)) {
        ast_node_t *n = alloc_ast(AST_VAR, form);
        n->as.var.sym = form;
        n->as.var.env_idx = -1;
        n->as.var.var_id = is_lexical_var(env, form) ? 1 : 0;
        return n;
    }

    if (IS_CONS(form)) {
        islisp_val op = CAR(form);
        islisp_val args = CDR(form);

        if (op == islisp_intern("quote")) {
            ast_node_t *n = alloc_ast(AST_LITERAL, form);
            n->as.lit.val = CAR(args);
            return n;
        }

        if (op == islisp_intern("if")) {
            ast_node_t *n = alloc_ast(AST_IF, form);
            n->as.if_expr.test = islisp_parse_ast(CAR(args), env);
            n->as.if_expr.then_branch = islisp_parse_ast(CAR(CDR(args)), env);
            if (IS_CONS(CDR(CDR(args)))) {
                n->as.if_expr.else_branch = islisp_parse_ast(CAR(CDR(CDR(args))), env);
            } else {
                n->as.if_expr.else_branch = alloc_ast(AST_LITERAL, ISLISP_NIL);
                n->as.if_expr.else_branch->as.lit.val = ISLISP_NIL;
            }
            return n;
        }

        if (op == islisp_intern("progn")) {
            int count = (int)AS_INT(islisp_length(args));
            ast_node_t *n = alloc_ast(AST_PROGN, form);
            n->as.progn.count = count;
            n->as.progn.exprs = (ast_node_t**)malloc(sizeof(ast_node_t*) * (count > 0 ? count : 1));
            islisp_val p = args;
            for (int i = 0; i < count; i++) {
                n->as.progn.exprs[i] = islisp_parse_ast(CAR(p), env);
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("let") || op == islisp_intern("let*")) {
            bool is_star = (op == islisp_intern("let*"));
            islisp_val bindings = CAR(args);
            islisp_val body = CDR(args);

            int num_bindings = (int)AS_INT(islisp_length(bindings));
            ast_node_t *n = alloc_ast(is_star ? AST_LET_STAR : AST_LET, form);
            n->as.let_expr.is_star = is_star;
            n->as.let_expr.num_bindings = num_bindings;
            n->as.let_expr.vars = (islisp_val*)malloc(sizeof(islisp_val) * (num_bindings > 0 ? num_bindings : 1));
            n->as.let_expr.inits = (ast_node_t**)malloc(sizeof(ast_node_t*) * (num_bindings > 0 ? num_bindings : 1));

            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.vars = env ? env->vars : ISLISP_NIL;
            inner_env.fns = env ? env->fns : ISLISP_NIL;
            inner_env.blocks = env ? env->blocks : ISLISP_NIL;
            inner_env.tags = env ? env->tags : ISLISP_NIL;

            islisp_val bp = bindings;
            for (int i = 0; i < num_bindings; i++) {
                islisp_val b = CAR(bp);
                islisp_val var = CAR(b);
                islisp_val init = CAR(CDR(b));
                n->as.let_expr.vars[i] = var;
                n->as.let_expr.inits[i] = islisp_parse_ast(init, is_star ? &inner_env : env);
                if (is_star) {
                    inner_env.vars = islisp_cons(var, inner_env.vars);
                }
                bp = CDR(bp);
            }

            if (!is_star) {
                for (int i = 0; i < num_bindings; i++) {
                    inner_env.vars = islisp_cons(n->as.let_expr.vars[i], inner_env.vars);
                }
            }

            int body_count = (int)AS_INT(islisp_length(body));
            n->as.let_expr.body_count = body_count;
            n->as.let_expr.body = (ast_node_t**)malloc(sizeof(ast_node_t*) * (body_count > 0 ? body_count : 1));
            islisp_val p = body;
            for (int i = 0; i < body_count; i++) {
                n->as.let_expr.body[i] = islisp_parse_ast(CAR(p), &inner_env);
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("setq")) {
            ast_node_t *n = alloc_ast(AST_SETQ, form);
            n->as.setq.sym = CAR(args);
            n->as.setq.val = islisp_parse_ast(CAR(CDR(args)), env);
            return n;
        }

        if (op == islisp_intern("dynamic")) {
            ast_node_t *n = alloc_ast(AST_DYNAMIC_VAR, form);
            n->as.var.sym = CAR(args);
            return n;
        }

        if (op == islisp_intern("set-dynamic")) {
            ast_node_t *n = alloc_ast(AST_SET_DYNAMIC, form);
            /* (set-dynamic form var) */
            n->as.setq.val = islisp_parse_ast(CAR(args), env);
            n->as.setq.sym = CAR(CDR(args));
            return n;
        }

        if (op == islisp_intern("dynamic-let")) {
            islisp_val bindings = CAR(args);
            islisp_val body = CDR(args);
            int num_b = (int)AS_INT(islisp_length(bindings));
            ast_node_t *n = alloc_ast(AST_DYNAMIC_LET, form);
            n->as.dynamic_let.num_bindings = num_b;
            n->as.dynamic_let.vars = (islisp_val*)malloc(sizeof(islisp_val) * (num_b > 0 ? num_b : 1));
            n->as.dynamic_let.inits = (ast_node_t**)malloc(sizeof(ast_node_t*) * (num_b > 0 ? num_b : 1));

            islisp_val p = bindings;
            for (int i = 0; i < num_b; i++) {
                islisp_val b = CAR(p);
                n->as.dynamic_let.vars[i] = CAR(b);
                n->as.dynamic_let.inits[i] = islisp_parse_ast(CAR(CDR(b)), env);
                p = CDR(p);
            }
            int bcount = (int)AS_INT(islisp_length(body));
            n->as.dynamic_let.body_count = bcount;
            n->as.dynamic_let.body = (ast_node_t**)malloc(sizeof(ast_node_t*) * (bcount > 0 ? bcount : 1));
            p = body;
            for (int i = 0; i < bcount; i++) {
                n->as.dynamic_let.body[i] = islisp_parse_ast(CAR(p), env);
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("lambda")) {
            islisp_val param_spec = CAR(args);
            islisp_val body = CDR(args);

            ast_node_t *n = alloc_ast(AST_LAMBDA, form);
            n->as.lambda.name = ISLISP_NIL;
            n->as.lambda.fn_id = g_fn_seq++;
            n->as.lambda.has_rest = false;
            n->as.lambda.rest_var = ISLISP_NIL;

            /* Parse params */
            int pcount = 0;
            islisp_val p = param_spec;
            while (IS_CONS(p)) {
                if (CAR(p) == SYM_REST || CAR(p) == SYM_COLON_REST) {
                    n->as.lambda.has_rest = true;
                    n->as.lambda.rest_var = CAR(CDR(p));
                    break;
                }
                pcount++;
                p = CDR(p);
            }

            n->as.lambda.num_params = pcount;
            n->as.lambda.param_names = (islisp_val*)malloc(sizeof(islisp_val) * (pcount > 0 ? pcount : 1));
            n->as.lambda.lexical_fns = env ? env->fns : ISLISP_NIL;
            p = param_spec;
            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.is_in_lambda = true;
            inner_env.vars = ISLISP_NIL;
            inner_env.fns = env ? env->fns : ISLISP_NIL;
            inner_env.blocks = env ? env->blocks : ISLISP_NIL;
            inner_env.tags = env ? env->tags : ISLISP_NIL;

            for (int i = 0; i < pcount; i++) {
                n->as.lambda.param_names[i] = CAR(p);
                inner_env.vars = islisp_cons(CAR(p), inner_env.vars);
                p = CDR(p);
            }
            if (n->as.lambda.has_rest) {
                inner_env.vars = islisp_cons(n->as.lambda.rest_var, inner_env.vars);
            }

            int bcount = (int)AS_INT(islisp_length(body));
            n->as.lambda.body_count = bcount;
            n->as.lambda.body = (ast_node_t**)malloc(sizeof(ast_node_t*) * (bcount > 0 ? bcount : 1));
            p = body;
            for (int i = 0; i < bcount; i++) {
                n->as.lambda.body[i] = islisp_parse_ast(CAR(p), &inner_env);
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("function")) {
            islisp_val target = CAR(args);
            if (IS_CONS(target) && CAR(target) == islisp_intern("lambda")) {
                return islisp_parse_ast(target, env);
            }
            ast_node_t *n = alloc_ast(AST_FUNCTION, form);
            n->as.var.sym = target;
            return n;
        }

        if (op == islisp_intern("defun")) {
            islisp_val name = CAR(args);
            islisp_val lambda_form = islisp_cons(islisp_intern("lambda"), CDR(args));
            ast_node_t *n = alloc_ast(AST_DEFUN, form);
            n->as.defun_expr.name = name;
            n->as.defun_expr.lambda_ast = islisp_parse_ast(lambda_form, env);
            n->as.defun_expr.lambda_ast->as.lambda.name = name;
            return n;
        }

        if (op == islisp_intern("defglobal") || op == islisp_intern("defconstant") || op == islisp_intern("defdynamic")) {
            ast_type_t t = (op == islisp_intern("defglobal")) ? AST_DEFGLOBAL :
                           (op == islisp_intern("defconstant")) ? AST_DEFCONSTANT : AST_DEFDYNAMIC;
            ast_node_t *n = alloc_ast(t, form);
            n->as.defglobal.name = CAR(args);
            n->as.defglobal.val_sexpr = CAR(CDR(args));
            return n;
        }

        if (op == islisp_intern("block")) {
            islisp_val name = CAR(args);
            islisp_val body = CDR(args);
            ast_node_t *n = alloc_ast(AST_BLOCK, form);
            n->as.block.name = name;
            n->as.block.block_id = g_block_seq++;

            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.blocks = islisp_cons(name, env ? env->blocks : ISLISP_NIL);
            inner_env.vars = env ? env->vars : ISLISP_NIL;
            inner_env.fns = env ? env->fns : ISLISP_NIL;
            inner_env.tags = env ? env->tags : ISLISP_NIL;

            int bcount = (int)AS_INT(islisp_length(body));
            n->as.block.body_count = bcount;
            n->as.block.body = (ast_node_t**)malloc(sizeof(ast_node_t*) * (bcount > 0 ? bcount : 1));
            islisp_val p = body;
            for (int i = 0; i < bcount; i++) {
                n->as.block.body[i] = islisp_parse_ast(CAR(p), &inner_env);
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("return-from")) {
            islisp_val name = CAR(args);
            islisp_val val_form = IS_CONS(CDR(args)) ? CAR(CDR(args)) : ISLISP_NIL;
            ast_node_t *n = alloc_ast(AST_RETURN_FROM, form);
            n->as.return_from.name = name;
            n->as.return_from.val = islisp_parse_ast(val_form, env);
            return n;
        }

        if (op == islisp_intern("tagbody")) {
            ast_node_t *n = alloc_ast(AST_TAGBODY, form);
            int tbid = g_tag_seq++;
            n->as.tagbody.tagbody_id = tbid;
            int count = (int)AS_INT(islisp_length(args));
            n->as.tagbody.count = count;
            n->as.tagbody.tags = (islisp_val*)malloc(sizeof(islisp_val) * (count > 0 ? count : 1));
            n->as.tagbody.items = (ast_node_t**)malloc(sizeof(ast_node_t*) * (count > 0 ? count : 1));

            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.vars = env ? env->vars : ISLISP_NIL;
            inner_env.fns = env ? env->fns : ISLISP_NIL;
            inner_env.blocks = env ? env->blocks : ISLISP_NIL;
            inner_env.tags = env ? env->tags : ISLISP_NIL;

            islisp_val p = args;
            for (int i = 0; i < count; i++) {
                islisp_val item = CAR(p);
                if (IS_SYMBOL(item)) {
                    inner_env.tags = islisp_cons(islisp_cons(item, TO_INT(tbid)), inner_env.tags);
                }
                p = CDR(p);
            }

            p = args;
            for (int i = 0; i < count; i++) {
                islisp_val item = CAR(p);
                if (IS_SYMBOL(item)) {
                    n->as.tagbody.tags[i] = item;
                    n->as.tagbody.items[i] = NULL;
                } else {
                    n->as.tagbody.tags[i] = ISLISP_NIL;
                    n->as.tagbody.items[i] = islisp_parse_ast(item, &inner_env);
                }
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("go")) {
            ast_node_t *n = alloc_ast(AST_GO, form);
            islisp_val tag = CAR(args);
            n->as.go.tag = tag;
            n->as.go.target_tagbody_id = 0;
            comp_env_t *curr = env;
            while (curr) {
                islisp_val entry = islisp_assoc(tag, curr->tags);
                if (IS_CONS(entry)) {
                    n->as.go.target_tagbody_id = (int)AS_INT(CDR(entry));
                    break;
                }
                curr = curr->parent;
            }
            return n;
        }

        if (op == islisp_intern("catch")) {
            ast_node_t *n = alloc_ast(AST_CATCH, form);
            n->as.catch_expr.catch_id = g_catch_seq++;
            n->as.catch_expr.tag = islisp_parse_ast(CAR(args), env);
            islisp_val body = CDR(args);
            int bcount = (int)AS_INT(islisp_length(body));
            n->as.catch_expr.count = bcount;
            n->as.catch_expr.body = (ast_node_t**)malloc(sizeof(ast_node_t*) * (bcount > 0 ? bcount : 1));
            islisp_val p = body;
            for (int i = 0; i < bcount; i++) {
                n->as.catch_expr.body[i] = islisp_parse_ast(CAR(p), env);
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("throw")) {
            ast_node_t *n = alloc_ast(AST_THROW, form);
            n->as.throw_expr.tag = islisp_parse_ast(CAR(args), env);
            n->as.throw_expr.val = islisp_parse_ast(CAR(CDR(args)), env);
            return n;
        }

        if (op == islisp_intern("unwind-protect")) {
            ast_node_t *n = alloc_ast(AST_UNWIND_PROTECT, form);
            n->as.unwind.unwind_id = g_unwind_seq++;
            n->as.unwind.protected_expr = islisp_parse_ast(CAR(args), env);
            islisp_val cleanups = CDR(args);
            int ccount = (int)AS_INT(islisp_length(cleanups));
            n->as.unwind.cleanup_count = ccount;
            n->as.unwind.cleanups = (ast_node_t**)malloc(sizeof(ast_node_t*) * (ccount > 0 ? ccount : 1));
            islisp_val p = cleanups;
            for (int i = 0; i < ccount; i++) {
                n->as.unwind.cleanups[i] = islisp_parse_ast(CAR(p), env);
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("with-handler")) {
            ast_node_t *n = alloc_ast(AST_WITH_HANDLER, form);
            n->as.with_handler.handler = islisp_parse_ast(CAR(args), env);
            islisp_val body = CDR(args);
            int bcount = (int)AS_INT(islisp_length(body));
            n->as.with_handler.body_count = bcount;
            n->as.with_handler.body = (ast_node_t**)malloc(sizeof(ast_node_t*) * (bcount > 0 ? bcount : 1));
            islisp_val p = body;
            for (int i = 0; i < bcount; i++) {
                n->as.with_handler.body[i] = islisp_parse_ast(CAR(p), env);
                p = CDR(p);
            }
            return n;
        }

        if (op == islisp_intern("defclass")) {
            ast_node_t *n = alloc_ast(AST_DEFCLASS, form);
            n->as.defclass_expr.name = CAR(args);
            n->as.defclass_expr.supers = CAR(CDR(args));
            n->as.defclass_expr.slot_specs = CAR(CDR(CDR(args)));
            n->as.defclass_expr.is_abstract = false;
            return n;
        }

        if (op == islisp_intern("defgeneric")) {
            ast_node_t *n = alloc_ast(AST_DEFGENERIC, form);
            n->as.defgeneric_expr.name = CAR(args);
            islisp_val lambda_list = CAR(CDR(args));
            int num_req = 0;
            bool has_rest = false;
            islisp_val p = lambda_list;
            while (IS_CONS(p)) {
                if (CAR(p) == SYM_REST || CAR(p) == SYM_COLON_REST) {
                    has_rest = true;
                    break;
                }
                num_req++;
                p = CDR(p);
            }
            n->as.defgeneric_expr.num_req = num_req;
            n->as.defgeneric_expr.has_rest = has_rest;
            return n;
        }

        if (op == islisp_intern("defmethod")) {
            ast_node_t *n = alloc_ast(AST_DEFMETHOD, form);
            islisp_val name = CAR(args);
            islisp_val rem = CDR(args);
            islisp_method_qualifier_t qual = METHOD_PRIMARY;
            if (IS_SYMBOL(CAR(rem))) {
                islisp_val qsym = CAR(rem);
                if (qsym == islisp_intern(":before")) qual = METHOD_BEFORE;
                else if (qsym == islisp_intern(":after")) qual = METHOD_AFTER;
                else if (qsym == islisp_intern(":around")) qual = METHOD_AROUND;
                rem = CDR(rem);
            }
            islisp_val param_profile = CAR(rem);
            islisp_val body = CDR(rem);

            islisp_val specializers = ISLISP_NIL;
            islisp_val *s_tail = &specializers;
            islisp_val clean_params = ISLISP_NIL;
            islisp_val *cp_tail = &clean_params;
            bool has_rest = false;

            islisp_val p = param_profile;
            while (IS_CONS(p)) {
                islisp_val p_elem = CAR(p);
                if (p_elem == SYM_REST || p_elem == SYM_COLON_REST) {
                    has_rest = true;
                    *cp_tail = islisp_cons(p_elem, CDR(p));
                    break;
                }
                if (IS_CONS(p_elem)) {
                    islisp_val var = CAR(p_elem);
                    islisp_val spec_class = CAR(CDR(p_elem));
                    *cp_tail = islisp_cons(var, ISLISP_NIL);
                    cp_tail = &(AS_CONS(*cp_tail)->cdr);
                    *s_tail = islisp_cons(spec_class, ISLISP_NIL);
                    s_tail = &(AS_CONS(*s_tail)->cdr);
                } else {
                    *cp_tail = islisp_cons(p_elem, ISLISP_NIL);
                    cp_tail = &(AS_CONS(*cp_tail)->cdr);
                    *s_tail = islisp_cons(islisp_intern("<object>"), ISLISP_NIL);
                    s_tail = &(AS_CONS(*s_tail)->cdr);
                }
                p = CDR(p);
            }

            islisp_val lambda_form = islisp_cons(islisp_intern("lambda"),
                                                 islisp_cons(clean_params, body));
            n->as.defmethod_expr.name = name;
            n->as.defmethod_expr.qual = qual;
            n->as.defmethod_expr.specializers = specializers;
            n->as.defmethod_expr.has_rest = has_rest;
            n->as.defmethod_expr.method_lambda = islisp_parse_ast(lambda_form, env);
            return n;
        }

        if (op == islisp_intern("convert")) {
            ast_node_t *n = alloc_ast(AST_CONVERT, form);
            n->as.convert_expr.obj = islisp_parse_ast(CAR(args), env);
            islisp_val target = CAR(CDR(args));
            if (IS_CONS(target)) {
                islisp_val t_op = CAR(target);
                if (t_op == islisp_intern("class") || t_op == islisp_intern("quote")) {
                    target = CAR(CDR(target));
                }
            }
            n->as.convert_expr.target_class = target;
            return n;
        }

        if (op == islisp_intern("the") || op == islisp_intern("assure")) {
            /* (the class-name form) / (assure class-name form) -> parse form */
            return islisp_parse_ast(CAR(CDR(args)), env);
        }

        if (op == islisp_intern("class")) {
            /* (class class-name) -> returns the class object */
            islisp_val cname = CAR(args);
            ast_node_t *n = alloc_ast(AST_VAR, form);
            n->as.var.sym = cname;
            return n;
        }

        if (op == islisp_intern("flet") || op == islisp_intern("labels")) {
            bool is_labels = (op == islisp_intern("labels"));
            islisp_val defs = CAR(args);
            islisp_val body = CDR(args);
            int num_fns = (int)AS_INT(islisp_length(defs));

            ast_node_t *n = alloc_ast(AST_FLET, form);
            n->as.flet_expr.is_labels = is_labels;
            n->as.flet_expr.num_fns = num_fns;
            n->as.flet_expr.names = (islisp_val*)malloc(sizeof(islisp_val) * (num_fns > 0 ? num_fns : 1));
            n->as.flet_expr.lambdas = (ast_node_t**)malloc(sizeof(ast_node_t*) * (num_fns > 0 ? num_fns : 1));

            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.vars = env ? env->vars : ISLISP_NIL;
            inner_env.fns = env ? env->fns : ISLISP_NIL;
            inner_env.blocks = env ? env->blocks : ISLISP_NIL;
            inner_env.tags = env ? env->tags : ISLISP_NIL;

            if (is_labels) {
                int *fn_ids = (int*)malloc(sizeof(int) * (num_fns > 0 ? num_fns : 1));
                islisp_val p = defs;
                for (int i = 0; i < num_fns; i++) {
                    islisp_val def = CAR(p);
                    islisp_val fname = CAR(def);
                    islisp_val params = CAR(CDR(def));
                    int pcount = 0;
                    bool has_rest = false;
                    islisp_val pp = params;
                    while (IS_CONS(pp)) {
                        if (CAR(pp) == SYM_REST || CAR(pp) == SYM_COLON_REST) {
                            has_rest = true;
                            break;
                        }
                        pcount++;
                        pp = CDR(pp);
                    }
                    fn_ids[i] = g_fn_seq++;
                    int min_a = pcount;
                    int max_a = has_rest ? -1 : pcount;
                    islisp_val entry = islisp_cons(fname,
                                         islisp_cons(TO_INT(fn_ids[i]),
                                           islisp_cons(TO_INT(min_a), TO_INT(max_a))));
                    inner_env.fns = islisp_cons(entry, inner_env.fns);
                    n->as.flet_expr.names[i] = fname;
                    p = CDR(p);
                }

                p = defs;
                for (int i = 0; i < num_fns; i++) {
                    islisp_val def = CAR(p);
                    islisp_val lambda_form = islisp_cons(islisp_intern("lambda"), CDR(def));
                    ast_node_t *lam = islisp_parse_ast(lambda_form, &inner_env);
                    lam->as.lambda.fn_id = fn_ids[i];
                    lam->as.lambda.lexical_fns = inner_env.fns;
                    n->as.flet_expr.lambdas[i] = lam;
                    p = CDR(p);
                }
                free(fn_ids);
            } else {
                islisp_val p = defs;
                for (int i = 0; i < num_fns; i++) {
                    islisp_val def = CAR(p);
                    islisp_val fname = CAR(def);
                    islisp_val lambda_form = islisp_cons(islisp_intern("lambda"), CDR(def));
                    ast_node_t *lam = islisp_parse_ast(lambda_form, env);
                    n->as.flet_expr.names[i] = fname;
                    n->as.flet_expr.lambdas[i] = lam;
                    int min_a = lam->as.lambda.num_params;
                    int max_a = lam->as.lambda.has_rest ? -1 : min_a;
                    islisp_val entry = islisp_cons(fname,
                                         islisp_cons(TO_INT(lam->as.lambda.fn_id),
                                           islisp_cons(TO_INT(min_a), TO_INT(max_a))));
                    inner_env.fns = islisp_cons(entry, inner_env.fns);
                    p = CDR(p);
                }
            }

            int bcount = (int)AS_INT(islisp_length(body));
            n->as.flet_expr.body_count = bcount;
            n->as.flet_expr.body = (ast_node_t**)malloc(sizeof(ast_node_t*) * (bcount > 0 ? bcount : 1));
            islisp_val p = body;
            for (int i = 0; i < bcount; i++) {
                n->as.flet_expr.body[i] = islisp_parse_ast(CAR(p), &inner_env);
                p = CDR(p);
            }
            return n;
        }

        /* Default: function application */
        int argc = (int)AS_INT(islisp_length(args));
        ast_node_t *n = alloc_ast(AST_CALL, form);
        n->as.call.fn = op;
        n->as.call.argc = argc;
        n->as.call.args = (ast_node_t**)malloc(sizeof(ast_node_t*) * (argc > 0 ? argc : 1));
        islisp_val p = args;
        for (int i = 0; i < argc; i++) {
            n->as.call.args[i] = islisp_parse_ast(CAR(p), env);
            p = CDR(p);
        }
        return n;
    }

    ast_node_t *n = alloc_ast(AST_LITERAL, form);
    n->as.lit.val = form;
    return n;
}

void islisp_free_ast(ast_node_t *node) {
    if (!node) return;
    /* Recursively free allocated sub-arrays if needed */
    free(node);
}
