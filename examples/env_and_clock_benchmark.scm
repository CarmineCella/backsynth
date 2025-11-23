;; env_and_clock_benchmark.scm
;;
;; Uses clock() to time a numeric operation,
;; and getvar to show environment info.

(load "stdlib.scm")

(print "=== env_and_clock_benchmark.scm ===\n\n")

(print "HOME = " (getvar "HOME") "\n")
(print "SHELL = " (getvar "SHELL") "\n\n")

(print "benchmark: sum of first 1e6 integers\n")

(def start (clock))

(def i   (array 0))
(def sum (array 0))

(while (< i (array 1000000))
  (begin
    (= sum (+ sum i))
    (= i (+ i (array 1)))))

(def stop (clock))

(print "result sum = " sum "\n")
(print "clock ticks elapsed = " (- stop start) "\n")
(print "Note: clock is CPU time, not wall clock.\n\n")
