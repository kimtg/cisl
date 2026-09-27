# CISL: ISLisp-to-C Compiler

**CISL** is an optimizing, standalone compiler and runtime system for ISLisp (ISO/IEC 13816:1997 / 13816:2007, based on Working Draft 23.0), implemented entirely in standard C99.

CISL translates ISLisp source programs directly into clean, efficient ANSI C code, compiling and linking them into standalone native executables using GCC/MinGW.

---

## Features

- **Full ISLisp Special Form & Core Language Support**:
  - Lexical scoping & local variables: `let`, `let*`, `setq`.
  - Local functions: `flet`, `labels` (with mutual recursion and direct C calls).
  - First-class closures & higher-order functions: `lambda`, `function` / `#'`, `funcall`, `apply`.
  - Non-local control flow: `block` / `return-from`, `catch` / `throw`, `tagbody` / `go`.
  - Unwinding & cleanups: `unwind-protect` properly coordinated with `longjmp` stack unwinding.
  - Dynamic binding: `defdynamic`, `dynamic`, `dynamic-let`, `set-dynamic`.
  - Error handling: `with-handler`, `signal-condition`, `condition-continuable`, `continue-condition`.
  - Macros & Quasiquotation: `defmacro`, backquote (`` ` ``), comma (`,`), splicing (`,@`), `macroexpand`.
  - Control macros: `cond`, `case`, `while`, `for`, `and`, `or`.
- **ILOS (ISLisp Object System)**:
  - Class definitions: `defclass` supporting single & multiple inheritance, class precedence list (CPL) linearization, slot descriptors with `:initarg`.
  - Instance lifecycle: `create`, `initialize-object`.
  - Slot manipulation: `slot-value`, `set-slot-value`, `(setf (slot-value ...))` and `slot-boundp`.
  - Class inspection & hierarchy predicates: `class-of`, `instancep`, `subclassp`, `(class <name>)`.
  - Generic functions & method dispatch: `defgeneric`, `defmethod`.
  - Standard method combination: primary methods, `:before` methods (most-specific-first), `:after` methods (least-specific-first), and `:around` methods.
  - Method continuation: `call-next-method`, `next-method-p`.
- **Non-Moving Mark-and-Sweep Garbage Collector**:
  - Conservative stack scanning using `setjmp` CPU register flush and stack bounds traversal.
  - Fast O(1) pointer validation using heap range bounding and open-addressing Fibonacci hash table.
  - Cycle-safe collection and $O(1)$ stack usage for deep list traversal.
  - Automatic collection triggering with adaptive heap sizing (doubling threshold on live survival).
  - Explicit GC root registration API (`islisp_gc_register_root`, `islisp_gc_register_root_array`) protecting global compiled literals.
  - Inspection and trigger builtins: `(gc)` and `(gc-stats)`.
- **Runtime & Value Representation**:
  - Tagged 64-bit pointer representation (`islisp_val`) supporting immediate integers, characters, booleans, and nil without heap allocation.
  - Heap-allocated types: cons cells, bignums/floats, symbols, strings, general vectors, multidimensional arrays, closures, streams, conditions, classes, generic functions, and instances.
  - Topologically sorted recursive literal initialization with automatic root registration.
  - Fast, re-entrant formatted I/O (`format`, `islisp_read`, nested lists, unread buffer stack).

---

## Project Architecture

```
cisl/
├── include/
│   ├── islisp.h              # Value representation, type tags, heap headers
│   ├── islisp_runtime.h      # Runtime library headers (classes, frames, memory, builtins)
│   └── islisp_compiler.h     # Compiler AST definitions, environment, codegen prototypes
├── src/
│   ├── main.c                # Compiler CLI driver & options
│   ├── compiler/
│   │   ├── reader.c          # S-expression file and string readers
│   │   ├── macro.c           # Macro expansion engine & backquote processor
│   │   ├── ast.c             # AST generator, lexical/function/tag environments
│   │   └── codegen.c         # C code generator & MinGW GCC invocation
│   └── runtime/
│       ├── runtime.c         # Core memory allocation, closures, symbols, globals
│       ├── math.c            # Arithmetic, comparisons, and math builtins
│       ├── list.c            # Cons cells, list operations, vector & array builtins
│       ├── string.c          # Strings and character operations
│       ├── io.c              # Streams, reader lexer, format function
│       ├── error.c           # Exception frames, unwind-protect stack manager
│       ├── ilos.c            # ILOS object system (CPL, dispatch, slots, methods)
│       └── builtins.c        # Standard library function registrations
├── tests/
│   ├── test_basics.lsp       # Arithmetic, predicates, lists, strings, vectors
│   ├── test_control.lsp      # Blocks, catch/throw, tagbody/go, unwind-protect, dynamic-let
│   ├── test_functions.lsp    # Rest args, flet, recursive labels, higher-order functions
│   ├── test_ilos.lsp         # Classes, instances, slot-value, methods, call-next-method
│   └── test_gc.lsp           # Garbage collection: churn, retention, cycles, vectors, ILOS
└── Makefile                  # MinGW / GNU Make build configuration
```

---

## Building

### Requirements
- **C Compiler**: GCC (MinGW-W64 on Windows or native GCC on POSIX systems)
- **Make**: GNU Make or `mingw32-make`

### Compilation
To build both the runtime library (`libislisp_rt.a`) and compiler executable (`cisl.exe`):
```bash
mingw32-make
# or
make
```

### Running Tests
To run all test suites (basics, control flow, functions, and ILOS object system):
```bash
mingw32-make test
# or
make test
```

---

## CLI Usage

```
cisl [options] <input.lsp>
cisl -e "<expression>"
```

### Options

| Flag | Description |
|------|-------------|
| `-o <file>` | Specify output executable binary or C file name |
| `-c` | Emit generated C source code only (does not invoke GCC) |
| `--run` | Compile to native binary and execute immediately |
| `-e "<expr>"` | Compile, evaluate, and print the result of an ISLisp expression |
| `-v, --version` | Display compiler version |
| `-h, --help` | Display usage information |

### Examples

#### 1. Compile and Execute a File
```bash
cisl --run tests/test_ilos.lsp
```

#### 2. Generate Standalone Binary
```bash
cisl -o my_program.exe tests/test_basics.lsp
./my_program.exe
```

#### 3. Inspect Generated C Code
```bash
cisl -c tests/test_functions.lsp -o output.c
```

#### 4. Evaluate One-Liner Expressions
```bash
cisl -e "(+ 123 456)"
# Output: 579

cisl -e "(mapcar (lambda (x) (* x x)) '(1 2 3 4 5))"
# Output: (1 4 9 16 25)
```

---

## ILOS Object System Example

```lisp
;; Define a class with slots and initargs
(defclass <point> ()
  ((x :initarg x)
   (y :initarg y)))

;; Define subclass
(defclass <colored-point> (<point>)
  ((color :initarg color)))

;; Generic function and methods
(defgeneric describe-pt (p))

(defmethod describe-pt ((p <point>))
  (format (standard-output) "Point at (~A, ~A)~%" (slot-value p 'x) (slot-value p 'y)))

(defmethod describe-pt :before ((p <colored-point>))
  (format (standard-output) "[Color: ~A] " (slot-value p 'color)))

(defglobal pt (create (class <colored-point>) 'x 10 'y 20 'color 'blue))
(describe-pt pt)
;; Output: [Color: blue] Point at (10, 20)
```

---

## Conformance & Standards Reference

CISL is developed according to:
- **ISO/IEC 13816:1997(E) / 13816:2007(E)** Information Technology — Programming Languages — ISLISP
- **Programming Language ISLISP Working Draft 23.0** (ISLisp HyperDraft specification)
