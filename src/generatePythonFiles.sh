#!/bin/sh
set -e

STDIN=$(cat)

cat <<EOF
from cffi import FFI
ffi = FFI()
ffi.cdef("""
${STDIN}
""")
C = None
for path in [ # Search for libcircllhist.so
   "/opt/circonus/lib/libcircllhist.so", # 1. vendor path
   "./libcircllhist.so", # 2. cwd
   "libcircllhist.so" # 3. system paths via ld.so
   ]:
   try:
     C = ffi.dlopen(path)
     break
   except OSError:
     pass
if not C:
   # let dlopen throw it's error
   ffi.dlopen("libcircllhist.so")
EOF
