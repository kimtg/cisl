(defun fib (n)
  (labels ((fib-rec (k)
             (if (< k 2)
                 k
                 (+ (fib-rec (- k 1)) (fib-rec (- k 2))))))
    (fib-rec n)))

(defun run-bench ()
  (let ((t0 (get-internal-real-time)))
    (let ((res (fib 40)))
      (let ((t1 (get-internal-real-time)))
        (format (standard-output) "RESULT=~A~%" res)
        (format (standard-output) "TIME=~A~%" (- t1 t0))))))

(run-bench)

