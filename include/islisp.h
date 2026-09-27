#ifndef ISLISP_H
#define ISLISP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#define strdup _strdup
#else
#include <unistd.h>
#endif

typedef uintptr_t islisp_val;

#define ISLISP_TAG_INT_MASK      0x1
#define ISLISP_TAG_INT           0x1

#define ISLISP_TAG_PTR_MASK      0x7
#define ISLISP_TAG_HEAP          0x0
#define ISLISP_TAG_CONS          0x2
#define ISLISP_TAG_CHAR          0x4
#define ISLISP_TAG_SPECIAL       0x6

#define IS_INT(v)       (((uintptr_t)(v) & ISLISP_TAG_INT_MASK) == ISLISP_TAG_INT)
#define IS_CONS(v)      (((uintptr_t)(v) & ISLISP_TAG_PTR_MASK) == ISLISP_TAG_CONS)
#define IS_CHAR(v)      (((uintptr_t)(v) & ISLISP_TAG_PTR_MASK) == ISLISP_TAG_CHAR)
#define IS_SPECIAL(v)   (((uintptr_t)(v) & ISLISP_TAG_PTR_MASK) == ISLISP_TAG_SPECIAL)
#define IS_HEAP(v)      (((uintptr_t)(v) & ISLISP_TAG_PTR_MASK) == ISLISP_TAG_HEAP && (v) != 0)

#define AS_INT(v)       ((int64_t)(((intptr_t)(v)) >> 1))
#define TO_INT(n)       ((islisp_val)(((intptr_t)(n) << 1) | 1))

#define AS_CHAR(v)      ((int)(((uintptr_t)(v)) >> 3))
#define TO_CHAR(c)      ((islisp_val)((((uintptr_t)(c)) << 3) | ISLISP_TAG_CHAR))

#define ISLISP_NIL      ((islisp_val)(0x06))
#define ISLISP_T        ((islisp_val)(0x0E))
#define ISLISP_UNBOUND  ((islisp_val)(0x16))
#define ISLISP_EOF      ((islisp_val)(0x1E))

#define IS_NIL(v)       ((v) == ISLISP_NIL)
#define IS_T(v)         ((v) == ISLISP_T)
#define IS_TRUE(v)      ((v) != ISLISP_NIL)
#define BOOL_VAL(b)     ((b) ? ISLISP_T : ISLISP_NIL)

#define AS_CONS(v)      ((struct islisp_cons*)((uintptr_t)(v) ^ ISLISP_TAG_CONS))
#define TO_CONS(ptr)    ((islisp_val)(((uintptr_t)(ptr)) | ISLISP_TAG_CONS))

#define AS_HEAP(v)      ((struct islisp_heap_obj*)(v))
#define TO_HEAP(ptr)    ((islisp_val)(ptr))

#define CAR(v)          (AS_CONS(v)->car)
#define CDR(v)          (AS_CONS(v)->cdr)

typedef enum {
    TYPE_FLOAT = 1,
    TYPE_STRING,
    TYPE_SYMBOL,
    TYPE_VECTOR,
    TYPE_ARRAY,
    TYPE_CLOSURE,
    TYPE_STREAM,
    TYPE_CLASS,
    TYPE_INSTANCE,
    TYPE_GENERIC_FUNCTION,
    TYPE_CONDITION,
    TYPE_METHOD
} islisp_type_t;

typedef struct islisp_header {
    uint8_t type;
    uint8_t flags;
    uint16_t extra;
    uint32_t len;
} islisp_header_t;

typedef struct islisp_cons {
    islisp_val car;
    islisp_val cdr;
} islisp_cons_t;

typedef struct islisp_heap_obj {
    islisp_header_t header;
} islisp_heap_obj_t;

#define HEAP_TYPE(v)    (AS_HEAP(v)->header.type)
#define HEAP_LEN(v)     (AS_HEAP(v)->header.len)

#define IS_FLOAT(v)     (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_FLOAT)
#define IS_STRING(v)    (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_STRING)
#define IS_SYMBOL(v)    (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_SYMBOL)
#define IS_VECTOR(v)    (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_VECTOR)
#define IS_ARRAY(v)     (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_ARRAY)
#define IS_CLOSURE(v)   (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_CLOSURE)
#define IS_STREAM(v)    (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_STREAM)
#define IS_CLASS(v)     (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_CLASS)
#define IS_INSTANCE(v)  (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_INSTANCE)
#define IS_GENERIC(v)   (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_GENERIC_FUNCTION)
#define IS_CONDITION(v) (IS_HEAP(v) && HEAP_TYPE(v) == TYPE_CONDITION)

#endif /* ISLISP_H */
