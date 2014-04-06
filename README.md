A toy lisp interpreter, based almost entirely (with some very minor changes) on the one described in [Build Your Own Lisp](http://www.buildyourownlisp.com/).

To build, you need:  OS X (doesn't compile on Windows or Linux), tested on 10.9.2 with:

```
Apple LLVM version 5.1 (clang-503.0.38) (based on LLVM 3.4svn)
Target: x86_64-apple-darwin13.1.0
Thread model: posix
```

You also need the mpc library from [here](https://github.com/orangeduck/mpc).

To compile:

```
cc -std=c99 -Wall -ledit mpc.c benlisp.c -o benlisp
```