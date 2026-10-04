;;; Project Euler Problems 1-10 in ISLisp
;;; Automated verification and answer output

(defun print-euler-result (prob-num name result expected)
  (format (standard-output) "Problem ~A (~A): Answer = ~A " prob-num name result)
  (if (= result expected)
      (format (standard-output) "[PASS]~%")
      (format (standard-output) "[FAIL: Expected ~A]~%" expected)))

;;; -------------------------------------------------------------
;;; Problem 1: Multiples of 3 or 5
;;; Find the sum of all the multiples of 3 or 5 below 1000.
;;; -------------------------------------------------------------
(defun euler-1 ()
  (let ((sum 0))
    (for ((i 1 (+ i 1)))
         ((>= i 1000) sum)
      (if (or (= (mod i 3) 0) (= (mod i 5) 0))
          (setq sum (+ sum i))))))

;;; -------------------------------------------------------------
;;; Problem 2: Even Fibonacci Numbers
;;; By considering the terms in the Fibonacci sequence whose values
;;; do not exceed 4,000,000, find the sum of the even-valued terms.
;;; -------------------------------------------------------------
(defun euler-2 ()
  (let ((a 1)
        (b 2)
        (sum 0))
    (while (<= b 4000000)
      (if (= (mod b 2) 0)
          (setq sum (+ sum b)))
      (let ((next (+ a b)))
        (setq a b)
        (setq b next)))
    sum))

;;; -------------------------------------------------------------
;;; Problem 3: Largest Prime Factor
;;; What is the largest prime factor of the number 600851475143?
;;; -------------------------------------------------------------
(defun euler-3 ()
  (let ((n 600851475143)
        (d 2))
    (while (<= (* d d) n)
      (if (= (mod n d) 0)
          (setq n (div n d))
          (setq d (+ d 1))))
    n))

;;; -------------------------------------------------------------
;;; Problem 4: Largest Palindrome Product
;;; Find the largest palindrome made from the product of two 3-digit numbers.
;;; -------------------------------------------------------------
(defun reverse-int (n)
  (let ((rev 0))
    (while (> n 0)
      (setq rev (+ (* rev 10) (mod n 10)))
      (setq n (div n 10)))
    rev))

(defun palindrome-p (n)
  (= n (reverse-int n)))

(defun euler-4 ()
  (let ((max-pal 0))
    (for ((i 999 (- i 1)))
         ((< i 100) max-pal)
      (for ((j i (- j 1)))
           ((or (< j 100) (< (* i j) max-pal)))
        (let ((prod (* i j)))
          (if (and (> prod max-pal) (palindrome-p prod))
              (setq max-pal prod)))))))

;;; -------------------------------------------------------------
;;; Problem 5: Smallest Multiple
;;; What is the smallest positive number that is evenly divisible
;;; by all of the numbers from 1 to 20?
;;; -------------------------------------------------------------
(defun euler-5 ()
  (let ((ans 1))
    (for ((i 2 (+ i 1)))
         ((> i 20) ans)
      (setq ans (lcm ans i)))))

;;; -------------------------------------------------------------
;;; Problem 6: Sum Square Difference
;;; Find the difference between the sum of the squares of the first
;;; one hundred natural numbers and the square of the sum.
;;; -------------------------------------------------------------
(defun euler-6 ()
  (let ((sum 0)
        (sum-sq 0))
    (for ((i 1 (+ i 1)))
         ((> i 100))
      (setq sum (+ sum i))
      (setq sum-sq (+ sum-sq (* i i))))
    (- (* sum sum) sum-sq)))

;;; -------------------------------------------------------------
;;; Problem 7: 10001st Prime
;;; What is the 10 001st prime number?
;;; -------------------------------------------------------------
(defun prime-p (n)
  (if (< n 2)
      nil
      (if (= n 2)
          t
          (if (= (mod n 2) 0)
              nil
              (let ((d 3)
                    (is-p t))
                (while (and is-p (<= (* d d) n))
                  (if (= (mod n d) 0)
                      (setq is-p nil)
                      (setq d (+ d 2))))
                is-p)))))

(defun euler-7 ()
  (let ((count 1)
        (candidate 1))
    (while (< count 10001)
      (setq candidate (+ candidate 2))
      (if (prime-p candidate)
          (setq count (+ count 1))))
    candidate))

;;; -------------------------------------------------------------
;;; Problem 8: Largest Product in a Series
;;; Find the thirteen adjacent digits in the 1000-digit number
;;; that have the greatest product.
;;; -------------------------------------------------------------
(defun char-to-digit (c)
  (- (convert c (class <integer>))
     (convert #\0 (class <integer>))))

(defun euler-8 ()
  (let* ((digits "7316717653133062491922511967442657474235534919493496983520312774506326239578318016984801869478851843858615607891129494954595017379583319528532088055111254069874715852386305071569329096329522744304355766896648950445244523161731856403098711121722383113622298934233803081353362766142828064444866452387493035890729629049156044077239071381051585930796086670172427121883998797908792274921901699720888093776657273330010533678812202354218097512545405947522435258490771167055601360483958644670632441572215539753697817977846174064955149290862569321978468622482839722413756570560574902614079729686524145351004748216637048440319989000889524345065854122758866688116427171479924442928230863465674813919123162824586178664583591245665294765456828489128831426076900422421902267105562632111110937054421750694165896040807198403850962455444362981230987879927244284909188845801561660979191338754992005240636899125607176060588611646710940507754100225698315520005593572972571636269561882670428252483600823257530420752963450")
         (len (length digits))
         (max-prod 0))
    (for ((i 0 (+ i 1)))
         ((> i (- len 13)) max-prod)
      (let ((prod 1))
        (for ((j 0 (+ j 1)))
             ((>= j 13))
          (setq prod (* prod (char-to-digit (elt digits (+ i j))))))
        (if (> prod max-prod)
            (setq max-prod prod))))))

;;; -------------------------------------------------------------
;;; Problem 9: Special Pythagorean Triplet
;;; A Pythagorean triplet is a set of three natural numbers, a < b < c,
;;; for which a^2 + b^2 = c^2. There exists exactly one Pythagorean
;;; triplet for which a + b + c = 1000. Find the product a*b*c.
;;; -------------------------------------------------------------
(defun euler-9 ()
  (let ((ans 0))
    (for ((a 1 (+ a 1)))
         ((or (> a 332) (> ans 0)) ans)
      (for ((b (+ a 1) (+ b 1)))
           ((or (> b (div (- 1000 a) 2)) (> ans 0)))
        (let ((c (- (- 1000 a) b)))
          (if (= (+ (* a a) (* b b)) (* c c))
              (setq ans (* a (* b c)))))))))

;;; -------------------------------------------------------------
;;; Problem 10: Summation of Primes
;;; Find the sum of all the primes below two million (2,000,000).
;;; -------------------------------------------------------------
(defun euler-10 ()
  (let* ((limit 2000000)
         (sieve (create-vector limit 1))
         (sum 0))
    (setf (elt sieve 0) 0)
    (setf (elt sieve 1) 0)
    (for ((p 2 (+ p 1)))
         ((> (* p p) limit))
      (if (= (elt sieve p) 1)
          (for ((i (* p p) (+ i p)))
               ((>= i limit))
            (setf (elt sieve i) 0))))
    (for ((p 2 (+ p 1)))
         ((>= p limit) sum)
      (if (= (elt sieve p) 1)
          (setq sum (+ sum p))))))

;;; -------------------------------------------------------------
;;; Execution and Verification
;;; -------------------------------------------------------------
(format (standard-output) "========================================~%")
(format (standard-output) "Project Euler Problems 1-10 Test Suite~%")
(format (standard-output) "========================================~%")

(print-euler-result 1  "Multiples of 3 or 5"         (euler-1)  233168)
(print-euler-result 2  "Even Fibonacci numbers"      (euler-2)  4613732)
(print-euler-result 3  "Largest prime factor"        (euler-3)  6857)
(print-euler-result 4  "Largest palindrome product"  (euler-4)  906609)
(print-euler-result 5  "Smallest multiple"           (euler-5)  232792560)
(print-euler-result 6  "Sum square difference"       (euler-6)  25164150)
(print-euler-result 7  "10001st prime"               (euler-7)  104743)
(print-euler-result 8  "Largest product in a series" (euler-8)  23514624000)
(print-euler-result 9  "Special Pythagorean triplet" (euler-9)  31875000)
(print-euler-result 10 "Summation of primes"         (euler-10) 142913828922)

(format (standard-output) "========================================~%")
(format (standard-output) "ALL PROJECT EULER TESTS COMPLETED!~%")
(format (standard-output) "========================================~%")
