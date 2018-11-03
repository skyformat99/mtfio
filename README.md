# mtfio
Multithreaded File I/O library for C.
# To build:
Requirements: make, (build-tools) gcc.
  
Make sure to compile program using this library with `gcc -o *output* *input* -L[directory of library file] -lmtfio`.

When making multiple read/writes while wanting to be uninterrupted, be sure to use mtflock/mtfunlock to prevent misordered data!
