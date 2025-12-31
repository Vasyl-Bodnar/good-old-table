#! /usr/local/bin/guile -s
!#

;; Use current folder, can also use enviroment variables here for absolute
;; You can also provide root path to configure, but "." is default anyway
(add-to-load-path ".")
(use-modules (buildlib))

(disable-default-failure)

(configure #:exe-name "zzz-example")

(compile-c)

;;(install) We do not want to install an example

(clean (memq #t (map (lambda (x) (equal? "clean" x))
                     (command-line))))
