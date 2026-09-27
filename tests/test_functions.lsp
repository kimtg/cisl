(defun assert-true (msg val)
  (if val
      (format (standard-output) "PASS: ~A~%" msg)
      (format (standard-output) "FAIL: ~A~%" msg)))

(defun assert-equal (msg a b)
  (if (equal a b)
      (format (standard-output) "PASS: ~A (~S == ~S)~%" msg a b)
      (format (standard-output) "FAIL: ~A (expected ~S, got ~S)~%" msg b a)))

;;; 1. Function with &rest parameter
(defun my-list (a b &rest r)
  (cons a (cons b r)))

(assert-equal "defun-rest" (my-list 1 2 3 4 5) '(1 2 3 4 5))
(assert-equal "defun-rest-empty" (my-list 'x 'y) '(x y))

;;; 2. Flet (lexical local functions)
(defun test-flet ()
  (flet ((square (n) (* n n))
         (double (n) (+ n n)))
    (+ (square 5) (double 10))))

(assert-equal "flet" (test-flet) 45)

;;; 3. Labels (mutually/recursive local functions)
(defun test-labels (n)
  (labels ((factorial (k acc)
             (if (<= k 1)
                 acc
                 (factorial (- k 1) (* k acc)))))
    (factorial n 1)))

(assert-equal "labels-fact" (test-labels 5) 120)

;;; 4. Higher-order functions with lambda
(defun test-map ()
  (mapcar (lambda (x) (+ x 10)) '(1 2 3)))

(assert-equal "lambda-mapcar" (test-map) '(11 12 13))

(format (standard-output) "ALL FUNCTIONS TESTS COMPLETED!~%")
