#include "islisp_compiler.h"

static void print_usage(const char *prog) {
    printf("CISL: ISLisp-to-C Compiler (ISO/IEC 13816 Standard Compliant)\n");
    printf("Usage:\n");
    printf("  %s [options] <input.lsp>\n", prog);
    printf("  %s -e \"<expression>\"\n", prog);
    printf("\nOptions:\n");
    printf("  -o <file>       Specify output executable or C file name\n");
    printf("  -c              Compile to C source code only (do not invoke GCC)\n");
    printf("  --run           Compile to temporary binary and run immediately\n");
    printf("  -e <expr>       Compile and run ISLisp expression\n");
    printf("  -v, --version   Display compiler version information\n");
    printf("  -h, --help      Display this help message\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *input_file = NULL;
    const char *output_file = NULL;
    const char *expr_str = NULL;
    bool c_only = false;
    bool run_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("cisl version 1.0.0 (ISLisp ISO/IEC 13816 Draft 23.0)\n");
            return 0;
        } else if (strcmp(argv[i], "-c") == 0) {
            c_only = true;
        } else if (strcmp(argv[i], "--run") == 0) {
            run_mode = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "Error: -o requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 < argc) {
                expr_str = argv[++i];
            } else {
                fprintf(stderr, "Error: -e requires an expression argument\n");
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        } else {
            input_file = argv[i];
        }
    }

    if (expr_str) {
        /* Compile expression string to temporary C file, compile to exe, and run */
        char tmp_c[] = "_cisl_tmp_expr.c";
        char tmp_exe[] = "_cisl_tmp_expr.exe";

        if (!islisp_compile_string_to_c(expr_str, tmp_c)) {
            fprintf(stderr, "Compilation failed for expression\n");
            return 1;
        }

        if (!islisp_compile_c_to_binary(tmp_c, tmp_exe)) {
            fprintf(stderr, "C compilation failed for expression\n");
            remove(tmp_c);
            return 1;
        }

        int ret = system(tmp_exe);
        remove(tmp_c);
        remove(tmp_exe);
        return ret;
    }

    if (!input_file) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    char default_c[512];
    char default_exe[512];

    const char *dot = strrchr(input_file, '.');
    size_t base_len = dot ? (size_t)(dot - input_file) : strlen(input_file);
    if (base_len > sizeof(default_c) - 10) base_len = sizeof(default_c) - 10;
    strncpy(default_c, input_file, base_len);
    default_c[base_len] = '\0';
    strcpy(default_exe, default_c);
    strcat(default_c, ".c");
    strcat(default_exe, ".exe");

    const char *target_c = c_only ? (output_file ? output_file : default_c) : default_c;
    const char *target_exe = output_file ? output_file : default_exe;

    if (!islisp_compile_file_to_c(input_file, target_c)) {
        fprintf(stderr, "Error: Failed to translate %s to C\n", input_file);
        return 1;
    }

    if (c_only) {
        printf("Generated C source code: %s\n", target_c);
        return 0;
    }

    if (!islisp_compile_c_to_binary(target_c, target_exe)) {
        fprintf(stderr, "Error: GCC compilation failed for %s\n", target_c);
        return 1;
    }

    printf("Compiled %s -> %s\n", input_file, target_exe);

    if (run_mode) {
        char run_cmd[512];
        snprintf(run_cmd, sizeof(run_cmd), "%s", target_exe);
#ifdef _WIN32
        for (char *p = run_cmd; *p; p++) {
            if (*p == '/') *p = '\\';
        }
#endif
        int ret = system(run_cmd);
        return ret;
    }

    return 0;
}
