CC = gcc
AR = ar
CFLAGS ?= -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS ?= -lm

RUNTIME_SRCS = src/runtime/runtime.c \
               src/runtime/math.c \
               src/runtime/list.c \
               src/runtime/string.c \
               src/runtime/io.c \
               src/runtime/error.c \
               src/runtime/builtins.c \
               src/runtime/ilos.c

COMPILER_SRCS = src/compiler/ast.c \
                src/compiler/codegen.c \
                src/compiler/macro.c \
                src/compiler/reader.c

MAIN_SRC = src/main.c

RUNTIME_OBJS = $(RUNTIME_SRCS:.c=.o)
COMPILER_OBJS = $(COMPILER_SRCS:.c=.o)
MAIN_OBJ = $(MAIN_SRC:.c=.o)

TARGET = cisl.exe
RUNTIME_LIB = libislisp_rt.a

all: $(RUNTIME_LIB) $(TARGET)

$(RUNTIME_LIB): $(RUNTIME_OBJS)
	$(AR) rcs $@ $^

$(TARGET): $(MAIN_OBJ) $(COMPILER_OBJS) $(RUNTIME_LIB)
	$(CC) $(CFLAGS) -o $@ $(MAIN_OBJ) $(COMPILER_OBJS) $(RUNTIME_LIB) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET) $(RUNTIME_LIB)
	$(TARGET) --run tests/test_basics.lsp
	$(TARGET) --run tests/test_control.lsp
	$(TARGET) --run tests/test_functions.lsp
	$(TARGET) --run tests/test_ilos.lsp
	$(TARGET) --run tests/test_gc.lsp
	$(TARGET) --run tests/test_euler.lsp

clean:
	-rm -f src/runtime/*.o src/compiler/*.o src/*.o $(TARGET) $(RUNTIME_LIB)
	-rm -f tests/*.exe tests/*.c _cisl_tmp*

.PHONY: all test clean
