(defun assert-true (msg val)
  (if val
      (format (standard-output) "PASS: ~A~%" msg)
      (format (standard-output) "FAIL: ~A~%" msg)))

(defun assert-equal (msg a b)
  (if (equal a b)
      (format (standard-output) "PASS: ~A (~S == ~S)~%" msg a b)
      (format (standard-output) "FAIL: ~A (expected ~S, got ~S)~%" msg b a)))

;;; 1. Block and Return-From
(defun test-block-1 ()
  (block b1
    (return-from b1 42)
    99))

(assert-equal "block-simple" (test-block-1) 42)

(defun test-block-nested ()
  (block outer
    (block inner
      (return-from outer 100))
    200))

(assert-equal "block-nested" (test-block-nested) 100)

;;; 2. Catch and Throw
(defun test-catch-1 ()
  (catch 'my-tag
    (throw 'my-tag 777)
    888))

(assert-equal "catch-throw" (test-catch-1) 777)

;;; 3. Tagbody and Go
(defun test-tagbody-1 ()
  (let ((x 0)
        (sum 0))
    (tagbody
     start
      (setq sum (+ sum x))
      (setq x (+ x 1))
      (if (<= x 5)
          (go start))
      (go end)
     skip
      (setq sum 9999)
     end)
    sum))

(assert-equal "tagbody-loop" (test-tagbody-1) 15)

;;; 4. Dynamic variables and Dynamic-let
(defdynamic *dyn-var* 10)

(defun read-dyn ()
  (dynamic *dyn-var*))

(assert-equal "dyn-top" (read-dyn) 10)

(assert-equal "dynamic-let"
  (dynamic-let ((*dyn-var* 99))
    (read-dyn))
  99)

(assert-equal "dyn-restored" (read-dyn) 10)

;;; 5. Unwind-protect
(defglobal *unwind-flag* nil)

(defun test-unwind ()
  (catch 'exit
    (unwind-protect
        (throw 'exit 123)
      (setq *unwind-flag* t))))

(assert-equal "unwind-val" (test-unwind) 123)
(assert-true "unwind-flag-set" *unwind-flag*)

;;; 6. Cond and Case
(defun test-cond (x)
  (cond
    ((= x 1) 'one)
    ((= x 2) 'two)
    (t 'other)))

(assert-equal "cond-1" (test-cond 1) 'one)
(assert-equal "cond-2" (test-cond 2) 'two)
(assert-equal "cond-3" (test-cond 3) 'other)

(defun test-case (x)
  (case x
    ((1 2) 'small)
    ((3 4) 'medium)
    (t 'large)))

(assert-equal "case-1" (test-case 2) 'small)
(assert-equal "case-2" (test-case 3) 'medium)
(assert-equal "case-3" (test-case 10) 'large)

;;; 7. While and For loops
(defun test-while ()
  (let ((i 0)
        (res nil))
    (while (< i 4)
      (setq res (cons i res))
      (setq i (+ i 1)))
    res))

(assert-equal "while-loop" (test-while) '(3 2 1 0))

(format (standard-output) "ALL CONTROL TESTS COMPLETED!~%")
