;; --------------------------
;; Scientific library support
;; --------------------------
;;

; Aliases
(def rows (lambda (m) (nrows m)))
(def cols (lambda (m) (ncols m)))
(def tr (lambda (m) (transpose m)))

; Column-wise and row-wise means using matmean / matstd
(def colmean (lambda (m) (matmean m (array 0))))
(def rowmean (lambda (m) (matmean m (array 1))))

(def colstd  (lambda (m) (matstd  m (array 0))))
(def rowstd  (lambda (m) (matstd  m (array 1))))

; A helper to standardize features (columns) then compute covariance
(def covz
  (lambda (m)
    (cov (zscore m))))


;; eof

