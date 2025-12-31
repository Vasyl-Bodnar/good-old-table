# build.scm

Build system library and script for C with no dependencies other than Guile and a few system commands.

Capable of caching and has plenty of options to tweak, while being kept simple.

Currently work in progress.

## Use

For Quickstart:

Copy `buildlib.scm` into root folder. 
Then create a `build.scm` file with the following base:

``` scheme
#! /usr/local/bin/guile -s
!#

(add-to-load-path ".")
(use-modules (buildlib))

(configure #:exe-name "my-app")

(compile-c)

(install)

;; (clean) ;; You can also provide a condition here
```

You can also find a slightly more involved example in the testing `build.scm`.
Otherwise you have the full power of Scheme to make it work for your usecase.

Note that `configure` has defaults, e.g. `#:c-compiler "cc" #:source-dir "src" #:build-dir "build" #:optimization "-O0"`.
If you do not find these appealing, do change them.

You can also disable the default behaviour to exit with a failure when encountering an error using `(disable-default-failure)`. 
In that case you should handle the `'ok` and `'fail` symbols for all commands

More information can be seen in the `buildlib.scm` file.

After that, if you have Guile Scheme installed, and the `build.scm` is executable, you can simply run it, `./build.scm`.
Or of course `guile build.scm`.

For Guile, you can also copy the script or compile it to your site folder 
(see `(display %load-path)` for script and `(display %load-compiled-path)` for compiled files). 
This change would allow you to get rid of `(add-to-load-path)`.
However, note that copying the script directly makes it easier to modify and distribute.

## Support and Compatibility

Compatibility with other Schemes is dubious and untested.

This is intended to work on *nix systems, others may or may not work.
