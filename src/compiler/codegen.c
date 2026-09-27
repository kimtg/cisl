#include "islisp_compiler.h"
#include <ctype.h>

static int g_lit_count = 0;
static int g_temp_count = 0;

typedef struct lit_node {
    int id;
    islisp_val val;
    char *c_name;
    struct lit_node *next;
} lit_node_t;

static lit_node_t *g_literals = NULL;

static void reset_literals(void) {
    g_literals = NULL;
    g_lit_count = 0;
    g_temp_count = 0;
}

static const char *c_ident(const char *name) {
    static char bufs[8][256];
    static int idx = 0;
    idx = (idx + 1) % 8;
    char *buf = bufs[idx];
    size_t j = 0;
    for (size_t i = 0; name[i] && j < sizeof(bufs[0]) - 5; i++) {
        char ch = name[i];
        if (isalnum((unsigned char)ch) || ch == '_') {
            buf[j++] = ch;
        } else if (ch == '-') {
            buf[j++] = '_';
        } else if (ch == '?') {
            buf[j++] = '_'; buf[j++] = 'p';
        } else if (ch == '!') {
            buf[j++] = '_'; buf[j++] = 'x';
        } else if (ch == '*') {
            buf[j++] = '_'; buf[j++] = 's';
        } else if (ch == '+') {
            buf[j++] = '_'; buf[j++] = 'a';
        } else if (ch == '/') {
            buf[j++] = '_'; buf[j++] = 'd';
        } else if (ch == '<') {
            buf[j++] = '_'; buf[j++] = 'l'; buf[j++] = 't';
        } else if (ch == '>') {
            buf[j++] = '_'; buf[j++] = 'g'; buf[j++] = 't';
        } else if (ch == '=') {
            buf[j++] = '_'; buf[j++] = 'e'; buf[j++] = 'q';
        } else {
            snprintf(buf + j, 5, "_%02x", (unsigned char)ch);
            j += strlen(buf + j);
        }
    }
    buf[j] = '\0';
    return buf;
}

static void register_literal_recursive(islisp_val v) {
    if (IS_NIL(v) || IS_T(v) || IS_INT(v) || IS_FLOAT(v) || IS_CHAR(v) || v == ISLISP_UNBOUND || v == ISLISP_EOF) {
        return;
    }
    lit_node_t *curr = g_literals;
    while (curr) {
        if (curr->val == v) return;
        curr = curr->next;
    }
    if (IS_CONS(v)) {
        register_literal_recursive(CAR(v));
        register_literal_recursive(CDR(v));
    }
    lit_node_t *n = (lit_node_t*)malloc(sizeof(lit_node_t));
    n->id = ++g_lit_count;
    n->val = v;
    char buf[64];
    snprintf(buf, sizeof(buf), "LIT_%d", n->id);
    n->c_name = strdup(buf);
    n->next = NULL;
    if (!g_literals) {
        g_literals = n;
    } else {
        lit_node_t *tail = g_literals;
        while (tail->next) tail = tail->next;
        tail->next = n;
    }
}

static char* get_literal_c_name(islisp_val val) {
    register_literal_recursive(val);
    lit_node_t *curr = g_literals;
    while (curr) {
        if (curr->val == val) return curr->c_name;
        curr = curr->next;
    }
    return "ISLISP_NIL";
}

static const char* get_literal_ref(islisp_val v) {
    if (IS_NIL(v)) return "ISLISP_NIL";
    if (IS_T(v)) return "ISLISP_T";
    if (IS_INT(v)) {
        static char bufs[8][64];
        static int bidx = 0;
        bidx = (bidx + 1) % 8;
        snprintf(bufs[bidx], sizeof(bufs[0]), "TO_INT(%lldLL)", (long long)AS_INT(v));
        return bufs[bidx];
    }
    if (IS_CHAR(v)) {
        static char bufs[8][64];
        static int bidx = 0;
        bidx = (bidx + 1) % 8;
        snprintf(bufs[bidx], sizeof(bufs[0]), "TO_CHAR(%u)", (unsigned int)AS_CHAR(v));
        return bufs[bidx];
    }
    if (IS_FLOAT(v)) {
        static char bufs[8][64];
        static int bidx = 0;
        bidx = (bidx + 1) % 8;
        snprintf(bufs[bidx], sizeof(bufs[0]), "islisp_make_float(%.17g)", ((islisp_float_t*)v)->val);
        return bufs[bidx];
    }
    return get_literal_c_name(v);
}

/* Forward declarations */
static void emit_ast(FILE *f, ast_node_t *n, const char *dest_var, comp_env_t *env);
static void emit_lambda_func(FILE *f, ast_node_t *lam);

/* Pre-pass: collect all literals and symbols before code emission */
static void collect_literals(ast_node_t *n, comp_env_t *env) {
    if (!n) return;
    switch (n->type) {
        case AST_LITERAL: {
            islisp_val v = n->as.lit.val;
            if (!IS_NIL(v) && !IS_T(v) && !IS_INT(v) && !IS_FLOAT(v) && !IS_CHAR(v)) {
                get_literal_c_name(v);
            }
            break;
        }
        case AST_VAR: {
            islisp_val sym = n->as.var.sym;
            if (sym != SYM_T && sym != SYM_NIL && (!env || !IS_TRUE(islisp_member(sym, env->vars)))) {
                get_literal_c_name(sym);
            }
            break;
        }
        case AST_DYNAMIC_VAR:
            get_literal_c_name(n->as.var.sym);
            break;
        case AST_SETQ: {
            islisp_val sym = n->as.setq.sym;
            if (!env || !IS_TRUE(islisp_member(sym, env->vars))) {
                get_literal_c_name(sym);
            }
            collect_literals(n->as.setq.val, env);
            break;
        }
        case AST_SET_DYNAMIC:
            get_literal_c_name(n->as.setq.sym);
            collect_literals(n->as.setq.val, env);
            break;
        case AST_IF:
            collect_literals(n->as.if_expr.test, env);
            collect_literals(n->as.if_expr.then_branch, env);
            collect_literals(n->as.if_expr.else_branch, env);
            break;
        case AST_PROGN:
            for (int i = 0; i < n->as.progn.count; i++) collect_literals(n->as.progn.exprs[i], env);
            break;
        case AST_LET:
        case AST_LET_STAR: {
            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.vars = env ? env->vars : ISLISP_NIL;

            for (int i = 0; i < n->as.let_expr.num_bindings; i++) {
                collect_literals(n->as.let_expr.inits[i], (n->type == AST_LET_STAR) ? &inner_env : env);
                inner_env.vars = islisp_cons(n->as.let_expr.vars[i], inner_env.vars);
            }
            for (int i = 0; i < n->as.let_expr.body_count; i++) {
                collect_literals(n->as.let_expr.body[i], &inner_env);
            }
            break;
        }
        case AST_FLET: {
            for (int i = 0; i < n->as.flet_expr.num_fns; i++) {
                collect_literals(n->as.flet_expr.lambdas[i], env);
            }
            for (int i = 0; i < n->as.flet_expr.body_count; i++) {
                collect_literals(n->as.flet_expr.body[i], env);
            }
            break;
        }
        case AST_LAMBDA: {
            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.vars = ISLISP_NIL;
            for (int i = 0; i < n->as.lambda.num_params; i++) {
                inner_env.vars = islisp_cons(n->as.lambda.param_names[i], inner_env.vars);
            }
            if (n->as.lambda.has_rest) {
                inner_env.vars = islisp_cons(n->as.lambda.rest_var, inner_env.vars);
            }
            for (int i = 0; i < n->as.lambda.body_count; i++) {
                collect_literals(n->as.lambda.body[i], &inner_env);
            }
            break;
        }
        case AST_FUNCTION:
            get_literal_c_name(n->as.var.sym);
            break;
        case AST_DEFUN:
            get_literal_c_name(n->as.defun_expr.name);
            collect_literals(n->as.defun_expr.lambda_ast, env);
            break;
        case AST_DEFGLOBAL:
        case AST_DEFCONSTANT:
        case AST_DEFDYNAMIC: {
            get_literal_c_name(n->as.defglobal.name);
            ast_node_t *val_ast = islisp_parse_ast(n->as.defglobal.val_sexpr, env);
            collect_literals(val_ast, env);
            islisp_free_ast(val_ast);
            break;
        }
        case AST_BLOCK:
            get_literal_c_name(n->as.block.name);
            for (int i = 0; i < n->as.block.body_count; i++) collect_literals(n->as.block.body[i], env);
            break;
        case AST_RETURN_FROM:
            get_literal_c_name(n->as.return_from.name);
            collect_literals(n->as.return_from.val, env);
            break;
        case AST_TAGBODY:
            for (int i = 0; i < n->as.tagbody.count; i++) {
                if (n->as.tagbody.items[i]) collect_literals(n->as.tagbody.items[i], env);
            }
            break;
        case AST_CATCH:
            collect_literals(n->as.catch_expr.tag, env);
            for (int i = 0; i < n->as.catch_expr.count; i++) collect_literals(n->as.catch_expr.body[i], env);
            break;
        case AST_THROW:
            collect_literals(n->as.throw_expr.tag, env);
            collect_literals(n->as.throw_expr.val, env);
            break;
        case AST_UNWIND_PROTECT:
            collect_literals(n->as.unwind.protected_expr, env);
            for (int i = 0; i < n->as.unwind.cleanup_count; i++) collect_literals(n->as.unwind.cleanups[i], env);
            break;

        case AST_DYNAMIC_LET:
            for (int i = 0; i < n->as.dynamic_let.num_bindings; i++) {
                get_literal_c_name(n->as.dynamic_let.vars[i]);
                collect_literals(n->as.dynamic_let.inits[i], env);
            }
            for (int i = 0; i < n->as.dynamic_let.body_count; i++) collect_literals(n->as.dynamic_let.body[i], env);
            break;
        case AST_WITH_HANDLER:
            collect_literals(n->as.with_handler.handler, env);
            for (int i = 0; i < n->as.with_handler.body_count; i++) collect_literals(n->as.with_handler.body[i], env);
            break;
        case AST_DEFCLASS:
            get_literal_c_name(n->as.defclass_expr.name);
            get_literal_c_name(n->as.defclass_expr.supers);
            get_literal_c_name(n->as.defclass_expr.slot_specs);
            break;
        case AST_DEFGENERIC:
            get_literal_c_name(n->as.defgeneric_expr.name);
            break;
        case AST_DEFMETHOD:
            get_literal_c_name(n->as.defmethod_expr.name);
            get_literal_c_name(n->as.defmethod_expr.specializers);
            collect_literals(n->as.defmethod_expr.method_lambda, env);
            break;
        case AST_CONVERT:
            collect_literals(n->as.convert_expr.obj, env);
            get_literal_c_name(n->as.convert_expr.target_class);
            break;
        case AST_CALL:
            if (IS_SYMBOL(n->as.call.fn)) {
                get_literal_c_name(n->as.call.fn);
            }
            for (int i = 0; i < n->as.call.argc; i++) collect_literals(n->as.call.args[i], env);
            break;
        default:
            break;
    }
}

/* Emit C expression value into dest_var */
static void emit_ast(FILE *f, ast_node_t *n, const char *dest_var, comp_env_t *env) {
    if (!n) {
        if (dest_var) fprintf(f, "    %s = ISLISP_NIL;\n", dest_var);
        return;
    }

    switch (n->type) {
        case AST_LITERAL: {
            islisp_val v = n->as.lit.val;
            if (IS_NIL(v)) {
                if (dest_var) fprintf(f, "    %s = ISLISP_NIL;\n", dest_var);
            } else if (IS_T(v)) {
                if (dest_var) fprintf(f, "    %s = ISLISP_T;\n", dest_var);
            } else if (IS_INT(v)) {
                if (dest_var) fprintf(f, "    %s = TO_INT(%lldLL);\n", dest_var, (long long)AS_INT(v));
            } else if (IS_FLOAT(v)) {
                if (dest_var) fprintf(f, "    %s = islisp_make_float(%.17g);\n", dest_var, ((islisp_float_t*)v)->val);
            } else if (IS_CHAR(v)) {
                if (dest_var) fprintf(f, "    %s = TO_CHAR(%d);\n", dest_var, AS_CHAR(v));
            } else {
                char *lit_name = get_literal_c_name(v);
                if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, lit_name);
            }
            break;
        }

        case AST_VAR: {
            islisp_val sym = n->as.var.sym;
            const char *sym_name = IS_SYMBOL(sym) ? ((islisp_symbol_t*)sym)->name : "unknown";
            if (sym == SYM_T) {
                if (dest_var) fprintf(f, "    %s = ISLISP_T;\n", dest_var);
            } else if (sym == SYM_NIL) {
                if (dest_var) fprintf(f, "    %s = ISLISP_NIL;\n", dest_var);
            } else if (env && IS_TRUE(islisp_member(sym, env->vars))) {
                if (dest_var) fprintf(f, "    %s = V_%s;\n", dest_var, c_ident(sym_name));
            } else {
                char *lit_name = get_literal_c_name(sym);
                if (dest_var) fprintf(f, "    %s = islisp_get_global(%s);\n", dest_var, lit_name);
            }
            break;
        }

        case AST_DYNAMIC_VAR: {
            char *lit_name = get_literal_c_name(n->as.var.sym);
            if (dest_var) fprintf(f, "    %s = islisp_get_dynamic(%s);\n", dest_var, lit_name);
            break;
        }

        case AST_SETQ: {
            islisp_val sym = n->as.setq.sym;
            const char *sym_name = IS_SYMBOL(sym) ? ((islisp_symbol_t*)sym)->name : "unknown";
            int t_id = ++g_temp_count;
            char val_var[32];
            snprintf(val_var, sizeof(val_var), "tmp_%d", t_id);
            fprintf(f, "    islisp_val %s;\n", val_var);
            emit_ast(f, n->as.setq.val, val_var, env);

            if (env && IS_TRUE(islisp_member(sym, env->vars))) {
                fprintf(f, "    V_%s = %s;\n", c_ident(sym_name), val_var);
            } else {
                char *lit_name = get_literal_c_name(sym);
                fprintf(f, "    islisp_set_global(%s, %s);\n", lit_name, val_var);
            }
            if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, val_var);
            break;
        }

        case AST_SET_DYNAMIC: {
            islisp_val sym = n->as.setq.sym;
            char *lit_name = get_literal_c_name(sym);
            int t_id = ++g_temp_count;
            char val_var[32];
            snprintf(val_var, sizeof(val_var), "tmp_%d", t_id);
            fprintf(f, "    islisp_val %s;\n", val_var);
            emit_ast(f, n->as.setq.val, val_var, env);
            fprintf(f, "    islisp_set_dynamic(%s, %s);\n", lit_name, val_var);
            if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, val_var);
            break;
        }

        case AST_IF: {
            int t_id = ++g_temp_count;
            char test_var[32];
            snprintf(test_var, sizeof(test_var), "test_%d", t_id);
            fprintf(f, "    islisp_val %s;\n", test_var);
            emit_ast(f, n->as.if_expr.test, test_var, env);
            fprintf(f, "    if (IS_TRUE(%s)) {\n", test_var);
            emit_ast(f, n->as.if_expr.then_branch, dest_var, env);
            fprintf(f, "    } else {\n");
            emit_ast(f, n->as.if_expr.else_branch, dest_var, env);
            fprintf(f, "    }\n");
            break;
        }

        case AST_PROGN: {
            for (int i = 0; i < n->as.progn.count; i++) {
                const char *d = (i == n->as.progn.count - 1) ? dest_var : NULL;
                emit_ast(f, n->as.progn.exprs[i], d, env);
            }
            if (n->as.progn.count == 0 && dest_var) {
                fprintf(f, "    %s = ISLISP_NIL;\n", dest_var);
            }
            break;
        }

        case AST_LET: {
            int nb = n->as.let_expr.num_bindings;
            fprintf(f, "    {\n");

            char **init_vars = (char**)malloc(sizeof(char*) * (nb > 0 ? nb : 1));
            for (int i = 0; i < nb; i++) {
                int t_id = ++g_temp_count;
                init_vars[i] = (char*)malloc(32);
                snprintf(init_vars[i], 32, "init_%d", t_id);
                fprintf(f, "        islisp_val %s;\n", init_vars[i]);
                emit_ast(f, n->as.let_expr.inits[i], init_vars[i], env);
            }

            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.vars = env ? env->vars : ISLISP_NIL;
            inner_env.fns = env ? env->fns : ISLISP_NIL;
            inner_env.blocks = env ? env->blocks : ISLISP_NIL;
            inner_env.tags = env ? env->tags : ISLISP_NIL;

            for (int i = 0; i < nb; i++) {
                islisp_val var = n->as.let_expr.vars[i];
                const char *vname = ((islisp_symbol_t*)var)->name;
                fprintf(f, "        islisp_val V_%s = %s;\n", c_ident(vname), init_vars[i]);
                inner_env.vars = islisp_cons(var, inner_env.vars);
                free(init_vars[i]);
            }
            free(init_vars);

            for (int i = 0; i < n->as.let_expr.body_count; i++) {
                const char *d = (i == n->as.let_expr.body_count - 1) ? dest_var : NULL;
                emit_ast(f, n->as.let_expr.body[i], d, &inner_env);
            }
            if (n->as.let_expr.body_count == 0 && dest_var) {
                fprintf(f, "        %s = ISLISP_NIL;\n", dest_var);
            }
            fprintf(f, "    }\n");
            break;
        }

        case AST_LET_STAR: {
            int nb = n->as.let_expr.num_bindings;
            fprintf(f, "    {\n");

            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.vars = env ? env->vars : ISLISP_NIL;
            inner_env.fns = env ? env->fns : ISLISP_NIL;
            inner_env.blocks = env ? env->blocks : ISLISP_NIL;
            inner_env.tags = env ? env->tags : ISLISP_NIL;

            for (int i = 0; i < nb; i++) {
                int t_id = ++g_temp_count;
                char init_var[32];
                snprintf(init_var, sizeof(init_var), "init_%d", t_id);
                fprintf(f, "        islisp_val %s;\n", init_var);
                emit_ast(f, n->as.let_expr.inits[i], init_var, &inner_env);
                islisp_val var = n->as.let_expr.vars[i];
                const char *vname = ((islisp_symbol_t*)var)->name;
                fprintf(f, "        islisp_val V_%s = %s;\n", c_ident(vname), init_var);
                inner_env.vars = islisp_cons(var, inner_env.vars);
            }

            for (int i = 0; i < n->as.let_expr.body_count; i++) {
                const char *d = (i == n->as.let_expr.body_count - 1) ? dest_var : NULL;
                emit_ast(f, n->as.let_expr.body[i], d, &inner_env);
            }
            if (n->as.let_expr.body_count == 0 && dest_var) {
                fprintf(f, "        %s = ISLISP_NIL;\n", dest_var);
            }
            fprintf(f, "    }\n");
            break;
        }

        case AST_FLET: {
            comp_env_t inner_env;
            memset(&inner_env, 0, sizeof(inner_env));
            inner_env.parent = env;
            inner_env.vars = env ? env->vars : ISLISP_NIL;
            inner_env.fns = env ? env->fns : ISLISP_NIL;
            inner_env.blocks = env ? env->blocks : ISLISP_NIL;
            inner_env.tags = env ? env->tags : ISLISP_NIL;

            for (int i = 0; i < n->as.flet_expr.num_fns; i++) {
                ast_node_t *lam = n->as.flet_expr.lambdas[i];
                int fid = lam->as.lambda.fn_id;
                int min_a = lam->as.lambda.num_params;
                int max_a = lam->as.lambda.has_rest ? -1 : min_a;
                islisp_val entry = islisp_cons(n->as.flet_expr.names[i],
                                     islisp_cons(TO_INT(fid),
                                       islisp_cons(TO_INT(min_a), TO_INT(max_a))));
                inner_env.fns = islisp_cons(entry, inner_env.fns);
            }

            for (int i = 0; i < n->as.flet_expr.body_count; i++) {
                const char *d = (i == n->as.flet_expr.body_count - 1) ? dest_var : NULL;
                emit_ast(f, n->as.flet_expr.body[i], d, &inner_env);
            }
            if (n->as.flet_expr.body_count == 0 && dest_var) {
                fprintf(f, "    %s = ISLISP_NIL;\n", dest_var);
            }
            break;
        }

        case AST_LAMBDA: {
            int fid = n->as.lambda.fn_id;
            int min_a = n->as.lambda.num_params;
            int max_a = n->as.lambda.has_rest ? -1 : min_a;
            if (dest_var) {
                fprintf(f, "    %s = islisp_make_closure(fn_lambda_%d, ISLISP_NIL, %d, %d, \"lambda\");\n",
                        dest_var, fid, min_a, max_a);
            }
            break;
        }

        case AST_FUNCTION: {
            islisp_val sym = n->as.var.sym;
            islisp_val fn_entry = env ? islisp_assoc(sym, env->fns) : ISLISP_NIL;
            if (IS_CONS(fn_entry)) {
                int local_fid = (int)AS_INT(CAR(CDR(fn_entry)));
                int min_a = (int)AS_INT(CAR(CDR(CDR(fn_entry))));
                int max_a = (int)AS_INT(CDR(CDR(CDR(fn_entry))));
                const char *sname = ((islisp_symbol_t*)sym)->name;
                if (dest_var) {
                    fprintf(f, "    %s = islisp_make_closure(fn_lambda_%d, ISLISP_NIL, %d, %d, \"%s\");\n",
                            dest_var, local_fid, min_a, max_a, sname);
                }
            } else {
                char *lit_name = get_literal_c_name(sym);
                if (dest_var) fprintf(f, "    %s = islisp_get_function(%s);\n", dest_var, lit_name);
            }
            break;
        }

        case AST_DEFUN: {
            islisp_val name = n->as.defun_expr.name;
            ast_node_t *lam = n->as.defun_expr.lambda_ast;
            char *lit_name = get_literal_c_name(name);
            int min_a = lam->as.lambda.num_params;
            int max_a = lam->as.lambda.has_rest ? -1 : min_a;
            const char *n_str = ((islisp_symbol_t*)name)->name;
            fprintf(f, "    islisp_set_function(%s, islisp_make_closure(fn_user_%s, ISLISP_NIL, %d, %d, \"%s\"));\n",
                    lit_name, c_ident(n_str), min_a, max_a, n_str);
            if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, lit_name);
            break;
        }

        case AST_DEFGLOBAL:
        case AST_DEFCONSTANT: {
            islisp_val name = n->as.defglobal.name;
            char *lit_name = get_literal_c_name(name);
            int t_id = ++g_temp_count;
            char val_var[32];
            snprintf(val_var, sizeof(val_var), "tmp_%d", t_id);
            fprintf(f, "    islisp_val %s;\n", val_var);
            ast_node_t *val_ast = islisp_parse_ast(n->as.defglobal.val_sexpr, env);
            emit_ast(f, val_ast, val_var, env);
            islisp_free_ast(val_ast);
            fprintf(f, "    islisp_set_global(%s, %s);\n", lit_name, val_var);
            if (n->type == AST_DEFCONSTANT) {
                fprintf(f, "    ((islisp_symbol_t*)%s)->is_constant = true;\n", lit_name);
            }
            if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, lit_name);
            break;
        }

        case AST_DEFDYNAMIC: {
            islisp_val name = n->as.defglobal.name;
            char *lit_name = get_literal_c_name(name);
            int t_id = ++g_temp_count;
            char val_var[32];
            snprintf(val_var, sizeof(val_var), "tmp_%d", t_id);
            fprintf(f, "    islisp_val %s;\n", val_var);
            ast_node_t *val_ast = islisp_parse_ast(n->as.defglobal.val_sexpr, env);
            emit_ast(f, val_ast, val_var, env);
            islisp_free_ast(val_ast);
            fprintf(f, "    islisp_set_dynamic(%s, %s);\n", lit_name, val_var);
            if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, lit_name);
            break;
        }

        case AST_BLOCK: {
            int bid = n->as.block.block_id;
            islisp_val bname = n->as.block.name;
            char *lit_name = get_literal_c_name(bname);

            fprintf(f, "    {\n");
            fprintf(f, "        islisp_frame_t bframe_%d;\n", bid);
            fprintf(f, "        bframe_%d.kind = FRAME_BLOCK;\n", bid);
            fprintf(f, "        bframe_%d.tag = %s;\n", bid, lit_name);
            fprintf(f, "        islisp_push_frame(&bframe_%d);\n", bid);
            fprintf(f, "        if (!setjmp(bframe_%d.jmp)) {\n", bid);

            for (int i = 0; i < n->as.block.body_count; i++) {
                const char *d = (i == n->as.block.body_count - 1) ? dest_var : NULL;
                emit_ast(f, n->as.block.body[i], d, env);
            }
            if (n->as.block.body_count == 0 && dest_var) {
                fprintf(f, "            %s = ISLISP_NIL;\n", dest_var);
            }
            fprintf(f, "        } else {\n");
            if (dest_var) fprintf(f, "            %s = bframe_%d.val;\n", dest_var, bid);
            fprintf(f, "        }\n");
            fprintf(f, "        islisp_pop_frame(&bframe_%d);\n", bid);
            fprintf(f, "        block_exit_%d: ;\n", bid);
            fprintf(f, "    }\n");
            break;
        }

        case AST_RETURN_FROM: {
            islisp_val bname = n->as.return_from.name;
            char *lit_name = get_literal_c_name(bname);
            int t_id = ++g_temp_count;
            char val_var[32];
            snprintf(val_var, sizeof(val_var), "tmp_%d", t_id);
            fprintf(f, "    islisp_val %s;\n", val_var);
            emit_ast(f, n->as.return_from.val, val_var, env);
            fprintf(f, "    islisp_return_from(%s, %s);\n", lit_name, val_var);
            break;
        }

        case AST_TAGBODY: {
            int tbid = n->as.tagbody.tagbody_id;
            fprintf(f, "    {\n");
            for (int i = 0; i < n->as.tagbody.count; i++) {
                if (n->as.tagbody.items[i] == NULL) {
                    islisp_val tag = n->as.tagbody.tags[i];
                    const char *ts = ((islisp_symbol_t*)tag)->name;
                    fprintf(f, "        tag_%d_%s: ;\n", tbid, c_ident(ts));
                } else {
                    emit_ast(f, n->as.tagbody.items[i], NULL, env);
                }
            }
            if (dest_var) fprintf(f, "        %s = ISLISP_NIL;\n", dest_var);
            fprintf(f, "    }\n");
            break;
        }

        case AST_GO: {
            islisp_val tag = n->as.go.tag;
            const char *ts = ((islisp_symbol_t*)tag)->name;
            fprintf(f, "    goto tag_%d_%s;\n", n->as.go.target_tagbody_id, c_ident(ts));
            break;
        }

        case AST_CATCH: {
            int cid = n->as.catch_expr.catch_id;
            int t_id = ++g_temp_count;
            char tag_var[32];
            snprintf(tag_var, sizeof(tag_var), "tag_%d", t_id);
            fprintf(f, "    {\n");
            fprintf(f, "        islisp_val %s;\n", tag_var);
            emit_ast(f, n->as.catch_expr.tag, tag_var, env);

            fprintf(f, "        islisp_frame_t cframe_%d;\n", cid);
            fprintf(f, "        cframe_%d.kind = FRAME_CATCH;\n", cid);
            fprintf(f, "        cframe_%d.tag = %s;\n", cid, tag_var);
            fprintf(f, "        islisp_push_frame(&cframe_%d);\n", cid);
            fprintf(f, "        if (!setjmp(cframe_%d.jmp)) {\n", cid);

            for (int i = 0; i < n->as.catch_expr.count; i++) {
                const char *d = (i == n->as.catch_expr.count - 1) ? dest_var : NULL;
                emit_ast(f, n->as.catch_expr.body[i], d, env);
            }
            if (n->as.catch_expr.count == 0 && dest_var) {
                fprintf(f, "            %s = ISLISP_NIL;\n", dest_var);
            }
            fprintf(f, "        } else {\n");
            if (dest_var) fprintf(f, "            %s = cframe_%d.val;\n", dest_var, cid);
            fprintf(f, "        }\n");
            fprintf(f, "        islisp_pop_frame(&cframe_%d);\n", cid);
            fprintf(f, "    }\n");
            break;
        }

        case AST_THROW: {
            int t_id1 = ++g_temp_count;
            int t_id2 = ++g_temp_count;
            char tag_var[32], val_var[32];
            snprintf(tag_var, sizeof(tag_var), "tmp_%d", t_id1);
            snprintf(val_var, sizeof(val_var), "tmp_%d", t_id2);
            fprintf(f, "    islisp_val %s, %s;\n", tag_var, val_var);
            emit_ast(f, n->as.throw_expr.tag, tag_var, env);
            emit_ast(f, n->as.throw_expr.val, val_var, env);
            fprintf(f, "    islisp_throw(%s, %s);\n", tag_var, val_var);
            break;
        }

        case AST_UNWIND_PROTECT: {
            int uid = n->as.unwind.unwind_id;
            fprintf(f, "    {\n");
            fprintf(f, "        islisp_frame_t uframe_%d;\n", uid);
            fprintf(f, "        uframe_%d.kind = FRAME_UNWIND;\n", uid);
            fprintf(f, "        islisp_push_frame(&uframe_%d);\n", uid);
            fprintf(f, "        if (!setjmp(uframe_%d.jmp)) {\n", uid);
            emit_ast(f, n->as.unwind.protected_expr, dest_var, env);
            fprintf(f, "            islisp_pop_frame(&uframe_%d);\n", uid);
            for (int i = 0; i < n->as.unwind.cleanup_count; i++) {
                emit_ast(f, n->as.unwind.cleanups[i], NULL, env);
            }
            fprintf(f, "        } else {\n");
            fprintf(f, "            islisp_pop_frame(&uframe_%d);\n", uid);
            for (int i = 0; i < n->as.unwind.cleanup_count; i++) {
                emit_ast(f, n->as.unwind.cleanups[i], NULL, env);
            }
            fprintf(f, "            islisp_continue_unwind();\n");
            fprintf(f, "        }\n");
            fprintf(f, "    }\n");
            break;
        }

        case AST_DYNAMIC_LET: {
            int nb = n->as.dynamic_let.num_bindings;
            fprintf(f, "    {\n");
            char **val_vars = (char**)malloc(sizeof(char*) * (nb > 0 ? nb : 1));
            for (int i = 0; i < nb; i++) {
                int t_id = ++g_temp_count;
                val_vars[i] = (char*)malloc(32);
                snprintf(val_vars[i], 32, "dval_%d", t_id);
                fprintf(f, "        islisp_val %s;\n", val_vars[i]);
                emit_ast(f, n->as.dynamic_let.inits[i], val_vars[i], env);
            }
            for (int i = 0; i < nb; i++) {
                char *lit_name = get_literal_c_name(n->as.dynamic_let.vars[i]);
                int fid = ++g_temp_count;
                fprintf(f, "        islisp_frame_t dframe_%d;\n", fid);
                fprintf(f, "        dframe_%d.kind = FRAME_DYNAMIC;\n", fid);
                fprintf(f, "        dframe_%d.tag = %s;\n", fid, lit_name);
                fprintf(f, "        dframe_%d.val = %s;\n", fid, val_vars[i]);
                fprintf(f, "        islisp_push_frame(&dframe_%d);\n", fid);
            }
            for (int i = 0; i < n->as.dynamic_let.body_count; i++) {
                const char *d = (i == n->as.dynamic_let.body_count - 1) ? dest_var : NULL;
                emit_ast(f, n->as.dynamic_let.body[i], d, env);
            }
            for (int i = 0; i < nb; i++) {
                fprintf(f, "        islisp_pop_frame(g_top_frame);\n");
                free(val_vars[i]);
            }
            free(val_vars);
            fprintf(f, "    }\n");
            break;
        }

        case AST_WITH_HANDLER: {
            int hid = ++g_temp_count;
            fprintf(f, "    {\n");
            char h_var[32];
            snprintf(h_var, sizeof(h_var), "handler_%d", hid);
            fprintf(f, "        islisp_val %s;\n", h_var);
            emit_ast(f, n->as.with_handler.handler, h_var, env);

            fprintf(f, "        islisp_frame_t hframe_%d;\n", hid);
            fprintf(f, "        hframe_%d.kind = FRAME_HANDLER;\n", hid);
            fprintf(f, "        hframe_%d.var = %s;\n", hid, h_var);
            fprintf(f, "        islisp_push_frame(&hframe_%d);\n", hid);

            for (int i = 0; i < n->as.with_handler.body_count; i++) {
                const char *d = (i == n->as.with_handler.body_count - 1) ? dest_var : NULL;
                emit_ast(f, n->as.with_handler.body[i], d, env);
            }
            fprintf(f, "        islisp_pop_frame(&hframe_%d);\n", hid);
            fprintf(f, "    }\n");
            break;
        }

        case AST_DEFCLASS: {
            char *cls_lit = get_literal_c_name(n->as.defclass_expr.name);
            char *sup_lit = get_literal_c_name(n->as.defclass_expr.supers);
            char *slot_lit = get_literal_c_name(n->as.defclass_expr.slot_specs);
            fprintf(f, "    islisp_defclass(%s, %s, %s, %s);\n",
                    cls_lit, sup_lit, slot_lit, n->as.defclass_expr.is_abstract ? "true" : "false");
            if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, cls_lit);
            break;
        }

        case AST_DEFGENERIC: {
            char *gf_lit = get_literal_c_name(n->as.defgeneric_expr.name);
            fprintf(f, "    islisp_defgeneric(%s, %d, %s);\n",
                    gf_lit, n->as.defgeneric_expr.num_req, n->as.defgeneric_expr.has_rest ? "true" : "false");
            if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, gf_lit);
            break;
        }

        case AST_DEFMETHOD: {
            char *gf_lit = get_literal_c_name(n->as.defmethod_expr.name);
            char *spec_lit = get_literal_c_name(n->as.defmethod_expr.specializers);
            int fid = n->as.defmethod_expr.method_lambda->as.lambda.fn_id;
            int min_a = n->as.defmethod_expr.method_lambda->as.lambda.num_params;
            int max_a = n->as.defmethod_expr.method_lambda->as.lambda.has_rest ? -1 : min_a;
            fprintf(f, "    islisp_defmethod(%s, %d, %s, %s, (islisp_closure_t*)islisp_make_closure(fn_lambda_%d, ISLISP_NIL, %d, %d, \"method\"));\n",
                    gf_lit, n->as.defmethod_expr.qual, spec_lit,
                    n->as.defmethod_expr.has_rest ? "true" : "false", fid, min_a, max_a);
            if (dest_var) fprintf(f, "    %s = %s;\n", dest_var, gf_lit);
            break;
        }

        case AST_CONVERT: {
            int t_id = ++g_temp_count;
            char obj_var[32];
            snprintf(obj_var, sizeof(obj_var), "tmp_%d", t_id);
            fprintf(f, "    islisp_val %s;\n", obj_var);
            emit_ast(f, n->as.convert_expr.obj, obj_var, env);
            char *target_lit = get_literal_c_name(n->as.convert_expr.target_class);
            if (dest_var) {
                fprintf(f, "    %s = islisp_convert(%s, islisp_get_global(%s));\n", dest_var, obj_var, target_lit);
            }
            break;
        }

        case AST_CALL: {
            int argc = n->as.call.argc;
            char **argv_vars = (char**)malloc(sizeof(char*) * (argc > 0 ? argc : 1));
            for (int i = 0; i < argc; i++) {
                int t_id = ++g_temp_count;
                argv_vars[i] = (char*)malloc(32);
                snprintf(argv_vars[i], 32, "arg_%d", t_id);
                fprintf(f, "    islisp_val %s;\n", argv_vars[i]);
                emit_ast(f, n->as.call.args[i], argv_vars[i], env);
            }

            islisp_val fn_op = n->as.call.fn;
            islisp_val fn_entry = (env && IS_SYMBOL(fn_op)) ? islisp_assoc(fn_op, env->fns) : ISLISP_NIL;
            if (IS_CONS(fn_entry)) {
                int local_fid = (int)AS_INT(CAR(CDR(fn_entry)));
                fprintf(f, "    {\n");
                fprintf(f, "        islisp_val c_argv[%d] = {", argc > 0 ? argc : 1);
                for (int i = 0; i < argc; i++) fprintf(f, "%s%s", i > 0 ? ", " : "", argv_vars[i]);
                fprintf(f, "};\n");
                if (dest_var) {
                    fprintf(f, "        %s = fn_lambda_%d(ISLISP_NIL, %d, c_argv);\n", dest_var, local_fid, argc);
                } else {
                    fprintf(f, "        fn_lambda_%d(ISLISP_NIL, %d, c_argv);\n", local_fid, argc);
                }
                fprintf(f, "    }\n");
            } else if (IS_SYMBOL(fn_op)) {
                const char *fn_name = ((islisp_symbol_t*)fn_op)->name;

                if (strcmp(fn_name, "funcall") == 0 && argc >= 1) {
                    fprintf(f, "    {\n");
                    fprintf(f, "        islisp_val c_argv[%d] = {", argc > 1 ? argc - 1 : 1);
                    for (int i = 1; i < argc; i++) fprintf(f, "%s%s", i > 1 ? ", " : "", argv_vars[i]);
                    fprintf(f, "};\n");
                    if (dest_var) fprintf(f, "        %s = islisp_funcall_argv(%s, %d, c_argv);\n", dest_var, argv_vars[0], argc - 1);
                    else fprintf(f, "        islisp_funcall_argv(%s, %d, c_argv);\n", argv_vars[0], argc - 1);
                    fprintf(f, "    }\n");
                } else if (strcmp(fn_name, "apply") == 0 && argc >= 2) {
                    fprintf(f, "    {\n");
                    fprintf(f, "        islisp_val c_argv[%d] = {", argc - 1);
                    for (int i = 1; i < argc; i++) fprintf(f, "%s%s", i > 1 ? ", " : "", argv_vars[i]);
                    fprintf(f, "};\n");
                    if (dest_var) fprintf(f, "        %s = islisp_apply(%s, %d, c_argv);\n", dest_var, argv_vars[0], argc - 1);
                    else fprintf(f, "        islisp_apply(%s, %d, c_argv);\n", argv_vars[0], argc - 1);
                    fprintf(f, "    }\n");
                } else if (strcmp(fn_name, "+") == 0) {
                    fprintf(f, "    {\n        islisp_val c_argv[%d] = {", argc > 0 ? argc : 1);
                    for (int i = 0; i < argc; i++) fprintf(f, "%s%s", i > 0 ? ", " : "", argv_vars[i]);
                    fprintf(f, "};\n");
                    if (dest_var) fprintf(f, "        %s = islisp_add(%d, c_argv);\n", dest_var, argc);
                    else fprintf(f, "        islisp_add(%d, c_argv);\n", argc);
                    fprintf(f, "    }\n");
                } else if (strcmp(fn_name, "-") == 0) {
                    fprintf(f, "    {\n        islisp_val c_argv[%d] = {", argc > 0 ? argc : 1);
                    for (int i = 0; i < argc; i++) fprintf(f, "%s%s", i > 0 ? ", " : "", argv_vars[i]);
                    fprintf(f, "};\n");
                    if (dest_var) fprintf(f, "        %s = islisp_sub(%d, c_argv);\n", dest_var, argc);
                    else fprintf(f, "        islisp_sub(%d, c_argv);\n", argc);
                    fprintf(f, "    }\n");
                } else if (strcmp(fn_name, "*") == 0) {
                    fprintf(f, "    {\n        islisp_val c_argv[%d] = {", argc > 0 ? argc : 1);
                    for (int i = 0; i < argc; i++) fprintf(f, "%s%s", i > 0 ? ", " : "", argv_vars[i]);
                    fprintf(f, "};\n");
                    if (dest_var) fprintf(f, "        %s = islisp_mul(%d, c_argv);\n", dest_var, argc);
                    else fprintf(f, "        islisp_mul(%d, c_argv);\n", argc);
                    fprintf(f, "    }\n");
                } else if (strcmp(fn_name, "/") == 0) {
                    fprintf(f, "    {\n        islisp_val c_argv[%d] = {", argc > 0 ? argc : 1);
                    for (int i = 0; i < argc; i++) fprintf(f, "%s%s", i > 0 ? ", " : "", argv_vars[i]);
                    fprintf(f, "};\n");
                    if (dest_var) fprintf(f, "        %s = islisp_div(%d, c_argv);\n", dest_var, argc);
                    else fprintf(f, "        islisp_div(%d, c_argv);\n", argc);
                    fprintf(f, "    }\n");
                } else if (strcmp(fn_name, "cons") == 0 && argc == 2) {
                    if (dest_var) fprintf(f, "    %s = islisp_cons(%s, %s);\n", dest_var, argv_vars[0], argv_vars[1]);
                    else fprintf(f, "    islisp_cons(%s, %s);\n", argv_vars[0], argv_vars[1]);
                } else if (strcmp(fn_name, "car") == 0 && argc == 1) {
                    if (dest_var) fprintf(f, "    %s = islisp_car(%s);\n", dest_var, argv_vars[0]);
                    else fprintf(f, "    islisp_car(%s);\n", argv_vars[0]);
                } else if (strcmp(fn_name, "cdr") == 0 && argc == 1) {
                    if (dest_var) fprintf(f, "    %s = islisp_cdr(%s);\n", dest_var, argv_vars[0]);
                    else fprintf(f, "    islisp_cdr(%s);\n", argv_vars[0]);
                } else if (strcmp(fn_name, "format") == 0 && argc >= 2) {
                    int num_fmt_args = argc - 2;
                    fprintf(f, "    {\n        islisp_val c_argv[%d] = {", num_fmt_args > 0 ? num_fmt_args : 1);
                    for (int i = 2; i < argc; i++) fprintf(f, "%s%s", i > 2 ? ", " : "", argv_vars[i]);
                    fprintf(f, "};\n");
                    if (dest_var) fprintf(f, "        %s = islisp_format(%s, %s, %d, c_argv);\n", dest_var, argv_vars[0], argv_vars[1], num_fmt_args);
                    else fprintf(f, "        islisp_format(%s, %s, %d, c_argv);\n", argv_vars[0], argv_vars[1], num_fmt_args);
                    fprintf(f, "    }\n");
                } else {
                    char *lit_name = get_literal_c_name(fn_op);
                    fprintf(f, "    {\n");
                    fprintf(f, "        islisp_val c_argv[%d] = {", argc > 0 ? argc : 1);
                    for (int i = 0; i < argc; i++) fprintf(f, "%s%s", i > 0 ? ", " : "", argv_vars[i]);
                    fprintf(f, "};\n");
                    if (dest_var) {
                        fprintf(f, "        %s = islisp_funcall_argv(islisp_get_function(%s), %d, c_argv);\n",
                                dest_var, lit_name, argc);
                    } else {
                        fprintf(f, "        islisp_funcall_argv(islisp_get_function(%s), %d, c_argv);\n",
                                lit_name, argc);
                    }
                    fprintf(f, "    }\n");
                }
            }

            for (int i = 0; i < argc; i++) free(argv_vars[i]);
            free(argv_vars);
            break;
        }

        default:
            if (dest_var) fprintf(f, "    %s = ISLISP_NIL;\n", dest_var);
            break;
    }
}

/* Collect lambdas and defuns for top-level code emission */
typedef struct func_node {
    ast_node_t *ast;
    struct func_node *next;
} func_node_t;

static func_node_t *g_functions = NULL;

static void add_function_node(ast_node_t *lam) {
    if (!lam) return;
    func_node_t *curr = g_functions;
    while (curr) {
        if (curr->ast == lam) return;
        curr = curr->next;
    }
    func_node_t *fn = (func_node_t*)malloc(sizeof(func_node_t));
    fn->ast = lam;
    fn->next = g_functions;
    g_functions = fn;
}

static void collect_functions(ast_node_t *n) {
    if (!n) return;
    if (n->type == AST_LAMBDA) {
        add_function_node(n);
    }
    if (n->type == AST_DEFUN) {
        add_function_node(n->as.defun_expr.lambda_ast);
    }
    switch (n->type) {
        case AST_IF:
            collect_functions(n->as.if_expr.test);
            collect_functions(n->as.if_expr.then_branch);
            collect_functions(n->as.if_expr.else_branch);
            break;
        case AST_PROGN:
            for (int i = 0; i < n->as.progn.count; i++) collect_functions(n->as.progn.exprs[i]);
            break;
        case AST_LET:
        case AST_LET_STAR:
            for (int i = 0; i < n->as.let_expr.num_bindings; i++) collect_functions(n->as.let_expr.inits[i]);
            for (int i = 0; i < n->as.let_expr.body_count; i++) collect_functions(n->as.let_expr.body[i]);
            break;
        case AST_FLET:
            for (int i = 0; i < n->as.flet_expr.num_fns; i++) collect_functions(n->as.flet_expr.lambdas[i]);
            for (int i = 0; i < n->as.flet_expr.body_count; i++) collect_functions(n->as.flet_expr.body[i]);
            break;
        case AST_SETQ:
            collect_functions(n->as.setq.val);
            break;
        case AST_CALL:
            for (int i = 0; i < n->as.call.argc; i++) collect_functions(n->as.call.args[i]);
            break;
        case AST_BLOCK:
            for (int i = 0; i < n->as.block.body_count; i++) collect_functions(n->as.block.body[i]);
            break;
        case AST_RETURN_FROM:
            collect_functions(n->as.return_from.val);
            break;
        case AST_TAGBODY:
            for (int i = 0; i < n->as.tagbody.count; i++) {
                if (n->as.tagbody.items[i]) collect_functions(n->as.tagbody.items[i]);
            }
            break;
        case AST_CATCH:
            collect_functions(n->as.catch_expr.tag);
            for (int i = 0; i < n->as.catch_expr.count; i++) collect_functions(n->as.catch_expr.body[i]);
            break;
        case AST_THROW:
            collect_functions(n->as.throw_expr.tag);
            collect_functions(n->as.throw_expr.val);
            break;
        case AST_UNWIND_PROTECT:
            collect_functions(n->as.unwind.protected_expr);
            for (int i = 0; i < n->as.unwind.cleanup_count; i++) collect_functions(n->as.unwind.cleanups[i]);
            break;
        case AST_DYNAMIC_LET:
            for (int i = 0; i < n->as.dynamic_let.num_bindings; i++) collect_functions(n->as.dynamic_let.inits[i]);
            for (int i = 0; i < n->as.dynamic_let.body_count; i++) collect_functions(n->as.dynamic_let.body[i]);
            break;
        case AST_WITH_HANDLER:
            collect_functions(n->as.with_handler.handler);
            for (int i = 0; i < n->as.with_handler.body_count; i++) collect_functions(n->as.with_handler.body[i]);
            break;
        case AST_LAMBDA:
            for (int i = 0; i < n->as.lambda.body_count; i++) collect_functions(n->as.lambda.body[i]);
            break;
        case AST_DEFUN:
            collect_functions(n->as.defun_expr.lambda_ast);
            break;
        case AST_DEFMETHOD:
            collect_functions(n->as.defmethod_expr.method_lambda);
            break;
        default:
            break;
    }
}

static void emit_lambda_func(FILE *f, ast_node_t *lam) {
    bool is_defun = (lam->as.lambda.name != ISLISP_NIL);
    const char *name_str = is_defun ? ((islisp_symbol_t*)lam->as.lambda.name)->name : NULL;
    int fid = lam->as.lambda.fn_id;

    if (is_defun) {
        fprintf(f, "static islisp_val fn_user_%s(islisp_val env, int argc, islisp_val *argv) {\n", c_ident(name_str));
    } else {
        fprintf(f, "static islisp_val fn_lambda_%d(islisp_val env, int argc, islisp_val *argv) {\n", fid);
    }
    fprintf(f, "    (void)env; (void)argc; (void)argv;\n");

    comp_env_t inner_env;
    memset(&inner_env, 0, sizeof(inner_env));
    inner_env.parent = NULL;
    inner_env.vars = ISLISP_NIL;
    inner_env.fns = lam->as.lambda.lexical_fns;

    int pcount = lam->as.lambda.num_params;
    for (int i = 0; i < pcount; i++) {
        islisp_val p = lam->as.lambda.param_names[i];
        const char *ps = ((islisp_symbol_t*)p)->name;
        fprintf(f, "    islisp_val V_%s = (argc > %d) ? argv[%d] : ISLISP_NIL;\n", c_ident(ps), i, i);
        inner_env.vars = islisp_cons(p, inner_env.vars);
    }
    if (lam->as.lambda.has_rest) {
        islisp_val r = lam->as.lambda.rest_var;
        const char *rs = ((islisp_symbol_t*)r)->name;
        fprintf(f, "    islisp_val V_%s = (argc > %d) ? islisp_list(argc - %d, argv + %d) : ISLISP_NIL;\n",
                c_ident(rs), pcount, pcount, pcount);
        inner_env.vars = islisp_cons(r, inner_env.vars);
    }

    fprintf(f, "    islisp_val return_val = ISLISP_NIL;\n");
    for (int i = 0; i < lam->as.lambda.body_count; i++) {
        const char *d = (i == lam->as.lambda.body_count - 1) ? "return_val" : NULL;
        emit_ast(f, lam->as.lambda.body[i], d, &inner_env);
    }
    fprintf(f, "    return return_val;\n");
    fprintf(f, "}\n\n");
}

static void emit_literal_init(FILE *f, lit_node_t *n) {
    islisp_val v = n->val;
    if (IS_SYMBOL(v)) {
        const char *sname = ((islisp_symbol_t*)v)->name;
        fprintf(f, "    %s = islisp_intern(\"%s\");\n", n->c_name, sname);
    } else if (IS_STRING(v)) {
        islisp_string_t *str = (islisp_string_t*)v;
        fprintf(f, "    %s = islisp_make_string(\"", n->c_name);
        for (uint32_t i = 0; i < str->header.len; i++) {
            char c = str->data[i];
            if (c == '"') fprintf(f, "\\\"");
            else if (c == '\\') fprintf(f, "\\\\");
            else if (c == '\n') fprintf(f, "\\n");
            else if (c == '\r') fprintf(f, "\\r");
            else if (c == '\t') fprintf(f, "\\t");
            else fputc(c, f);
        }
        fprintf(f, "\");\n");
    } else if (IS_CONS(v)) {
        const char *car_ref = get_literal_ref(CAR(v));
        const char *cdr_ref = get_literal_ref(CDR(v));
        fprintf(f, "    %s = islisp_cons(%s, %s);\n", n->c_name, car_ref, cdr_ref);
    } else {
        fprintf(f, "    %s = ISLISP_NIL;\n", n->c_name);
    }
}

bool islisp_compile_file_to_c(const char *in_lsp, const char *out_c) {
    islisp_init_runtime();
    islisp_init_macros();
    reset_literals();
    g_functions = NULL;

    islisp_val forms = islisp_read_file(in_lsp);

    int form_count = (int)AS_INT(islisp_length(forms));
    ast_node_t **toplevel_asts = (ast_node_t**)malloc(sizeof(ast_node_t*) * (form_count > 0 ? form_count : 1));
    islisp_val p = forms;
    for (int i = 0; i < form_count; i++) {
        islisp_val form = CAR(p);
        islisp_val expanded = islisp_macroexpand(form);
        toplevel_asts[i] = islisp_parse_ast(expanded, NULL);
        collect_functions(toplevel_asts[i]);
        collect_literals(toplevel_asts[i], NULL);
        p = CDR(p);
    }

    FILE *f = fopen(out_c, "w");
    if (!f) {
        fprintf(stderr, "Cannot open output C file: %s\n", out_c);
        return false;
    }

    fprintf(f, "/* Generated by CISL - ISLisp to C Compiler */\n");
    fprintf(f, "#include \"islisp_runtime.h\"\n\n");

    /* Forward declare literals */
    lit_node_t *lit = g_literals;
    while (lit) {
        fprintf(f, "static islisp_val %s;\n", lit->c_name);
        lit = lit->next;
    }
    fprintf(f, "\n");

    /* Forward declare functions */
    func_node_t *fn = g_functions;
    while (fn) {
        ast_node_t *lam = fn->ast;
        bool is_defun = (lam->as.lambda.name != ISLISP_NIL);
        if (is_defun) {
            fprintf(f, "static islisp_val fn_user_%s(islisp_val env, int argc, islisp_val *argv);\n",
                    c_ident(((islisp_symbol_t*)lam->as.lambda.name)->name));
        } else {
            fprintf(f, "static islisp_val fn_lambda_%d(islisp_val env, int argc, islisp_val *argv);\n",
                    lam->as.lambda.fn_id);
        }
        fn = fn->next;
    }
    fprintf(f, "\n");

    /* Emit function definitions */
    fn = g_functions;
    while (fn) {
        emit_lambda_func(f, fn->ast);
        fn = fn->next;
    }

    /* Emit literal initializer */
    fprintf(f, "static void init_literals(void) {\n");
    lit = g_literals;
    while (lit) {
        emit_literal_init(f, lit);
        fprintf(f, "    islisp_gc_register_root(&%s);\n", lit->c_name);
        lit = lit->next;
    }
    fprintf(f, "}\n\n");

    /* Emit toplevel execution */
    fprintf(f, "static void toplevel_exec(void) {\n");
    for (int i = 0; i < form_count; i++) {
        emit_ast(f, toplevel_asts[i], NULL, NULL);
    }
    fprintf(f, "}\n\n");

    /* Emit main() */
    fprintf(f, "int main(int argc, char **argv) {\n");
    fprintf(f, "    (void)argc; (void)argv;\n");
    fprintf(f, "    islisp_init_runtime();\n");
    fprintf(f, "    init_literals();\n");
    fprintf(f, "    toplevel_exec();\n");
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");

    fclose(f);

    for (int i = 0; i < form_count; i++) islisp_free_ast(toplevel_asts[i]);
    free(toplevel_asts);
    return true;
}

bool islisp_compile_string_to_c(const char *code, const char *out_c) {
    islisp_init_runtime();
    islisp_init_macros();
    reset_literals();
    g_functions = NULL;

    islisp_val forms = islisp_read_from_string(code);
    int form_count = (int)AS_INT(islisp_length(forms));
    ast_node_t **toplevel_asts = (ast_node_t**)malloc(sizeof(ast_node_t*) * (form_count > 0 ? form_count : 1));
    islisp_val p = forms;
    for (int i = 0; i < form_count; i++) {
        islisp_val form = CAR(p);
        islisp_val expanded = islisp_macroexpand(form);
        toplevel_asts[i] = islisp_parse_ast(expanded, NULL);
        collect_functions(toplevel_asts[i]);
        collect_literals(toplevel_asts[i], NULL);
        p = CDR(p);
    }

    FILE *f = fopen(out_c, "w");
    if (!f) return false;

    fprintf(f, "/* Generated by CISL */\n#include \"islisp_runtime.h\"\n\n");
    lit_node_t *lit = g_literals;
    while (lit) {
        fprintf(f, "static islisp_val %s;\n", lit->c_name);
        lit = lit->next;
    }
    fprintf(f, "\n");

    func_node_t *fn = g_functions;
    while (fn) {
        ast_node_t *lam = fn->ast;
        if (lam->as.lambda.name != ISLISP_NIL) {
            fprintf(f, "static islisp_val fn_user_%s(islisp_val env, int argc, islisp_val *argv);\n",
                    c_ident(((islisp_symbol_t*)lam->as.lambda.name)->name));
        } else {
            fprintf(f, "static islisp_val fn_lambda_%d(islisp_val env, int argc, islisp_val *argv);\n",
                    lam->as.lambda.fn_id);
        }
        fn = fn->next;
    }
    fprintf(f, "\n");

    fn = g_functions;
    while (fn) {
        emit_lambda_func(f, fn->ast);
        fn = fn->next;
    }

    fprintf(f, "static void init_literals(void) {\n");
    lit = g_literals;
    while (lit) {
        emit_literal_init(f, lit);
        fprintf(f, "    islisp_gc_register_root(&%s);\n", lit->c_name);
        lit = lit->next;
    }
    fprintf(f, "}\n\n");

    fprintf(f, "static void toplevel_exec(void) {\n");
    if (form_count > 0) {
        fprintf(f, "    islisp_val res = ISLISP_UNBOUND;\n");
        for (int i = 0; i < form_count; i++) {
            const char *d = (i == form_count - 1) ? "res" : NULL;
            emit_ast(f, toplevel_asts[i], d, NULL);
        }
        fprintf(f, "    if (res != ISLISP_UNBOUND) {\n");
        fprintf(f, "        islisp_val stdout_s = islisp_standard_output();\n");
        fprintf(f, "        islisp_format(stdout_s, islisp_make_string(\"~S\\n\"), 1, &res);\n");
        fprintf(f, "    }\n");
    }
    fprintf(f, "}\n\n");

    fprintf(f, "int main(int argc, char **argv) {\n");
    fprintf(f, "    (void)argc; (void)argv;\n");
    fprintf(f, "    islisp_init_runtime();\n");
    fprintf(f, "    init_literals();\n");
    fprintf(f, "    toplevel_exec();\n");
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");

    fclose(f);
    for (int i = 0; i < form_count; i++) islisp_free_ast(toplevel_asts[i]);
    free(toplevel_asts);
    return true;
}

bool islisp_compile_c_to_binary(const char *in_c, const char *out_exe) {
    char cmd[1024];
    /* If libislisp_rt.a exists, link with it; otherwise compile with runtime objects */
    FILE *f = fopen("libislisp_rt.a", "rb");
    if (f) {
        fclose(f);
        snprintf(cmd, sizeof(cmd), "gcc -O2 -Iinclude \"%s\" libislisp_rt.a -lm -o \"%s\"", in_c, out_exe);
    } else {
        snprintf(cmd, sizeof(cmd), "gcc -O2 -Iinclude \"%s\" src/runtime/runtime.c src/runtime/math.c src/runtime/list.c src/runtime/string.c src/runtime/io.c src/runtime/error.c src/runtime/ilos.c -lm -o \"%s\"", in_c, out_exe);
    }
    int ret = system(cmd);
    return (ret == 0);
}
