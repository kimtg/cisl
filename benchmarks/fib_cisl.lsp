(defun fib (n)
  (if (< n 2)
      n
      (+ (fib (- n 1)) (fib (- n 2)))))

(defun run-bench ()
  (let ((t0 (get-internal-real-time)))
    (let ((res (fib 40)))
      (let ((t1 (get-internal-real-time)))
        (format (standard-output) "RESULT=~A~%" res)
        (format (standard-output) "TIME=~A~%" (- t1 t0))))))

(run-bench)

