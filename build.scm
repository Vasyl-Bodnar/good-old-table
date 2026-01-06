#! /usr/local/bin/guile -s
!#
;; This Source Code Form is subject to the terms of the Mozilla Public
;; License, v. 2.0. If a copy of the MPL was not distributed with this
;; file, You can obtain one at http://mozilla.org/MPL/2.0/.

(add-to-load-path ".")
(use-modules (buildlib))

(define clean? (memq #t (map (lambda (x) (equal? "clean" x))
                             (command-line))))
(define install? (memq #t (map (lambda (x) (equal? "install" x))
                               (command-line))))
(define testing? (memq #t (map (lambda (x) (equal? "testing" x))
                               (command-line))))
(define compile? (not clean?))

(if testing?
    (begin
      (configure #:exe-name "got-test")
      (compile-c compile?)

      (configure #:exe-name "dgot-test"
                 #:optimization "-O3"
                 #:derive '(DYNAMIC_TABLE))
      (compile-c compile?))
    (begin
      (configure #:lib-name "libgot" #:lib-source-dir "src/lib" #:lib-type 'both
                 #:optimization "-O3")
      (compile-c compile?)

      (configure #:lib-name "libdgot" #:lib-source-dir "src/lib" #:lib-type 'both
                 #:optimization "-O3"
                 #:derive '(DYNAMIC_TABLE))
      (compile-c compile?)))

(install install?)

(clean clean?)
