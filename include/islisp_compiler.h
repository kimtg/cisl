#ifndef ISLISP_COMPILER_H
#define ISLISP_COMPILER_H

#include "islisp.h"
#include "islisp_runtime.h"

/* Compiler AST representation */

typedef enum {
    AST_LITERAL,
    AST_VAR,
    AST_DYNAMIC_VAR,
    AST_CALL,
    AST_IF,
    AST_PROGN,
    AST_LET,
    AST_LET_STAR,
    AST_SETQ,
    AST_SET_DYNAMIC,
    AST_LAMBDA,
    AST_FUNCTION,
    AST_DEFUN,
    AST_DEFMACRO,
    AST_DEFGLOBAL,
    AST_DEFCONSTANT,
    AST_DEFDYNAMIC,
    AST_DEFCLASS,
    AST_DEFGENERIC,
    AST_DEFMETHOD,
    AST_BLOCK,
    AST_RETURN_FROM,
    AST_TAGBODY,
    AST_GO,
    AST_CATCH,
    AST_THROW,
    AST_UNWIND_PROTECT,
    AST_DYNAMIC_LET,
    AST_WITH_HANDLER,
    AST_THE,
    AST_ASSURE,
    AST_CONVERT,
    AST_FLET
} ast_type_t;

typedef struct ast_node {
    ast_type_t type;
    islisp_val raw_sexpr;
    union {
        struct {
            islisp_val val;
        } lit;
        struct {
            islisp_val sym;
            int env_idx;      /* -1 if local C var, >= 0 if closure env var */
            int var_id;       /* unique local var id */
        } var;
        struct {
            islisp_val fn;    /* symbol or lambda AST */
            int argc;
            struct ast_node **args;
            bool is_direct;   /* if known global or runtime fn */
        } call;
        struct {
            struct ast_node *test;
            struct ast_node *then_branch;
            struct ast_node *else_branch;
        } if_expr;
        struct {
            int count;
            struct ast_node **exprs;
        } progn;
        struct {
            bool is_star;
            int num_bindings;
            islisp_val *vars;
            struct ast_node **inits;
            int body_count;
            struct ast_node **body;
        } let_expr;
        struct {
            islisp_val sym;
            struct ast_node *val;
        } setq;
        struct {
            islisp_val name;  /* symbol or nil */
            islisp_val params;
            bool has_rest;
            islisp_val rest_var;
            int num_params;
            islisp_val *param_names;
            int body_count;
            struct ast_node **body;
            int fn_id;        /* unique C function ID */
            islisp_val captured_vars; /* list of symbols */
            islisp_val lexical_fns;   /* list of local function entries in scope */
        } lambda;
        struct {
            islisp_val name;
            struct ast_node *lambda_ast;
        } defun_expr;
        struct {
            islisp_val name;
            islisp_val val_sexpr;
        } defglobal;
        struct {
            islisp_val name;
            int body_count;
            struct ast_node **body;
            int block_id;
        } block;
        struct {
            islisp_val name;
            struct ast_node *val;
            int target_block_id;
        } return_from;
        struct {
            int count;
            islisp_val *tags;   /* symbol or nil if expression */
            struct ast_node **items;
            int tagbody_id;
        } tagbody;
        struct {
            islisp_val tag;
            int target_tagbody_id;
        } go;
        struct {
            struct ast_node *tag;
            int count;
            struct ast_node **body;
            int catch_id;
        } catch_expr;
        struct {
            struct ast_node *tag;
            struct ast_node *val;
        } throw_expr;
        struct {
            struct ast_node *protected_expr;
            int cleanup_count;
            struct ast_node **cleanups;
            int unwind_id;
        } unwind;
        struct {
            int num_bindings;
            islisp_val *vars;
            struct ast_node **inits;
            int body_count;
            struct ast_node **body;
        } dynamic_let;
        struct {
            struct ast_node *handler;
            int body_count;
            struct ast_node **body;
        } with_handler;
        struct {
            islisp_val name;
            islisp_val supers;
            islisp_val slot_specs;
            bool is_abstract;
        } defclass_expr;
        struct {
            islisp_val name;
            int num_req;
            bool has_rest;
        } defgeneric_expr;
        struct {
            islisp_val name;
            islisp_method_qualifier_t qual;
            islisp_val specializers;
            bool has_rest;
            struct ast_node *method_lambda;
        } defmethod_expr;
        struct {
            struct ast_node *obj;
            islisp_val target_class;
        } convert_expr;
        struct {
            bool is_labels;
            int num_fns;
            islisp_val *names;
            struct ast_node **lambdas;
            int body_count;
            struct ast_node **body;
        } flet_expr;
    } as;
} ast_node_t;

/* Compiler environment */
typedef struct comp_env {
    struct comp_env *parent;
    islisp_val vars;    /* list of symbols in scope */
    islisp_val fns;     /* list of local function names in scope */
    islisp_val blocks;  /* list of active block names */
    islisp_val tags;    /* list of active tagbody tags */
    bool is_in_lambda;
    islisp_val captured;/* symbols captured from outer scopes */
} comp_env_t;

/* Parser / Reader */
islisp_val islisp_read_file(const char *filename);
islisp_val islisp_read_from_string(const char *str);

/* Macro Expander */
void islisp_init_macros(void);
islisp_val islisp_macroexpand(islisp_val form);
islisp_val islisp_expand_backquote(islisp_val form);

/* AST & Semantic Analysis */
ast_node_t* islisp_parse_ast(islisp_val form, comp_env_t *env);
void islisp_free_ast(ast_node_t *node);

/* Code Generator */
typedef struct {
    FILE *out;
    int var_counter;
    int fn_counter;
    int block_counter;
    int tag_counter;
    int lit_counter;
    islisp_val literals;    /* list of (val . sym_name) */
    islisp_val top_defuns;  /* list of lambda ast nodes for top-level defuns */
    islisp_val closures;    /* list of lambda ast nodes for closures */
} codegen_ctx_t;

bool islisp_compile_file_to_c(const char *in_lsp, const char *out_c);
bool islisp_compile_string_to_c(const char *code, const char *out_c);
bool islisp_compile_c_to_binary(const char *in_c, const char *out_exe);

#endif /* ISLISP_COMPILER_H */
