;; stress_test.scm
;;
;; Benchmarks a CPU-intensive loop

(def loop (lambda (n)
  (if (> n 0) (loop (- n 1)))))


(print "starting test...")
(def tic (clock))
(loop 1000000)
(def toc (clock))
(print "\nelapsed time: " (- toc tic) "\n")

;; eof