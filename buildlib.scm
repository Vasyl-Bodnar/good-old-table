;; This Source Code Form is subject to the terms of the Mozilla Public
;; License, v. 2.0. If a copy of the MPL was not distributed with this
;; file, You can obtain one at http://mozilla.org/MPL/2.0/.
(define-module (buildlib)
  #:autoload (ice-9 optargs) (define*)
  #:autoload (ice-9 ftw) (nftw)
  #:autoload (ice-9 threads) (par-map n-par-map)
  #:autoload (ice-9 binary-ports) (get-bytevector-all put-bytevector put-u8)
  #:autoload (ice-9 textual-ports) (get-line get-string-all)
  #:autoload (ice-9 popen) (open-pipe*)
  #:autoload (srfi srfi-1) (drop-right! last)
  #:export (configure compile-c install clean disable-default-failure))

(define *c-compiler* #f)
(define *source-directory* #f)
(define *build-directory* #f)
(define *obj-build-directory* #f)
(define *executable* #f)
;; Libraries are WIP
(define *library* #f)
(define *library-type* #f)
(define *extra-args* #f)

(define *root* #f)
(define *metadata* #f)
(define *compiler-info-hash* #f)

(define *hash-command* #f)
(define *sudo-command* #f)
(define *rmdir-command* #f)

(define fss file-name-separator-string)

(define *fatal-fail* #t)

(define (info . strs)
  (display (apply string-append strs))
  (newline)
  #t)

(define (warn . strs)
  (display (apply string-append (cons "WARNING: " strs)))
  (newline)
  #t)

(define (fail . strs)
  (display (apply string-append (cons "FAILURE: " strs)))
  (newline)
  (if (eq? *fatal-fail* 'ok)
      (set! *fatal-fail* 'fail)
      (if *fatal-fail* (set! *fatal-fail* #f)))
  #f)

(define (return-fail)
  (if *fatal-fail*
      *fatal-fail*
      (exit #f)))

(define (check-fail)
  (and (return-fail) (not (equal? return-fail 'fail))))

(define (system*-fail fail-fun . args)
  (let ((result (status:exit-val (apply system* args))))
    (unless (= result 0) (fail-fun (number->string result)))))

(define (disable-default-failure)
  (set! *fatal-fail* 'ok))

(define (hash-file filename)
  (let* ((port (open-pipe* OPEN_READ *hash-command* filename))
         (hash (get-line port)))
    (close-port port)
    hash))

;; This works, but is rather fragile
(define (hash-port port)
  (let* ((tmp-port (mkstemp "/tmp/build-XXXXXX"))
         (bv (get-bytevector-all port)))
    (put-bytevector tmp-port bv)
    (let ((hash (hash-file (port-filename tmp-port))))
      (delete-file (port-filename tmp-port))
      (close-port tmp-port)
      (string-take hash
                   (do ((i 0 (1+ i)))
                       ((or (>= i (string-length hash))
                            (char=? #\space (string-ref hash i)))
                        i))))))

(define (hash-compiler-info)
  (let* ((port (open-pipe* OPEN_READ *c-compiler* "--version"))
         (str (get-string-all port)))
    (close-port port)
    (if (eof-object? str)
        (fail "Could not get compiler version"))
    (let* ((port (open-input-string (string-append str *extra-args*)))
           (hash (hash-port port)))
      (close-input-port port)
      hash)))

;; TODO: libraries are unsupported at the moment
;; lib-type is 'none 'static or 'dynamic
(define* (configure #:key (c-compiler "cc") (root ".")
                    (exe-name "a") (lib-name "a") (lib-type 'none)
                    (source-dir "src") (build-dir "build") (obj-dir "obj")
                    (optimization "-O0") (debug "") (wall ""))
  (set! *root* (canonicalize-path root))
  (set! *c-compiler* c-compiler)
  (set! *source-directory* (string-append *root* fss source-dir))
  (set! *build-directory* (string-append *root* fss build-dir))
  (set! *obj-build-directory* (string-append *build-directory* fss obj-dir))
  (set! *executable* (string-append *build-directory* fss exe-name))
  (set! *extra-args*
        (string-append optimization
                       (if (equal? debug "") debug (string-append " " debug))
                       (if (equal? wall "") wall (string-append " " wall))))
  (set! *metadata* (string-append *build-directory* fss ".metadata"))
  (unless (equal? lib-type 'none)
    (set! *library* (string-append *build-directory* fss lib-name))
    (set! *library-type* lib-type))
  (unless (let ((st (stat *source-directory* #f)))
            (and st (eq? (stat:type st) 'directory)))
    (fail "Could not find a source directory named " *source-directory*))
  (unless (let ((st (stat *build-directory* #f)))
            (and st (eq? (stat:type st) 'directory)))
    (warn "Could not find a build directory named " *build-directory*)
    (info "Creating the build directory")
    (mkdir *build-directory*))
  (unless (let ((st (stat *obj-build-directory* #f)))
            (and st (eq? (stat:type st) 'directory)))
    (warn "Could not find an object directory named " *obj-build-directory*)
    (info "Creating the obj directory")
    (mkdir *obj-build-directory*))
  (unless (file-exists? *metadata*)
    (warn "Could not find metadata")
    (info "Creating new metadata")
    (let ((file (open *metadata* O_CREAT)))
      (close file)))
  ;; We also need to ensure a few system commands are present, and set them accordingly
  ;; TODO: Consider adding a proper local hash function implementation instead
  (let ((hash-command (or (search-path (parse-path (getenv "PATH")) "b2sum")
                          (search-path (parse-path (getenv "PATH")) "sha512sum")
                          (search-path (parse-path (getenv "PATH")) "sha256sum")
                          (search-path (parse-path (getenv "PATH")) "sha1sum")
                          (search-path (parse-path (getenv "PATH")) "md5sum")))
        (sudo-command (or (search-path (parse-path (getenv "PATH")) "sudo")
                          (search-path (parse-path (getenv "PATH")) "doas")))
        (rmdir-command (search-path (parse-path (getenv "PATH")) "rmdir"))
        (rm-command (search-path (parse-path (getenv "PATH")) "rm"))
        (cp-command (search-path (parse-path (getenv "PATH")) "cp")))
    (if hash-command
        (set! *hash-command* hash-command)
        (fail "Could not find a hash command in PATH"))
    (if sudo-command
        (set! *sudo-command* sudo-command)
        (warn "Could not find a sudo/doas command in PATH, installing might not be possible"))
    (if rmdir-command
        (set! *rmdir-command* rmdir-command)
        (warn "No rmdir in PATH? Using rm -r"))
    (unless rm-command
      (fail "No rm in PATH!?"))
    (unless cp-command
      (fail "No cp in PATH!?")))
  (let ((compiler-info-hash (hash-compiler-info)))
    (if compiler-info-hash
        (set! *compiler-info-hash* compiler-info-hash)
        (fail "Could not find a c compiler: " *c-compiler*)))
  (return-fail))

(define (handle-hash args)
  (case (car args)
    ((hashed) (cadr args))
    ((finished) (hash-file (cadr args)))))

(define (check-cache src obj hashes)
  (if (file-exists? obj)
      (let* ((file-hash (hash-file src))
             (found (member file-hash hashes)))
        (if found (car found) #f))
      #f))

;; TODO: While we currently just hash sources directly, we should run gcc -E at least
;; Otherwise it will not recompile despite changes to headers
(define* (hash-inputs objs #:key (num-threads #f))
  (info "Hashing sources")
  (let ((hashes (if num-threads
                    (n-par-map num-threads handle-hash objs)
                    (par-map handle-hash objs))))
    (call-with-output-file *metadata*
      (lambda (port)
        (display *compiler-info-hash* port)
        (newline port)
        (for-each (lambda (hash)
                    (display hash port)
                    (newline port))
                  hashes)))))

(define (compile-c-object-file src obj)
  (info "Compiling " (basename src))
  (let ((result (status:exit-val (system* *c-compiler* *extra-args* "-c" src "-o" obj))))
    (if (= result 0) (list 'finished src obj)
        (list #f "Could not compile a file: " (basename src) "\nReturned " (number->string result)))))

(define (link-exe objs)
  (info "Linking an executable")
  (apply system*-fail (cons (lambda (ret) (fail "Could not not link an executable\nReturned " ret))
                            (cons *c-compiler* (append (map caddr objs) (list "-o" *executable*))))))

(define (handle-compile result)
  (case (car result)
    ((hashed) result)
    ((unfinished) (apply compile-c-object-file (cdr result)))))

(define (check-objs objs)
  (for-each
   (lambda (obj)
     (unless (car obj)
       (apply fail (cdr obj))))
   objs))

(define* (compile-c #:key (num-threads #f))
  (unless (and *c-compiler*
               *extra-args*
               *source-directory*
               *build-directory*
               *obj-build-directory*
               *metadata*
               *compiler-info-hash*
               *hash-command*)
    (fail "Must run (configure) first"))
  (let* ((objs '())
         (all-hashes (reverse (call-with-input-file *metadata*
                                (lambda (port)
                                  (do ((hash (get-line port) (get-line port))
                                       (list '() (cons hash list)))
                                      ((eof-object? hash) list))))))
         (compiler-hash-good?
          (and (> (length all-hashes) 1) (equal? *compiler-info-hash* (car all-hashes))))
         (hashes (if compiler-hash-good? (cdr all-hashes) '())))
    (nftw
     *source-directory*
     (lambda (filename statinfo flag base level)
       (case flag
         ((regular)
          (unless (equal? (basename filename) (basename filename ".c"))
            (let* ((obj-filename (string-append *obj-build-directory* fss (basename filename ".c") ".o"))
                   (hash (if compiler-hash-good?
                             (check-cache filename obj-filename hashes)
                             #f)))
              (if hash
                  (begin
                    (info "Already compiled, skipping " (basename filename))
                    (set! objs (cons (list 'hashed hash obj-filename) objs)))
                  (set! objs (cons (list 'unfinished filename obj-filename) objs)))
              #t)))
         ((directory) (info "Entering a directory: " (basename filename)))
         ((invalid-stat) (fail "Could not stat a file: " (basename filename)))
         ((directory-not-readable) (fail "Directory is not readable: " (basename filename)))
         ((stale-symlink) (fail "Could not follow a symlink: " (basename filename))))))
    (if num-threads
        (set! objs (n-par-map num-threads handle-compile objs))
        (set! objs (par-map handle-compile objs)))
    (check-objs objs)
    (if (check-fail) (link-exe objs))
    (if (check-fail) (hash-inputs objs)))
  (return-fail))

(define* (install #:key (prefix "/usr/local"))
  (info "Copying to local bin")
  (if *sudo-command*
      (system*-fail (lambda (ret) (fail "Could not copy the executable\nReturned " ret))
                    *sudo-command* "cp" *executable* (string-append prefix fss "bin" fss (basename *executable*)))
      (system*-fail (lambda (ret) (fail "Could not copy the executable\nReturned " ret))
                    "cp" *executable* (string-append prefix fss "bin" fss (basename *executable*))))
  (return-fail))

;; This is a careful clean that only targets what it itself may have produced
;; If you truly need a nuclear option, it shall be provided
(define* (clean  #:optional (conditional #t) #:key (nuclear #f))
  (when conditional
    (info "Cleaning builds")
    (if (or nuclear (not *rmdir-command*))
        (system*-fail (lambda (ret) (fail "Could not delete the build directory\nReturned " ret))
                      "rm" "-r" *build-directory*)
        (begin
          (delete-file *executable*)
          (delete-file *metadata*)
          (nftw *obj-build-directory*
                (lambda (filename statinfo flag base level)
                  (if (and (eq? flag 'regular) (not (equal? (basename filename) (basename filename ".o"))))
                      (begin (delete-file filename) #t)
                      #t)))
          (system*-fail (lambda (ret) (fail "Could not delete the obj directory\nReturned " ret))
                        *rmdir-command* *obj-build-directory*)
          (system*-fail (lambda (ret) (fail "Could not delete the build directory\nReturned " ret))
                        *rmdir-command* *build-directory*))))
  (return-fail))
