#include "islisp_runtime.h"
#include <ctype.h>

static islisp_stream_t g_std_in = {
    .header = { .type = TYPE_STREAM, .flags = 0, .extra = 0, .len = 0 },
    .kind = STREAM_FILE,
    .file = NULL,
    .str_buf = NULL,
    .str_cap = 0,
    .str_len = 0,
    .str_pos = 0,
    .is_input = true,
    .is_output = false,
    .is_open = true,
    .unread_count = 0
};

static islisp_stream_t g_std_out = {
    .header = { .type = TYPE_STREAM, .flags = 0, .extra = 0, .len = 0 },
    .kind = STREAM_FILE,
    .file = NULL,
    .str_buf = NULL,
    .str_cap = 0,
    .str_len = 0,
    .str_pos = 0,
    .is_input = false,
    .is_output = true,
    .is_open = true,
    .unread_count = 0
};

static islisp_stream_t g_std_err = {
    .header = { .type = TYPE_STREAM, .flags = 0, .extra = 0, .len = 0 },
    .kind = STREAM_FILE,
    .file = NULL,
    .str_buf = NULL,
    .str_cap = 0,
    .str_len = 0,
    .str_pos = 0,
    .is_input = false,
    .is_output = true,
    .is_open = true,
    .unread_count = 0
};

islisp_val islisp_standard_input(void) {
    if (!g_std_in.file) g_std_in.file = stdin;
    return TO_HEAP(&g_std_in);
}

islisp_val islisp_standard_output(void) {
    if (!g_std_out.file) g_std_out.file = stdout;
    return TO_HEAP(&g_std_out);
}

islisp_val islisp_error_output(void) {
    if (!g_std_err.file) g_std_err.file = stderr;
    return TO_HEAP(&g_std_err);
}

islisp_val islisp_open_stream_p(islisp_val stream) {
    if (!IS_STREAM(stream)) return ISLISP_NIL;
    return BOOL_VAL(((islisp_stream_t*)stream)->is_open);
}

islisp_val islisp_input_stream_p(islisp_val stream) {
    if (!IS_STREAM(stream)) return ISLISP_NIL;
    return BOOL_VAL(((islisp_stream_t*)stream)->is_input);
}

islisp_val islisp_output_stream_p(islisp_val stream) {
    if (!IS_STREAM(stream)) return ISLISP_NIL;
    return BOOL_VAL(((islisp_stream_t*)stream)->is_output);
}

islisp_val islisp_open_input_file(islisp_val filename, islisp_val elem_class) {
    (void)elem_class;
    if (!IS_STRING(filename)) islisp_error("open-input-file: filename must be a string");
    FILE *f = fopen(((islisp_string_t*)filename)->data, "rb");
    if (!f) islisp_error("Cannot open input file %s", ((islisp_string_t*)filename)->data);

    islisp_stream_t *s = (islisp_stream_t*)islisp_alloc(sizeof(islisp_stream_t));
    s->header.type = TYPE_STREAM;
    s->kind = STREAM_FILE;
    s->file = f;
    s->is_input = true;
    s->is_output = false;
    s->is_open = true;
    s->unread_count = 0;
    return TO_HEAP(s);
}

islisp_val islisp_open_output_file(islisp_val filename, islisp_val elem_class) {
    (void)elem_class;
    if (!IS_STRING(filename)) islisp_error("open-output-file: filename must be a string");
    FILE *f = fopen(((islisp_string_t*)filename)->data, "wb");
    if (!f) islisp_error("Cannot open output file %s", ((islisp_string_t*)filename)->data);

    islisp_stream_t *s = (islisp_stream_t*)islisp_alloc(sizeof(islisp_stream_t));
    s->header.type = TYPE_STREAM;
    s->kind = STREAM_FILE;
    s->file = f;
    s->is_input = false;
    s->is_output = true;
    s->is_open = true;
    s->unread_count = 0;
    return TO_HEAP(s);
}

islisp_val islisp_open_io_file(islisp_val filename, islisp_val elem_class) {
    (void)elem_class;
    if (!IS_STRING(filename)) islisp_error("open-io-file: filename must be a string");
    FILE *f = fopen(((islisp_string_t*)filename)->data, "r+b");
    if (!f) f = fopen(((islisp_string_t*)filename)->data, "w+b");
    if (!f) islisp_error("Cannot open I/O file %s", ((islisp_string_t*)filename)->data);

    islisp_stream_t *s = (islisp_stream_t*)islisp_alloc(sizeof(islisp_stream_t));
    s->header.type = TYPE_STREAM;
    s->kind = STREAM_FILE;
    s->file = f;
    s->is_input = true;
    s->is_output = true;
    s->is_open = true;
    s->unread_count = 0;
    return TO_HEAP(s);
}

islisp_val islisp_close(islisp_val stream_val) {
    if (!IS_STREAM(stream_val)) islisp_error("close: argument must be a stream");
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    if (s->is_open) {
        if (s->kind == STREAM_FILE && s->file && s->file != stdin && s->file != stdout && s->file != stderr) {
            fclose(s->file);
            s->file = NULL;
        }
        s->is_open = false;
    }
    return ISLISP_T;
}

islisp_val islisp_finish_output(islisp_val stream_val) {
    if (!IS_STREAM(stream_val)) islisp_error("finish-output: argument must be a stream");
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    if (s->kind == STREAM_FILE && s->file) {
        fflush(s->file);
    }
    return ISLISP_NIL;
}

islisp_val islisp_create_string_input_stream(islisp_val str_val) {
    if (!IS_STRING(str_val)) islisp_error("create-string-input-stream: argument must be a string");
    islisp_string_t *str = (islisp_string_t*)str_val;
    islisp_stream_t *s = (islisp_stream_t*)islisp_alloc(sizeof(islisp_stream_t));
    s->header.type = TYPE_STREAM;
    s->kind = STREAM_STRING_IN;
    s->str_buf = strdup(str->data);
    s->str_len = str->header.len;
    s->str_pos = 0;
    s->is_input = true;
    s->is_output = false;
    s->is_open = true;
    s->unread_count = 0;
    return TO_HEAP(s);
}

islisp_val islisp_create_string_output_stream(void) {
    islisp_stream_t *s = (islisp_stream_t*)islisp_alloc(sizeof(islisp_stream_t));
    s->header.type = TYPE_STREAM;
    s->kind = STREAM_STRING_OUT;
    s->str_cap = 128;
    s->str_buf = (char*)malloc(s->str_cap);
    s->str_buf[0] = '\0';
    s->str_len = 0;
    s->str_pos = 0;
    s->is_input = false;
    s->is_output = true;
    s->is_open = true;
    s->unread_count = 0;
    return TO_HEAP(s);
}

islisp_val islisp_get_output_stream_string(islisp_val stream_val) {
    if (!IS_STREAM(stream_val)) islisp_error("get-output-stream-string: argument must be a stream");
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    if (s->kind != STREAM_STRING_OUT) islisp_error("get-output-stream-string: stream is not a string output stream");
    islisp_val res = islisp_make_string_len(s->str_buf, s->str_len);
    s->str_len = 0;
    s->str_buf[0] = '\0';
    return res;
}

static int stream_getc(islisp_stream_t *s) {
    if (!s->is_open || !s->is_input) return EOF;
    if (s->unread_count > 0) {
        return s->unread_buf[--s->unread_count];
    }
    if (s->kind == STREAM_FILE) {
        return fgetc(s->file);
    } else if (s->kind == STREAM_STRING_IN) {
        if (s->str_pos < s->str_len) {
            return (unsigned char)s->str_buf[s->str_pos++];
        }
        return EOF;
    }
    return EOF;
}

static void stream_ungetc(int c, islisp_stream_t *s) {
    if (c == EOF) return;
    if (s->unread_count < 16) {
        s->unread_buf[s->unread_count++] = c;
    }
}

static void stream_putc(int c, islisp_stream_t *s) {
    if (!s->is_open || !s->is_output) return;
    if (s->kind == STREAM_FILE) {
        fputc(c, s->file);
    } else if (s->kind == STREAM_STRING_OUT) {
        if (s->str_len + 2 >= s->str_cap) {
            s->str_cap *= 2;
            s->str_buf = (char*)realloc(s->str_buf, s->str_cap);
        }
        s->str_buf[s->str_len++] = (char)c;
        s->str_buf[s->str_len] = '\0';
    }
}

static void stream_puts(const char *str, islisp_stream_t *s) {
    while (*str) {
        stream_putc(*str++, s);
    }
}

islisp_val islisp_stream_ready_p(islisp_val stream_val) {
    if (!IS_STREAM(stream_val)) return ISLISP_NIL;
    return ISLISP_T;
}

islisp_val islisp_probe_file(islisp_val filename) {
    if (!IS_STRING(filename)) islisp_error("probe-file: filename must be a string");
    FILE *f = fopen(((islisp_string_t*)filename)->data, "rb");
    if (f) {
        fclose(f);
        return ISLISP_T;
    }
    return ISLISP_NIL;
}

islisp_val islisp_file_position(islisp_val stream_val) {
    if (!IS_STREAM(stream_val)) islisp_error("file-position: argument must be a stream");
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    if (s->kind == STREAM_FILE && s->file) {
        return TO_INT(ftell(s->file));
    }
    if (s->kind == STREAM_STRING_IN) {
        return TO_INT(s->str_pos);
    }
    return TO_INT(0);
}

islisp_val islisp_set_file_position(islisp_val stream_val, islisp_val pos_val) {
    if (!IS_STREAM(stream_val)) islisp_error("set-file-position: argument must be a stream");
    if (!IS_INT(pos_val)) islisp_error("set-file-position: position must be an integer");
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    int64_t pos = AS_INT(pos_val);
    if (s->kind == STREAM_FILE && s->file) {
        fseek(s->file, (long)pos, SEEK_SET);
        return pos_val;
    }
    if (s->kind == STREAM_STRING_IN) {
        if (pos >= 0 && (size_t)pos <= s->str_len) {
            s->str_pos = (size_t)pos;
            return pos_val;
        }
    }
    return ISLISP_NIL;
}

islisp_val islisp_file_length(islisp_val filename, islisp_val elem_class) {
    (void)elem_class;
    if (!IS_STRING(filename)) islisp_error("file-length: filename must be a string");
    FILE *f = fopen(((islisp_string_t*)filename)->data, "rb");
    if (!f) return ISLISP_NIL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fclose(f);
    return TO_INT(len);
}

/* Character & Byte I/O */

islisp_val islisp_read_char(islisp_val stream_val, islisp_val eos_err, islisp_val eos_val) {
    islisp_stream_t *s = (stream_val == ISLISP_UNBOUND) ? &g_std_in : (islisp_stream_t*)stream_val;
    int c = stream_getc(s);
    if (c == EOF) {
        if (eos_err == ISLISP_UNBOUND || IS_TRUE(eos_err)) {
            islisp_error("end-of-stream on stream");
        }
        return (eos_val == ISLISP_UNBOUND) ? ISLISP_NIL : eos_val;
    }
    return TO_CHAR(c);
}

islisp_val islisp_preview_char(islisp_val stream_val, islisp_val eos_err, islisp_val eos_val) {
    islisp_stream_t *s = (stream_val == ISLISP_UNBOUND) ? &g_std_in : (islisp_stream_t*)stream_val;
    int c = stream_getc(s);
    if (c == EOF) {
        if (eos_err == ISLISP_UNBOUND || IS_TRUE(eos_err)) {
            islisp_error("end-of-stream on stream");
        }
        return (eos_val == ISLISP_UNBOUND) ? ISLISP_NIL : eos_val;
    }
    stream_ungetc(c, s);
    return TO_CHAR(c);
}

islisp_val islisp_read_line(islisp_val stream_val, islisp_val eos_err, islisp_val eos_val) {
    islisp_stream_t *s = (stream_val == ISLISP_UNBOUND) ? &g_std_in : (islisp_stream_t*)stream_val;
    int c = stream_getc(s);
    if (c == EOF) {
        if (eos_err == ISLISP_UNBOUND || IS_TRUE(eos_err)) {
            islisp_error("end-of-stream on stream");
        }
        return (eos_val == ISLISP_UNBOUND) ? ISLISP_NIL : eos_val;
    }
    char buf[1024];
    size_t len = 0;
    while (c != EOF && c != '\n') {
        if (c != '\r') {
            if (len < sizeof(buf) - 1) buf[len++] = (char)c;
        }
        c = stream_getc(s);
    }
    buf[len] = '\0';
    return islisp_make_string_len(buf, len);
}

islisp_val islisp_read_byte(islisp_val stream_val, islisp_val eos_err, islisp_val eos_val) {
    return islisp_read_char(stream_val, eos_err, eos_val);
}

islisp_val islisp_write_byte(islisp_val byte_val, islisp_val stream_val) {
    if (!IS_INT(byte_val)) islisp_error("write-byte: argument must be an integer");
    islisp_stream_t *s = (stream_val == ISLISP_UNBOUND) ? &g_std_out : (islisp_stream_t*)stream_val;
    stream_putc((int)AS_INT(byte_val), s);
    return byte_val;
}

/* Printing & Formatting */

islisp_val islisp_format_char(islisp_val stream_val, islisp_val ch) {
    if (!IS_CHAR(ch)) islisp_error("format-char: second argument must be a character");
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    stream_putc(AS_CHAR(ch), s);
    return ISLISP_NIL;
}

islisp_val islisp_format_float(islisp_val stream_val, islisp_val f_val) {
    if (!IS_FLOAT(f_val)) islisp_error("format-float: second argument must be a float");
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.16g", ((islisp_float_t*)f_val)->val);
    stream_puts(buf, s);
    return ISLISP_NIL;
}

islisp_val islisp_format_fresh_line(islisp_val stream_val) {
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    stream_putc('\n', s);
    return ISLISP_NIL;
}

islisp_val islisp_format_integer(islisp_val stream_val, islisp_val n_val, islisp_val radix_val) {
    if (!IS_INT(n_val)) islisp_error("format-integer: second argument must be an integer");
    int radix = 10;
    if (radix_val != ISLISP_UNBOUND && IS_INT(radix_val)) radix = (int)AS_INT(radix_val);
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    char buf[128];
    int64_t n = AS_INT(n_val);
    if (radix == 10) {
        snprintf(buf, sizeof(buf), "%lld", (long long)n);
    } else if (radix == 16) {
        snprintf(buf, sizeof(buf), "%llX", (unsigned long long)n);
    } else if (radix == 8) {
        snprintf(buf, sizeof(buf), "%llo", (unsigned long long)n);
    } else if (radix == 2) {
        char bits[65];
        uint64_t u = (uint64_t)n;
        int i = 0;
        if (u == 0) bits[i++] = '0';
        else {
            while (u > 0) {
                bits[i++] = (u & 1) ? '1' : '0';
                u >>= 1;
            }
        }
        for (int j = 0; j < i; j++) buf[j] = bits[i - 1 - j];
        buf[i] = '\0';
    } else {
        snprintf(buf, sizeof(buf), "%lld", (long long)n);
    }
    stream_puts(buf, s);
    return ISLISP_NIL;
}

islisp_val islisp_format_tab(islisp_val stream_val, islisp_val col_val) {
    (void)col_val;
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    stream_putc('\t', s);
    return ISLISP_NIL;
}

islisp_val islisp_format_object(islisp_val stream_val, islisp_val obj, islisp_val escape_p) {
    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    bool esc = (escape_p != ISLISP_UNBOUND) && IS_TRUE(escape_p);

    if (IS_NIL(obj)) {
        stream_puts("()", s);
    } else if (IS_T(obj)) {
        stream_puts("t", s);
    } else if (IS_INT(obj)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%lld", (long long)AS_INT(obj));
        stream_puts(buf, s);
    } else if (IS_FLOAT(obj)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.16g", ((islisp_float_t*)obj)->val);
        stream_puts(buf, s);
    } else if (IS_CHAR(obj)) {
        int c = AS_CHAR(obj);
        if (esc) {
            if (c == ' ') stream_puts("#\\space", s);
            else if (c == '\n') stream_puts("#\\newline", s);
            else {
                stream_puts("#\\", s);
                stream_putc(c, s);
            }
        } else {
            stream_putc(c, s);
        }
    } else if (IS_STRING(obj)) {
        islisp_string_t *str = (islisp_string_t*)obj;
        if (esc) {
            stream_putc('"', s);
            for (uint32_t i = 0; i < str->header.len; i++) {
                char ch = str->data[i];
                if (ch == '"') stream_puts("\\\"", s);
                else if (ch == '\\') stream_puts("\\\\", s);
                else if (ch == '\n') stream_puts("\\n", s);
                else if (ch == '\t') stream_puts("\\t", s);
                else stream_putc(ch, s);
            }
            stream_putc('"', s);
        } else {
            stream_puts(str->data, s);
        }
    } else if (IS_SYMBOL(obj)) {
        stream_puts(((islisp_symbol_t*)obj)->name, s);
    } else if (IS_CONS(obj)) {
        stream_putc('(', s);
        islisp_val p = obj;
        bool first = true;
        while (IS_CONS(p)) {
            if (!first) stream_putc(' ', s);
            first = false;
            islisp_format_object(stream_val, CAR(p), escape_p);
            p = CDR(p);
        }
        if (!IS_NIL(p)) {
            stream_puts(" . ", s);
            islisp_format_object(stream_val, p, escape_p);
        }
        stream_putc(')', s);
    } else if (IS_VECTOR(obj)) {
        islisp_vector_t *vec = (islisp_vector_t*)obj;
        stream_puts("#(", s);
        for (uint32_t i = 0; i < vec->header.len; i++) {
            if (i > 0) stream_putc(' ', s);
            islisp_format_object(stream_val, vec->data[i], escape_p);
        }
        stream_putc(')', s);
    } else if (IS_CLOSURE(obj)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "#<FUNCTION %s>", ((islisp_closure_t*)obj)->name);
        stream_puts(buf, s);
    } else if (IS_GENERIC(obj)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "#<GENERIC %s>", ((islisp_symbol_t*)((islisp_generic_function_t*)obj)->name)->name);
        stream_puts(buf, s);
    } else if (IS_CLASS(obj)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "#<CLASS %s>", ((islisp_symbol_t*)((islisp_class_t*)obj)->name)->name);
        stream_puts(buf, s);
    } else if (IS_INSTANCE(obj)) {
        islisp_instance_t *inst = (islisp_instance_t*)obj;
        char buf[128];
        snprintf(buf, sizeof(buf), "#<INSTANCE OF %s>", ((islisp_symbol_t*)((islisp_class_t*)inst->class_obj)->name)->name);
        stream_puts(buf, s);
    } else if (IS_CONDITION(obj)) {
        islisp_condition_t *cond = (islisp_condition_t*)obj;
        char buf[256];
        snprintf(buf, sizeof(buf), "#<CONDITION: %s>", cond->message ? cond->message : "unknown");
        stream_puts(buf, s);
    } else {
        stream_puts("#<OBJECT>", s);
    }
    return ISLISP_NIL;
}

islisp_val islisp_format(islisp_val stream_val, islisp_val fmt_str, int argc, islisp_val *argv) {
    if (!IS_STREAM(stream_val)) islisp_error("format: first argument must be a stream");
    if (!IS_STRING(fmt_str)) islisp_error("format: second argument must be a format string");

    islisp_stream_t *s = (islisp_stream_t*)stream_val;
    const char *fmt = ((islisp_string_t*)fmt_str)->data;
    int arg_idx = 0;

    while (*fmt) {
        if (*fmt == '~') {
            fmt++;
            char dir = (char)toupper((unsigned char)*fmt);
            if (dir == 'A') {
                if (arg_idx < argc) {
                    islisp_format_object(stream_val, argv[arg_idx++], ISLISP_NIL);
                }
            } else if (dir == 'S') {
                if (arg_idx < argc) {
                    islisp_format_object(stream_val, argv[arg_idx++], ISLISP_T);
                }
            } else if (dir == 'D') {
                if (arg_idx < argc) {
                    islisp_format_integer(stream_val, argv[arg_idx++], TO_INT(10));
                }
            } else if (dir == 'X') {
                if (arg_idx < argc) {
                    islisp_format_integer(stream_val, argv[arg_idx++], TO_INT(16));
                }
            } else if (dir == 'O') {
                if (arg_idx < argc) {
                    islisp_format_integer(stream_val, argv[arg_idx++], TO_INT(8));
                }
            } else if (dir == 'B') {
                if (arg_idx < argc) {
                    islisp_format_integer(stream_val, argv[arg_idx++], TO_INT(2));
                }
            } else if (dir == 'F') {
                if (arg_idx < argc) {
                    islisp_format_float(stream_val, argv[arg_idx++]);
                }
            } else if (dir == 'C') {
                if (arg_idx < argc) {
                    islisp_format_char(stream_val, argv[arg_idx++]);
                }
            } else if (dir == '%') {
                stream_putc('\n', s);
            } else if (dir == '&') {
                islisp_format_fresh_line(stream_val);
            } else if (dir == '~') {
                stream_putc('~', s);
            } else if (dir == 'T') {
                islisp_format_tab(stream_val, TO_INT(8));
            } else if (dir == '\0') {
                break;
            } else {
                stream_putc('~', s);
                stream_putc(*fmt, s);
            }
            if (*fmt) fmt++;
        } else {
            stream_putc(*fmt++, s);
        }
    }
    return ISLISP_NIL;
}

/* S-expression Reader implementation */

static void skip_whitespace_and_comments(islisp_stream_t *s) {
    while (1) {
        int c = stream_getc(s);
        if (c == EOF) return;
        if (isspace(c)) continue;
        if (c == ';') {
            /* Line comment: skip until newline or EOF */
            while ((c = stream_getc(s)) != EOF && c != '\n');
            continue;
        }
        if (c == '#') {
            int c2 = stream_getc(s);
            if (c2 == '|') {
                /* Block comment #| ... |# (nestable) */
                int depth = 1;
                while (depth > 0) {
                    int ch = stream_getc(s);
                    if (ch == EOF) return;
                    if (ch == '#' && (c2 = stream_getc(s)) == '|') {
                        depth++;
                    } else if (ch == '|' && (c2 = stream_getc(s)) == '#') {
                        depth--;
                    }
                }
                continue;
            } else {
                stream_ungetc(c2, s);
                stream_ungetc('#', s);
                return;
            }
        }
        stream_ungetc(c, s);
        return;
    }
}

static islisp_val read_sexpr(islisp_stream_t *s, bool *is_dot, bool *is_rparen);

static islisp_val read_list(islisp_stream_t *s) {
    islisp_val head = ISLISP_NIL;
    islisp_val *tail = &head;

    while (1) {
        skip_whitespace_and_comments(s);
        bool is_dot = false, is_rparen = false;
        islisp_val elem = read_sexpr(s, &is_dot, &is_rparen);
        if (elem == ISLISP_EOF) {
            break;
        }
        if (is_rparen) {
            break;
        }
        if (is_dot) {
            /* Dotted pair: read next element and closing paren */
            bool dot2 = false, rparen2 = false;
            islisp_val rest_val = read_sexpr(s, &dot2, &rparen2);
            *tail = rest_val;
            skip_whitespace_and_comments(s);
            int close_p = stream_getc(s);
            if (close_p != ')') {
                islisp_error("Missing closing parenthesis in dotted list");
            }
            break;
        }
        islisp_val cell = islisp_cons(elem, ISLISP_NIL);
        *tail = cell;
        tail = &(AS_CONS(cell)->cdr);
    }
    return head;
}

static islisp_val read_sexpr(islisp_stream_t *s, bool *is_dot, bool *is_rparen) {
    skip_whitespace_and_comments(s);
    int c = stream_getc(s);
    if (c == EOF) return ISLISP_EOF;

    if (c == ')') {
        if (is_rparen) *is_rparen = true;
        return ISLISP_NIL;
    }
    if (c == '(') {
        return read_list(s);
    }
    if (c == '\'') {
        islisp_val quoted = read_sexpr(s, NULL, NULL);
        return islisp_cons(islisp_intern("quote"), islisp_cons(quoted, ISLISP_NIL));
    }
    if (c == '`') {
        islisp_val quoted = read_sexpr(s, NULL, NULL);
        return islisp_cons(islisp_intern("quasiquote"), islisp_cons(quoted, ISLISP_NIL));
    }
    if (c == ',') {
        int next_c = stream_getc(s);
        if (next_c == '@') {
            islisp_val unquoted = read_sexpr(s, NULL, NULL);
            return islisp_cons(islisp_intern("unquote-splicing"), islisp_cons(unquoted, ISLISP_NIL));
        } else {
            stream_ungetc(next_c, s);
            islisp_val unquoted = read_sexpr(s, NULL, NULL);
            return islisp_cons(islisp_intern("unquote"), islisp_cons(unquoted, ISLISP_NIL));
        }
    }
    if (c == '"') {
        /* String literal */
        char buf[4096];
        size_t len = 0;
        while ((c = stream_getc(s)) != EOF && c != '"') {
            if (c == '\\') {
                int esc = stream_getc(s);
                if (esc == 'n') c = '\n';
                else if (esc == 't') c = '\t';
                else if (esc == 'r') c = '\r';
                else if (esc == '"') c = '"';
                else if (esc == '\\') c = '\\';
                else c = esc;
            }
            if (len < sizeof(buf) - 1) buf[len++] = (char)c;
        }
        buf[len] = '\0';
        return islisp_make_string_len(buf, len);
    }
    if (c == '#') {
        int c2 = stream_getc(s);
        if (c2 == '(') {
            /* Vector literal #( ... ) */
            islisp_val lst = read_list(s);
            int len = (int)AS_INT(islisp_length(lst));
            islisp_val vec = islisp_make_vector(len, ISLISP_NIL);
            islisp_vector_t *v = (islisp_vector_t*)vec;
            islisp_val p = lst;
            for (int i = 0; i < len; i++) {
                v->data[i] = CAR(p);
                p = CDR(p);
            }
            return vec;
        }
        if (c2 == '\'') {
            /* #'fn */
            islisp_val fn_name = read_sexpr(s, NULL, NULL);
            return islisp_cons(islisp_intern("function"), islisp_cons(fn_name, ISLISP_NIL));
        }
        if (c2 == '\\') {
            /* Character literal */
            char name_buf[64];
            size_t nlen = 0;
            int ch = stream_getc(s);
            name_buf[nlen++] = (char)ch;
            while ((ch = stream_getc(s)) != EOF && !isspace(ch) && ch != '(' && ch != ')' && ch != ';') {
                if (nlen < sizeof(name_buf) - 1) name_buf[nlen++] = (char)ch;
            }
            stream_ungetc(ch, s);
            name_buf[nlen] = '\0';
            if (nlen == 1) {
                return TO_CHAR((unsigned char)name_buf[0]);
            }
            if (strcasecmp(name_buf, "space") == 0) return TO_CHAR(' ');
            if (strcasecmp(name_buf, "newline") == 0) return TO_CHAR('\n');
            if (strcasecmp(name_buf, "tab") == 0) return TO_CHAR('\t');
            if (strcasecmp(name_buf, "return") == 0) return TO_CHAR('\r');
            return TO_CHAR((unsigned char)name_buf[0]);
        }
        if (c2 == 'b' || c2 == 'B' || c2 == 'o' || c2 == 'O' || c2 == 'x' || c2 == 'X') {
            char num_buf[128];
            num_buf[0] = '#';
            num_buf[1] = (char)c2;
            size_t nlen = 2;
            int ch;
            while ((ch = stream_getc(s)) != EOF && !isspace(ch) && ch != '(' && ch != ')' && ch != ';') {
                if (nlen < sizeof(num_buf) - 1) num_buf[nlen++] = (char)ch;
            }
            stream_ungetc(ch, s);
            num_buf[nlen] = '\0';
            return islisp_parse_number(islisp_make_string_len(num_buf, nlen));
        }
    }

    /* Symbol or number */
    char token[256];
    size_t tlen = 0;
    bool escaped = false;

    if (c == '|') {
        /* Exact case symbol */
        escaped = true;
        while ((c = stream_getc(s)) != EOF && c != '|') {
            if (tlen < sizeof(token) - 1) token[tlen++] = (char)c;
        }
        token[tlen] = '\0';
        return islisp_intern(token);
    }

    token[tlen++] = (char)c;
    while ((c = stream_getc(s)) != EOF && !isspace(c) && c != '(' && c != ')' && c != ';' && c != '"' && c != '\'') {
        if (tlen < sizeof(token) - 1) token[tlen++] = (char)c;
    }
    stream_ungetc(c, s);
    token[tlen] = '\0';

    if (tlen == 1 && token[0] == '.') {
        if (is_dot) *is_dot = true;
        return ISLISP_NIL;
    }

    /* Check if token is a number */
    char *end = NULL;
    long long ival = strtoll(token, &end, 10);
    if (end != token && *end == '\0') {
        return TO_INT(ival);
    }
    double fval = strtod(token, &end);
    if (end != token && *end == '\0' && (strchr(token, '.') || strchr(token, 'e') || strchr(token, 'E'))) {
        return islisp_make_float(fval);
    }

    /* Symbol: fold to lowercase if not escaped */
    if (!escaped) {
        for (size_t i = 0; i < tlen; i++) {
            token[i] = (char)tolower((unsigned char)token[i]);
        }
    }
    if (strcmp(token, "nil") == 0) return ISLISP_NIL;
    if (strcmp(token, "t") == 0) return ISLISP_T;

    return islisp_intern(token);
}

islisp_val islisp_read(islisp_val stream_val, islisp_val eos_err, islisp_val eos_val) {
    islisp_stream_t *s = (stream_val == ISLISP_UNBOUND) ? &g_std_in : (islisp_stream_t*)stream_val;
    islisp_val val = read_sexpr(s, NULL, NULL);
    if (val == ISLISP_EOF) {
        if (eos_err == ISLISP_UNBOUND || IS_TRUE(eos_err)) {
            islisp_error("end-of-stream on stream");
        }
        return (eos_val == ISLISP_UNBOUND) ? ISLISP_NIL : eos_val;
    }
    return val;
}
