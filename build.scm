#! /usr/local/bin/guile -s
!#
;; This Source Code Form is subject to the terms of the Mozilla Public
;; License, v. 2.0. If a copy of the MPL was not distributed with this
;; file, You can obtain one at http://mozilla.org/MPL/2.0/.

;; Use current folder, can also use enviroment variables here for absolute
;; You can also provide root path to configure, but "." is default anyway
(add-to-load-path ".")
(use-modules (buildlib))

(configure #:exe-name "got-test"
           #:lib-source-dir "src/lib" #:lib-name "libgot" #:lib-type 'both)

(compile-c)

(install (memq #t (map (lambda (x) (equal? "install" x))
                       (command-line))))

(clean (memq #t (map (lambda (x) (equal? "clean" x))
                     (command-line))))
