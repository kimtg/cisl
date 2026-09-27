;;; CISL Garbage Collector Test Suite

(defun assert-true (name cond)
  (if cond
      (format-object (standard-output) (string-append "PASS: " (string-append name "\n")) nil)
      (format-object (standard-output) (string-append "FAIL: " (string-append name "\n")) nil)))

(defun assert-eq (name expected actual)
  (if (equal expected actual)
      (format-object (standard-output) (string-append "PASS: " (string-append name "\n")) nil)
      (progn
        (format-object (standard-output) (string-append "FAIL: " name) nil)
        (format-object (standard-output) " - expected: " nil)
        (format-object (standard-output) expected t)
        (format-object (standard-output) " got: " nil)
        (format-object (standard-output) actual t)
        (format-object (standard-output) "\n" nil))))

;;; 1. Basic GC Invocation
(defun test-gc-basic ()
  (assert-true "gc-call-returns-t" (eq (gc) t))
  (let ((stats (gc-stats)))
    (assert-true "gc-stats-is-list" (consp stats))
    (assert-true "gc-stats-has-collections" (integerp (cdr (assoc 'collections stats))))
    (assert-true "gc-stats-has-freed" (integerp (cdr (assoc 'freed-objects stats))))))

;;; 2. Allocation Churn and Collection
(defun make-garbage (n)
  (let ((i 0))
    (while (< i n)
      (cons i (cons (+ i 1) nil))
      (setq i (+ i 1)))))

(defun test-gc-churn ()
  (let ((before-stats (gc-stats)))
    (let ((freed-before (cdr (assoc 'freed-objects before-stats))))
      ;; Allocate 50000 temporary cons cells
      (make-garbage 25000)
      (gc)
      (let ((after-stats (gc-stats)))
        (let ((freed-after (cdr (assoc 'freed-objects after-stats))))
          (assert-true "gc-reclaims-garbage" (> freed-after freed-before)))))))

;;; 3. Retention of Live Root Objects
(defun make-live-list (n acc)
  (if (= n 0)
      acc
      (make-live-list (- n 1) (cons n acc))))

(defun sum-list (lst acc)
  (if (null lst)
      acc
      (sum-list (cdr lst) (+ acc (car lst)))))

(defun test-gc-retention ()
  (let ((live-data (make-live-list 100 nil)))
    ;; Generate garbage while live-data is in scope
    (make-garbage 10000)
    (gc)
    ;; Verify live-data is completely intact
    (assert-eq "live-list-length" 100 (length live-data))
    (assert-eq "live-list-sum" 5050 (sum-list live-data 0))
    (assert-eq "live-list-head" 1 (car live-data))))

;;; 4. Cyclic Structures Collection
(defun make-cyclic-garbage (n)
  (let ((i 0))
    (while (< i n)
      (let ((c (cons i nil)))
        (set-cdr c c))
      (setq i (+ i 1)))))

(defun test-gc-cycles ()
  (let ((before (cdr (assoc 'freed-objects (gc-stats)))))
    (make-cyclic-garbage 5000)
    (gc)
    (let ((after (cdr (assoc 'freed-objects (gc-stats)))))
      (assert-true "gc-handles-cycles" (> after before)))))

;;; 5. Higher-Order Functions and Lambda Survival
(defun test-gc-lambda ()
  (let ((data '(1 2 3 4 5)))
    (let ((mapped (mapcar (lambda (x) (+ x 100)) data)))
      (make-garbage 5000)
      (gc)
      (assert-eq "lambda-mapcar-after-gc" '(101 102 103 104 105) mapped))))

;;; 6. Vectors and Strings Survival
(defun test-gc-vectors-and-strings ()
  (let ((v (vector 10 20 30 40 50))
        (s "Hello ISLisp GC"))
    (make-garbage 5000)
    (gc)
    (assert-eq "vector-preserved" 30 (elt v 2))
    (assert-eq "string-preserved" "Hello ISLisp GC" s)))

;;; 7. ILOS Objects Survival
(defclass <gc-item> ()
  ((name :initarg name)
   (val  :initarg val)))

(defun test-gc-ilos ()
  (let ((item (create (class <gc-item>) 'name 'alpha 'val 999)))
    (make-garbage 5000)
    (gc)
    (assert-eq "ilos-slot-name" 'alpha (slot-value item 'name))
    (assert-eq "ilos-slot-val" 999 (slot-value item 'val))))

;;; Run All Tests
(test-gc-basic)
(test-gc-churn)
(test-gc-retention)
(test-gc-cycles)
(test-gc-lambda)
(test-gc-vectors-and-strings)
(test-gc-ilos)

(format-object (standard-output) "ALL GC TESTS COMPLETED!\n" nil)
