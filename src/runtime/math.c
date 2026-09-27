#include "islisp_runtime.h"
#include <math.h>
#include <errno.h>

static double to_double(islisp_val v) {
    if (IS_INT(v)) return (double)AS_INT(v);
    if (IS_FLOAT(v)) return ((islisp_float_t*)v)->val;
    islisp_error("Expected a number, got non-number");
    return 0.0;
}

islisp_val islisp_add(int argc, islisp_val *argv) {
    if (argc == 0) return TO_INT(0);
    bool any_float = false;
    int64_t i_sum = 0;
    double f_sum = 0.0;

    for (int i = 0; i < argc; i++) {
        islisp_val v = argv[i];
        if (IS_INT(v)) {
            if (any_float) {
                f_sum += (double)AS_INT(v);
            } else {
                i_sum += AS_INT(v);
            }
        } else if (IS_FLOAT(v)) {
            if (!any_float) {
                any_float = true;
                f_sum = (double)i_sum + ((islisp_float_t*)v)->val;
            } else {
                f_sum += ((islisp_float_t*)v)->val;
            }
        } else {
            islisp_error("+: argument is not a number");
        }
    }
    return any_float ? islisp_make_float(f_sum) : TO_INT(i_sum);
}

islisp_val islisp_sub(int argc, islisp_val *argv) {
    if (argc == 0) {
        islisp_error("-: requires at least 1 argument");
        return TO_INT(0);
    }
    if (argc == 1) {
        islisp_val v = argv[0];
        if (IS_INT(v)) return TO_INT(-AS_INT(v));
        if (IS_FLOAT(v)) return islisp_make_float(-((islisp_float_t*)v)->val);
        islisp_error("-: argument is not a number");
    }

    bool any_float = IS_FLOAT(argv[0]);
    int64_t i_res = IS_INT(argv[0]) ? AS_INT(argv[0]) : 0;
    double f_res = any_float ? ((islisp_float_t*)argv[0])->val : 0.0;

    for (int i = 1; i < argc; i++) {
        islisp_val v = argv[i];
        if (IS_INT(v)) {
            if (any_float) {
                f_res -= (double)AS_INT(v);
            } else {
                i_res -= AS_INT(v);
            }
        } else if (IS_FLOAT(v)) {
            if (!any_float) {
                any_float = true;
                f_res = (double)i_res - ((islisp_float_t*)v)->val;
            } else {
                f_res -= ((islisp_float_t*)v)->val;
            }
        } else {
            islisp_error("-: argument is not a number");
        }
    }
    return any_float ? islisp_make_float(f_res) : TO_INT(i_res);
}

islisp_val islisp_mul(int argc, islisp_val *argv) {
    if (argc == 0) return TO_INT(1);
    bool any_float = false;
    int64_t i_prod = 1;
    double f_prod = 1.0;

    for (int i = 0; i < argc; i++) {
        islisp_val v = argv[i];
        if (IS_INT(v)) {
            if (any_float) {
                f_prod *= (double)AS_INT(v);
            } else {
                i_prod *= AS_INT(v);
            }
        } else if (IS_FLOAT(v)) {
            if (!any_float) {
                any_float = true;
                f_prod = (double)i_prod * ((islisp_float_t*)v)->val;
            } else {
                f_prod *= ((islisp_float_t*)v)->val;
            }
        } else {
            islisp_error("*: argument is not a number");
        }
    }
    return any_float ? islisp_make_float(f_prod) : TO_INT(i_prod);
}

islisp_val islisp_quotient(islisp_val dividend, islisp_val divisor) {
    if (!islisp_numberp(dividend) || !islisp_numberp(divisor)) {
        islisp_error("quotient: arguments must be numbers");
    }
    if (IS_INT(dividend) && IS_INT(divisor)) {
        int64_t b = AS_INT(divisor);
        if (b == 0) {
            islisp_error("division-by-zero");
        }
        int64_t a = AS_INT(dividend);
        if (a % b == 0) {
            return TO_INT(a / b);
        } else {
            return islisp_make_float((double)a / (double)b);
        }
    }
    double a = to_double(dividend);
    double b = to_double(divisor);
    if (b == 0.0) {
        islisp_error("division-by-zero");
    }
    return islisp_make_float(a / b);
}

islisp_val islisp_div(int argc, islisp_val *argv) {
    if (argc < 1) {
        islisp_error("/: requires at least 1 argument");
    }
    if (argc == 1) {
        return islisp_reciprocal(argv[0]);
    }
    islisp_val res = argv[0];
    for (int i = 1; i < argc; i++) {
        res = islisp_quotient(res, argv[i]);
    }
    return res;
}

islisp_val islisp_reciprocal(islisp_val x) {
    if (IS_INT(x)) {
        int64_t n = AS_INT(x);
        if (n == 0) islisp_error("division-by-zero");
        if (n == 1) return TO_INT(1);
        if (n == -1) return TO_INT(-1);
        return islisp_make_float(1.0 / (double)n);
    }
    if (IS_FLOAT(x)) {
        double d = ((islisp_float_t*)x)->val;
        if (d == 0.0) islisp_error("division-by-zero");
        return islisp_make_float(1.0 / d);
    }
    islisp_error("reciprocal: argument must be a number");
    return ISLISP_NIL;
}

islisp_val islisp_num_div(islisp_val z1, islisp_val z2) {
    if (!IS_INT(z1) || !IS_INT(z2)) {
        islisp_error("div: arguments must be integers");
    }
    int64_t a = AS_INT(z1);
    int64_t b = AS_INT(z2);
    if (b == 0) islisp_error("division-by-zero");
    int64_t q = a / b;
    int64_t r = a % b;
    if ((r != 0) && ((a < 0) ^ (b < 0))) {
        q -= 1;
    }
    return TO_INT(q);
}

islisp_val islisp_num_mod(islisp_val z1, islisp_val z2) {
    if (!IS_INT(z1) || !IS_INT(z2)) {
        islisp_error("mod: arguments must be integers");
    }
    int64_t a = AS_INT(z1);
    int64_t b = AS_INT(z2);
    if (b == 0) islisp_error("division-by-zero");
    int64_t r = a % b;
    if ((r != 0) && ((a < 0) ^ (b < 0))) {
        r += b;
    }
    return TO_INT(r);
}

islisp_val islisp_remainder(islisp_val dividend, islisp_val divisor) {
    if (!IS_INT(dividend) || !IS_INT(divisor)) {
        islisp_error("remainder: arguments must be integers");
    }
    int64_t b = AS_INT(divisor);
    if (b == 0) islisp_error("division-by-zero");
    return TO_INT(AS_INT(dividend) % b);
}

islisp_val islisp_abs(islisp_val x) {
    if (IS_INT(x)) {
        int64_t n = AS_INT(x);
        return TO_INT(n < 0 ? -n : n);
    }
    if (IS_FLOAT(x)) {
        return islisp_make_float(fabs(((islisp_float_t*)x)->val));
    }
    islisp_error("abs: argument must be a number");
    return ISLISP_NIL;
}

islisp_val islisp_min(int argc, islisp_val *argv) {
    if (argc < 1) islisp_error("min: requires at least 1 argument");
    islisp_val m = argv[0];
    for (int i = 1; i < argc; i++) {
        if (IS_TRUE(islisp_num_lt(argv[i], m))) {
            m = argv[i];
        }
    }
    return m;
}

islisp_val islisp_max(int argc, islisp_val *argv) {
    if (argc < 1) islisp_error("max: requires at least 1 argument");
    islisp_val m = argv[0];
    for (int i = 1; i < argc; i++) {
        if (IS_TRUE(islisp_num_gt(argv[i], m))) {
            m = argv[i];
        }
    }
    return m;
}

islisp_val islisp_exp(islisp_val x) { return islisp_make_float(exp(to_double(x))); }
islisp_val islisp_log(islisp_val x) { return islisp_make_float(log(to_double(x))); }
islisp_val islisp_sqrt(islisp_val x) { return islisp_make_float(sqrt(to_double(x))); }
islisp_val islisp_sin(islisp_val x) { return islisp_make_float(sin(to_double(x))); }
islisp_val islisp_cos(islisp_val x) { return islisp_make_float(cos(to_double(x))); }
islisp_val islisp_tan(islisp_val x) { return islisp_make_float(tan(to_double(x))); }
islisp_val islisp_atan(islisp_val x) { return islisp_make_float(atan(to_double(x))); }
islisp_val islisp_atan2(islisp_val y, islisp_val x) { return islisp_make_float(atan2(to_double(y), to_double(x))); }
islisp_val islisp_sinh(islisp_val x) { return islisp_make_float(sinh(to_double(x))); }
islisp_val islisp_cosh(islisp_val x) { return islisp_make_float(cosh(to_double(x))); }
islisp_val islisp_tanh(islisp_val x) { return islisp_make_float(tanh(to_double(x))); }
islisp_val islisp_atanh(islisp_val x) { return islisp_make_float(atanh(to_double(x))); }

islisp_val islisp_expt(islisp_val x1, islisp_val x2) {
    if (IS_INT(x1) && IS_INT(x2) && AS_INT(x2) >= 0) {
        int64_t base = AS_INT(x1);
        int64_t exp_val = AS_INT(x2);
        int64_t res = 1;
        while (exp_val > 0) {
            if (exp_val & 1) res *= base;
            base *= base;
            exp_val >>= 1;
        }
        return TO_INT(res);
    }
    return islisp_make_float(pow(to_double(x1), to_double(x2)));
}

islisp_val islisp_floor(islisp_val x) {
    if (IS_INT(x)) return x;
    return TO_INT((int64_t)floor(to_double(x)));
}

islisp_val islisp_ceiling(islisp_val x) {
    if (IS_INT(x)) return x;
    return TO_INT((int64_t)ceil(to_double(x)));
}

islisp_val islisp_truncate(islisp_val x) {
    if (IS_INT(x)) return x;
    return TO_INT((int64_t)trunc(to_double(x)));
}

islisp_val islisp_round(islisp_val x) {
    if (IS_INT(x)) return x;
    return TO_INT((int64_t)round(to_double(x)));
}

static int64_t gcd_impl(int64_t a, int64_t b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        int64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

islisp_val islisp_gcd(islisp_val z1, islisp_val z2) {
    if (!IS_INT(z1) || !IS_INT(z2)) islisp_error("gcd: arguments must be integers");
    return TO_INT(gcd_impl(AS_INT(z1), AS_INT(z2)));
}

islisp_val islisp_lcm(islisp_val z1, islisp_val z2) {
    if (!IS_INT(z1) || !IS_INT(z2)) islisp_error("lcm: arguments must be integers");
    int64_t a = AS_INT(z1);
    int64_t b = AS_INT(z2);
    if (a == 0 || b == 0) return TO_INT(0);
    int64_t g = gcd_impl(a, b);
    return TO_INT(llabs((a / g) * b));
}

islisp_val islisp_isqrt(islisp_val z) {
    if (!IS_INT(z) || AS_INT(z) < 0) islisp_error("isqrt: argument must be a non-negative integer");
    return TO_INT((int64_t)sqrt((double)AS_INT(z)));
}

islisp_val islisp_float(islisp_val x) {
    if (IS_FLOAT(x)) return x;
    if (IS_INT(x)) return islisp_make_float((double)AS_INT(x));
    islisp_error("float: argument must be a number");
    return ISLISP_NIL;
}

islisp_val islisp_parse_number(islisp_val str) {
    if (!IS_STRING(str)) islisp_error("parse-number: argument must be a string");
    const char *s = ((islisp_string_t*)str)->data;
    char *end = NULL;
    /* Check radix prefixes #b, #o, #x */
    if (s[0] == '#' && (s[1] == 'b' || s[1] == 'B')) {
        long long val = strtoll(s + 2, &end, 2);
        if (end != s + 2 && *end == '\0') return TO_INT(val);
    } else if (s[0] == '#' && (s[1] == 'o' || s[1] == 'O')) {
        long long val = strtoll(s + 2, &end, 8);
        if (end != s + 2 && *end == '\0') return TO_INT(val);
    } else if (s[0] == '#' && (s[1] == 'x' || s[1] == 'X')) {
        long long val = strtoll(s + 2, &end, 16);
        if (end != s + 2 && *end == '\0') return TO_INT(val);
    }
    /* Try integer */
    long long ival = strtoll(s, &end, 10);
    if (end != s && *end == '\0') {
        return TO_INT(ival);
    }
    /* Try float */
    double fval = strtod(s, &end);
    if (end != s && *end == '\0') {
        return islisp_make_float(fval);
    }
    islisp_error("parse-number: cannot parse \"%s\"", s);
    return ISLISP_NIL;
}

/* Comparisons */
islisp_val islisp_num_eq(islisp_val a, islisp_val b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) == AS_INT(b));
    return BOOL_VAL(to_double(a) == to_double(b));
}

islisp_val islisp_num_neq(islisp_val a, islisp_val b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) != AS_INT(b));
    return BOOL_VAL(to_double(a) != to_double(b));
}

islisp_val islisp_num_lt(islisp_val a, islisp_val b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) < AS_INT(b));
    return BOOL_VAL(to_double(a) < to_double(b));
}

islisp_val islisp_num_lteq(islisp_val a, islisp_val b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) <= AS_INT(b));
    return BOOL_VAL(to_double(a) <= to_double(b));
}

islisp_val islisp_num_gt(islisp_val a, islisp_val b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) > AS_INT(b));
    return BOOL_VAL(to_double(a) > to_double(b));
}

islisp_val islisp_num_gteq(islisp_val a, islisp_val b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) >= AS_INT(b));
    return BOOL_VAL(to_double(a) >= to_double(b));
}
