(defun assert-true (msg val)
  (if val
      (format (standard-output) "PASS: ~A~%" msg)
      (format (standard-output) "FAIL: ~A~%" msg)))

(assert-true "num-eq" (= 10 10))
(assert-true "num-lt" (< 5 10))
(assert-true "num-lteq" (<= 10 10))
(assert-true "num-gt" (> 15 10))
(assert-true "num-gteq" (>= 10 10))

(assert-true "integerp" (integerp 123))
(assert-true "numberp" (numberp 123))
(assert-true "symbolp" (symbolp 'hello))
(assert-true "characterp" (characterp #\a))
(assert-true "stringp" (stringp "world"))
(assert-true "consp" (consp (cons 1 2)))
(assert-true "null" (null nil))
(assert-true "not-null" (not nil))
