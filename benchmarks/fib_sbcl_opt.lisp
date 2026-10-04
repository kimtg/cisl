(defun fib (n)
  (declare (optimize (speed 3) (safety 0) (debug 0))
           (type fixnum n))
  (the fixnum
    (if (< n 2)
        n
        (+ (the fixnum (fib (- n 1)))
           (the fixnum (fib (- n 2)))))))

(let ((n 40))
  (let ((t0 (get-internal-real-time)))
    (let ((res (fib n)))
      (let ((t1 (get-internal-real-time)))
        (format t "RESULT=~A~%" res)
        (format t "TIME=~,6F~%" (/ (- t1 t0) (float internal-time-units-per-second)))))))

