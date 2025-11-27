;; infinite_recursion.scm
;;
;; Example of infinite recursion (the program will never stop)

(def rec (lambda (x) { (print x "\n") (rec (+ x 1))})) 

(rec 1)

;; eof

