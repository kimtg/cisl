;;; Test Basic Operations: Arithmetic, Predicates, Lists, Strings, Vectors
(defun assert-true (msg val)
  (if val
      (format (standard-output) "PASS: ~A~%" msg)
      (format (standard-output) "FAIL: ~A~%" msg)))

(defun assert-equal (msg a b)
  (if (equal a b)
      (format (standard-output) "PASS: ~A (~S == ~S)~%" msg a b)
      (format (standard-output) "FAIL: ~A (expected ~S, got ~S)~%" msg b a)))

;; Arithmetic tests
(assert-equal "add" (+ 10 20 30) 60)
(assert-equal "sub" (- 100 30 10) 60)
(assert-equal "neg" (- 42) -42)
(assert-equal "mul" (* 2 3 4 5) 120)
(assert-equal "div" (div 14 4) 3)
(assert-equal "mod" (mod 14 4) 2)
(assert-equal "gcd" (gcd 54 24) 6)
(assert-equal "lcm" (lcm 4 6) 12)
(assert-equal "isqrt" (isqrt 100) 10)
(assert-equal "expt" (expt 2 10) 1024)
(assert-equal "abs" (abs -50) 50)
(assert-equal "min" (min 10 3 25) 3)
(assert-equal "max" (max 10 3 25) 25)

;; Comparisons
(assert-true "num-eq" (= 10 10))
(assert-true "num-lt" (< 5 10))
(assert-true "num-lteq" (<= 10 10))
(assert-true "num-gt" (> 15 10))
(assert-true "num-gteq" (>= 10 10))

;; Predicates
(assert-true "integerp" (integerp 123))
(assert-true "numberp" (numberp 123))
(assert-true "symbolp" (symbolp 'hello))
(assert-true "characterp" (characterp #\a))
(assert-true "stringp" (stringp "world"))
(assert-true "consp" (consp (cons 1 2)))
(assert-true "null" (null nil))
(assert-true "not-null" (not nil))

;; List Operations
(assert-equal "car/cdr" (car (cdr '(a b c))) 'b)
(assert-equal "length" (length '(1 2 3 4 5)) 5)
(assert-equal "reverse" (reverse '(1 2 3)) '(3 2 1))
(assert-equal "append" (append '(1 2) '(3 4) '(5)) '(1 2 3 4 5))
(assert-equal "member" (member 'b '(a b c)) '(b c))
(assert-equal "assoc" (assoc 'b '((a . 1) (b . 2))) '(b . 2))
(assert-equal "mapcar" (mapcar (lambda (x) (* x x)) '(1 2 3 4)) '(1 4 9 16))

;; String & Vector Operations
(assert-true "string=" (string= "foo" "foo"))
(assert-true "string<" (string< "abc" "xyz"))
(assert-equal "string-append" (string-append "Hello, " "ISLisp" "!") "Hello, ISLisp!")
(assert-equal "elt-string" (elt "hello" 1) #\e)
(assert-equal "subseq-string" (subseq "hello world" 0 5) "hello")
(assert-equal "vector" (elt (vector 10 20 30) 1) 20)

;; Logic
(assert-equal "and-true" (and 1 2 3) 3)
(assert-equal "and-false" (and 1 nil 3) nil)
(assert-equal "or-first" (or nil 42 nil) 42)

(format (standard-output) "ALL BASICS TESTS COMPLETED!~%")
