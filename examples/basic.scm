;; basic.scm
;;
;; Nothing fancy here!

(def a [1 2 3 4])

(def add2 (lambda (x) {
    (print "-> " x "\n")    
}))


(print (add2 a) "\n")

;; eof

