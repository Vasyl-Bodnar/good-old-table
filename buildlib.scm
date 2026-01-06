;; This Source Code Form is subject to the terms of the Mozilla Public
;; License, v. 2.0. If a copy of the MPL was not distributed with this
;; file, You can obtain one at http://mozilla.org/MPL/2.0/.
(define-module (buildlib)
  #:autoload (ice-9 optargs) (define*)
  #:autoload (ice-9 ftw) (nftw scandir)
  #:autoload (ice-9 threads) (par-map n-par-map)
  #:autoload (ice-9 binary-ports) (get-bytevector-all put-bytevector put-u8)
  #:autoload (ice-9 textual-ports) (get-line get-string-all)
  #:autoload (ice-9 popen) (open-pipe*)
  #:autoload (srfi srfi-1) (delete-duplicates lset<=)
  #:export (cache configure compile-c install clean disable-default-failure))

(define *c-compiler* #f)
(define *c-archiver* #f)
(define *source-directory* #f)
(define *lib-source-directory* #f)
(define *build-directory* #f)
(define *obj-build-directory* #f)
(define *executable* #f)
(define *library* #f)
(define *library-type* #f)
(define *extra-args* #f)

(define *cache* #t)
(define *keep-configs* 3)

(define *verbosity* 3)
(define *root* #f)
(define *metadata* #f)
(define *compiler-info-hash* #f)

(define *hash-command* #f)
(define *sudo-command* #f)
(define *rmdir-command* #f)

(define fss file-name-separator-string)

(define *fatal-fail* #t)

(define (info . strs)
  (when (= *verbosity* 3)
    (display (apply string-append strs))
    (newline))
  #t)

(define (warn . strs)
  (when (>= *verbosity* 2)
    (display (apply string-append (cons "WARNING: " strs)))
    (newline))
  #t)

(define (fail . strs)
  (when (>= *verbosity* 0)
    (display (apply string-append (cons "FAILURE: " strs)))
    (newline))
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

;; FIX: This works, but is rather fragile
(define (hash-port port)
  (let* ((tmp-port (mkstemp "/tmp/build-XXXXXX"))
         (bv (get-bytevector-all port)))
    (put-bytevector tmp-port bv)
    (force-output tmp-port)
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
    (let* ((port (open-input-string (string-append str *c-archiver* (apply string-append *extra-args*))))
           (hash (hash-port port)))
      (close-input-port port)
      hash)))

(define (verbosity level)
  (set! *verbosity* (if (> level 3) 3 level)))

(define* (cache #:optional (conditional #t) #:key (enable #t) (keep-configs 3))
  (when conditional
    (set! *cache* enable)
    (set! *keep-configs* keep-configs))
  (return-fail))

;; lib-type is 'none, 'static, 'dynamic, or 'both
(define* (configure #:optional (conditional #t) #:key (c-compiler "cc") (c-archiver "ar")
                    (root ".") (exe-name #f) (lib-name exe-name) (lib-type 'none) (sudo #t)
                    (source-dir "src") (lib-source-dir source-dir) (build-dir "build") (obj-dir "obj")
                    (optimization "-O0") (debug "-g") (wall "-Wall") (derive '()))
  (if (not (or exe-name (and lib-name (memq lib-type '(static dynamic both)))))
      (fail "You need to provide a name for at least one of executable or a library with the type (one of 'static, 'dynamic, 'both)")
      (when conditional
        (set! *root* (canonicalize-path root))
        (set! *c-compiler* c-compiler)
        (set! *c-archiver* c-archiver)
        (set! *source-directory* (string-append *root* fss source-dir))
        (set! *lib-source-directory* (string-append *root* fss lib-source-dir))
        (set! *build-directory* (string-append *root* fss build-dir))
        (set! *obj-build-directory* (string-append *build-directory* fss obj-dir))
        (set! *extra-args* (append (list optimization debug wall)
                                   (map (lambda (der)
                                          (string-append "-D" (if (symbol? der)
                                                                  (symbol->string der) der)))
                                        derive)))
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
          (when *cache*
            (if hash-command
                (set! *hash-command* hash-command)
                (fail "Could not find a hash command in PATH")))
          (when sudo
            (if sudo-command
                (set! *sudo-command* sudo-command)
                (warn "Could not find a sudo/doas command in PATH, installing might not be possible")))
          (if rmdir-command
              (set! *rmdir-command* rmdir-command)
              (warn "No rmdir in PATH? Using rm -r"))
          (unless rm-command
            (fail "No rm in PATH!?"))
          (unless cp-command
            (fail "No cp in PATH!?")))
        (when exe-name
          (set! *executable* (string-append *build-directory* fss exe-name)))
        (unless (equal? lib-type 'none)
          (when (and (memq lib-type '(static both)) (not (search-path (parse-path (getenv "PATH")) c-archiver)))
            (warn "Could not find archiver named \"" c-archiver "\" on PATH, cannot do static libraries")
            (set! *c-archiver* #f))
          (set! *library* (string-append *build-directory* fss lib-name))
          (set! *library-type* lib-type))
        (unless (let ((st (stat *source-directory* #f)))
                  (and st (eq? (stat:type st) 'directory)))
          (fail "Could not find a source directory named " *source-directory*))
        (unless (let ((st (stat *lib-source-directory* #f)))
                  (and st (eq? (stat:type st) 'directory)))
          (fail "Could not find a library source directory named " *lib-source-directory*))
        (unless (let ((st (stat *build-directory* #f)))
                  (and st (eq? (stat:type st) 'directory)))
          (info "Could not find a build directory named " *build-directory*)
          (info "Creating the build directory")
          (mkdir *build-directory*))
        (when *cache*
          (set! *metadata* (string-append *build-directory* fss ".metadata"))
          (unless (let ((st (stat *metadata* #f)))
                    (and st (eq? (stat:type st) 'directory)))
            (info "Could not find metadata named" *metadata*)
            (info "Creating new metadata")
            (mkdir *metadata*))
          (let ((compiler-info-hash (hash-compiler-info)))
            (if compiler-info-hash
                (begin
                  (set! *compiler-info-hash* compiler-info-hash)
                  (set! *obj-build-directory* (in-vicinity *metadata* *compiler-info-hash*)))
                (fail "Could not check a c compiler: " *c-compiler*))))
        (unless (let ((st (stat *obj-build-directory* #f)))
                  (and st (eq? (stat:type st) 'directory)))
          (info "Could not find an object directory named " *obj-build-directory*)
          (info "Creating the obj directory")
          (mkdir *obj-build-directory*))))
  (return-fail))

(define (hash-c-file source-file)
  (let* ((port (apply open-pipe* (append (list OPEN_READ *c-compiler* "-E") (cdddr *extra-args*) (list source-file))))
         (str (get-string-all port)))
    (close-port port)
    (if (eof-object? str)
        (fail "Could not macro expand file " (basename source-file)))
    (let* ((port (open-input-string str))
           (hash (hash-port port)))
      (close-input-port port)
      hash)))

;; args is one of
;; ('hashed hash obj-filename)
;; ('finished filename obj-filename)
(define (handle-hash args)
  (case (car args)
    ((hashed) (cadr args))
    ((finished) (hash-c-file (cadr args)))
    ((unfinished) #f)))

(define (check-cache src obj hashes)
  (if (file-exists? obj)
      (let* ((file-hash (hash-c-file src))
             (found (member file-hash hashes)))
        (if found (car found) #f))
      #f))

;; Deletes a flat directory (no subdirectories)
(define (delete-dir dir)
  (nftw
   dir
   (lambda (filename statinfo flag base level)
     (when (eq? flag 'regular)
       (delete-file filename))
     #t))
  (system*-fail (lambda (ret) (fail "Could not delete a directory " dir "\nReturned " ret))
                *rmdir-command* dir))

(define (last-dir configs)
  (let ((s (sort (map (lambda (f)
                        (call-with-input-file (in-vicinity (in-vicinity *metadata* f) "metafile")
                          (lambda (port)
                            (cons (string->number (get-line port)) (in-vicinity *metadata* f)))))
                      configs) (lambda (x y) (> (car x) (car y))))))
    (cdr (car s))))

;; TODO: While we currently just hash sources directly, we should run gcc -E at least
;; Otherwise it will not recompile despite changes to headers
;;
;; objs is a list of one of
;; ('hashed hash obj-filename)
;; ('finished filename obj-filename)
(define* (hash-inputs objs #:key (num-threads #f))
  (when *cache*
    (info "Hashing sources")
    (let ((configs (filter (lambda (f) (and (not (equal? f *compiler-info-hash*)) (not (equal? f ".")) (not (equal? f "..")))) (scandir *metadata*)))
          (hashes (if num-threads
                      (n-par-map num-threads handle-hash objs)
                      (par-map handle-hash objs))))
      (call-with-output-file (in-vicinity *obj-build-directory* "metafile")
        (lambda (port)
          (display (get-internal-real-time) port)
          (newline port)
          (for-each (lambda (hash)
                      (when hash
                        (display hash port)
                        (newline port)))
                    hashes)))
      (if (>= (length configs) *keep-configs*)
          (delete-dir (last-dir configs))))))

(define (compile-c-object-file src obj)
  (info "Compiling " (basename src))
  (let ((result (status:exit-val (apply system* (append (list *c-compiler*) *extra-args* (list "-c" src "-o" obj))))))
    (if (= result 0) (list 'finished src obj)
        (list #f "Could not compile a file: " (basename src) "\nReturned " (number->string result)))))

(define (link-exe objs)
  (info "Linking an executable")
  (apply system*-fail (cons (lambda (ret) (fail "Could not not link an executable\nReturned " ret))
                            (cons *c-compiler* (append objs (list "-o" *executable*))))))

(define (link-lib objs)
  (when (and (memq *library-type* '(static both)) *c-archiver*)
    (info "Linking a static library")
    (apply system*-fail (cons (lambda (ret) (fail "Could not not link a static library\nReturned " ret))
                              (append (list *c-archiver* "rcs" (string-append *library* ".a")) objs))))
  (when (memq *library-type* '(dynamic both))
    (info "Linking a dynamic library")
    (apply system*-fail (cons (lambda (ret) (fail "Could not not link a dynamic library\nReturned " ret))
                              (append (list *c-compiler* "-shared" "-fPIC" "-o" (string-append *library* ".so")) objs)))))

;; result is one of
;; ('hashed hash obj-filename)
;; ('unfinished filename obj-filename)
(define (handle-compile result)
  (case (car result)
    ((hashed) result)
    ((unfinished) (apply compile-c-object-file (cdr result)))))

;; objs is a list of one of
;; ('hashed hash obj-filename)
;; ('finished filename obj-filename)
;; (#f reason)
(define (check-objs objs)
  (for-each
   (lambda (obj)
     (unless (car obj)
       (apply fail (cdr obj))))
   objs))

;; compiled-objs should be alist of obj and hash
(define* (collect-objs dir hashes #:optional (compiled-objs '()))
  (let ((objs '()))
    (nftw
     dir
     (lambda (filename statinfo flag base level)
       (case flag
         ((regular)
          (unless (equal? (basename filename) (basename filename ".c"))
            (let* ((obj-filename (string-append *obj-build-directory* fss (basename filename ".c") ".o"))
                   (hash (and *cache*
                              (or (assoc-ref compiled-objs obj-filename)
                                  (if hashes
                                      (check-cache filename obj-filename hashes)
                                      #f)))))
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
    objs))

(define* (compile-c #:optional (conditional #t) #:key (num-threads #f))
  (when conditional
    (unless (and *c-compiler* *extra-args*
                 *source-directory* *build-directory*
                 *obj-build-directory*)
      (fail "Must run (configure) first"))
    (let* ((metadata (in-vicinity *obj-build-directory* "metafile"))
           (hashes (if (file-exists? metadata)
                       (reverse (call-with-input-file metadata
                                  (lambda (port)
                                    (do ((hash (get-line port) (get-line port))
                                         (list '() (cons hash list)))
                                        ((eof-object? hash) list)))))
                       '()))
           (exe-objs '())
           (lib-objs '()))
      (when *executable*
        (set! exe-objs (collect-objs *source-directory* hashes '()))
        (if num-threads
            (set! exe-objs (n-par-map num-threads handle-compile exe-objs))
            (set! exe-objs (par-map handle-compile exe-objs)))
        (check-objs exe-objs)
        (if (check-fail) (link-exe (map caddr exe-objs)))
        (if (check-fail) (hash-inputs exe-objs)))
      (when (and *library* (memq *library-type* '(static dynamic both)))
        (set! lib-objs (collect-objs *lib-source-directory* hashes exe-objs))
        (if (string-prefix? *source-directory* *lib-source-directory*)
            (begin
              (if num-threads
                  (set! lib-objs (n-par-map num-threads handle-compile lib-objs))
                  (set! lib-objs (par-map handle-compile lib-objs)))
              (check-objs lib-objs))
            (info "Already compiled"))
        (if (check-fail) (link-lib (map caddr lib-objs))))
      (if (and (check-fail) (not (lset<= equal? lib-objs exe-objs))) (hash-inputs (delete-duplicates (append exe-objs lib-objs))))))
  (return-fail))

(define* (install #:optional (conditional #t) #:key (prefix "/usr/local"))
  (when conditional
    (info "Copying to local bin")
    (if *sudo-command*
        (begin
          (when *executable*
            (system*-fail (lambda (ret) (fail "Could not copy the executable\nReturned " ret))
                          *sudo-command* "cp" *executable* (string-append prefix fss "bin" fss (basename *executable*))))
          (when *library*
            (when (memq *library-type* '(static both))
              (system*-fail (lambda (ret) (fail "Could not copy the static library\nReturned " ret))
                            *sudo-command* "cp" (string-append *library* ".a") (string-append prefix fss "lib" fss (basename (string-append *library* ".a")))))
            (when (memq *library-type* '(dynamic both))
              (system*-fail (lambda (ret) (fail "Could not copy the dynamic library\nReturned " ret))
                            *sudo-command* "cp" (string-append *library* ".so") (string-append prefix fss "lib" fss (basename (string-append *library* ".so")))))))
        (begin
          (when *executable*
            (system*-fail (lambda (ret) (fail "Could not copy the executable\nReturned " ret))
                          "cp" *executable* (string-append prefix fss "bin" fss (basename *executable*))))
          (when *library*
            (when (memq *library-type* '(static both))
              (system*-fail (lambda (ret) (fail "Could not copy the static library\nReturned " ret))
                            "cp" (string-append *library* ".a") (string-append prefix fss "lib" fss (basename (string-append *library* ".a")))))
            (when (memq *library-type* '(dynamic both))
              (system*-fail (lambda (ret) (fail "Could not copy the dynamic library\nReturned " ret))
                            "cp" (string-append *library* ".so") (string-append prefix fss "lib" fss (basename (string-append *library* ".so")))))))))
  (return-fail))

;; This is a careful clean that tries to target what it itself may have produced
;; If you truly need a nuclear option, it shall be provided
(define* (clean  #:optional (conditional #t) #:key (nuclear #f))
  (when conditional
    (info "Cleaning builds")
    (if (or nuclear (not *rmdir-command*))
        (system*-fail (lambda (ret) (fail "Could not delete the build directory\nReturned " ret))
                      "rm" "-r" *build-directory*)
        (begin
          (when (file-exists? (in-vicinity *build-directory* "obj"))
            (delete-dir (in-vicinity *build-directory* "obj")))
          (delete-dir *obj-build-directory*)
          (for-each delete-dir
                    (map (lambda (f) (in-vicinity *metadata* f))
                         (filter (lambda (f) (not (or (equal? f ".") (equal? f ".."))))
                                 (scandir *metadata*))))
          (delete-dir *metadata*)
          (delete-dir *build-directory*))))
  (return-fail))
