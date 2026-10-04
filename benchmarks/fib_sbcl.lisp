(defun fib (n)
  (if (< n 2)
      n
      (+ (fib (- n 1)) (fib (- n 2)))))

(let ((n 40))
  (let ((t0 (get-internal-real-time)))
    (let ((res (fib n)))
      (let ((t1 (get-internal-real-time)))
        (format t "RESULT=~A~%" res)
        (format t "TIME=~,6F~%" (/ (- t1 t0) (float internal-time-units-per-second)))))))

