#! /usr/local/bin/guile -s
!#

;; Use current folder, can also use enviroment variables here for absolute
;; You can also provide root path to configure, but "." is default anyway
(add-to-load-path ".")
(use-modules (buildlib))

(configure #:c-compiler "tcc" #:exe-name "got-test") ;;#:lib-source-dir "src/lib" #:lib-name "libgot" #:lib-type 'both)

(compile-c)

(install (memq #t (map (lambda (x) (equal? "install" x))
                       (command-line))))

(clean (memq #t (map (lambda (x) (equal? "clean" x))
                     (command-line))))
