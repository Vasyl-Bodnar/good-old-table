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

(define (test) (status:exit-val (system "./build/dgot-test > tmp.out && diff tmp.out test/base.out && rm tmp.out")))

(if testing?
    (begin
      (configure #:exe-name "got-test")
      (compile-c compile?)

      (configure #:exe-name "dgot-test"
                 #:derive '(DYNAMIC_TABLE))
      (compile-c compile?)

      (let ((t (test)))
        (if (not (= t 0))
            (fail "Got a test error: " (number->string t))
            (info "Tested"))))
    (begin
      (configure #:lib-name "libgot" #:lib-source-dir "src/lib" #:lib-type 'both
                 #:include-name "got"
                 #:optimization "-O3")
      (compile-c compile?)
      (install install?)

      (configure #:lib-name "libdgot" #:lib-source-dir "src/lib" #:lib-type 'both
                 #:include-name "got"
                 #:optimization "-O3"
                 #:derive '(DYNAMIC_TABLE))
      (compile-c compile?)
      (install install?)))

(clean clean?)
