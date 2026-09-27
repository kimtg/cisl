(defun assert-true (msg val)
  (if val
      (format (standard-output) "PASS: ~A~%" msg)
      (format (standard-output) "FAIL: ~A~%" msg)))

(defun assert-equal (msg a b)
  (if (equal a b)
      (format (standard-output) "PASS: ~A (~S == ~S)~%" msg a b)
      (format (standard-output) "FAIL: ~A (expected ~S, got ~S)~%" msg b a)))

;;; 1. Classes and Instances
(defclass <point> ()
  ((x :initarg x)
   (y :initarg y)
   (tag)))

(defclass <colored-point> (<point>)
  ((color :initarg color)))

(defglobal pt (create (class <point>) 'x 10 'y 20))
(defglobal cpt (create (class <colored-point>) 'x 1 'y 2 'color 'blue))

;; Predicates
(assert-true "instancep-point" (instancep pt (class <point>)))
(assert-true "instancep-subclass" (instancep cpt (class <point>)))
(assert-true "instancep-colored" (instancep cpt (class <colored-point>)))
(assert-true "subclassp" (subclassp (class <colored-point>) (class <point>)))

;; Slot access and mutation
(assert-equal "slot-x" (slot-value pt 'x) 10)
(assert-equal "slot-y" (slot-value pt 'y) 20)
(setf (slot-value pt 'x) 99)
(assert-equal "slot-setf" (slot-value pt 'x) 99)

(assert-true "slot-boundp-bound" (slot-boundp pt 'y))
(assert-true "slot-boundp-unbound" (not (slot-boundp pt 'tag)))

(assert-equal "cpt-slots" (list (slot-value cpt 'x) (slot-value cpt 'y) (slot-value cpt 'color)) '(1 2 blue))

;;; 2. Generic Functions & Primary Method Dispatch
(defgeneric describe-obj (obj))
(defmethod describe-obj ((obj <point>))
  'point)
(defmethod describe-obj ((obj <colored-point>))
  'colored-point)
(defmethod describe-obj ((obj <integer>))
  'an-integer)

(assert-equal "dispatch-point" (describe-obj pt) 'point)
(assert-equal "dispatch-colored" (describe-obj cpt) 'colored-point)
(assert-equal "dispatch-integer" (describe-obj 42) 'an-integer)

;;; 3. call-next-method
(defgeneric calc-weight (obj))
(defmethod calc-weight ((obj <point>))
  10)
(defmethod calc-weight ((obj <colored-point>))
  (+ 5 (call-next-method)))

(assert-equal "call-next-method" (calc-weight cpt) 15)

;;; 4. Method Combination: :before, :after, :around
(defglobal *audit-trail* '())

(defgeneric process-data (obj))

(defmethod process-data :before ((obj <point>))
  (setq *audit-trail* (cons 'before-point *audit-trail*)))

(defmethod process-data ((obj <point>))
  (setq *audit-trail* (cons 'primary-point *audit-trail*))
  42)

(defmethod process-data :after ((obj <point>))
  (setq *audit-trail* (cons 'after-point *audit-trail*)))

(defglobal res (process-data pt))
(assert-equal "before-after-result" res 42)
(assert-equal "before-after-order" *audit-trail* '(after-point primary-point before-point))

;; Around method
(defgeneric compute-val (obj))
(defmethod compute-val ((obj <point>))
  100)
(defmethod compute-val :around ((obj <point>))
  (* 2 (call-next-method)))

(assert-equal "around-method" (compute-val pt) 200)

(format (standard-output) "ALL ILOS TESTS COMPLETED!~%")
