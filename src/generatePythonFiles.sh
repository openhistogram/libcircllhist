#!/bin/sh

if [ $# -lt 2 ]
then
  echo "Usage: $0 <libcircllhist_src_dir> <destination_file>"
  exit
fi

src_dir=$1
dst_file=$2

if [ -x /usr/bin/gsed ]; then
    SED=gsed
else
    SED=sed
fi
AWK=awk

cdef_content=`cat $src_dir/circllhist.h | egrep -v "#if|#endif|#define|#include" | $SED 's/u_int/uint/g' | $SED 's/API_EXPORT(\([^\)]*\))/\1/g' | $AWK '/typedef struct histogram histogram_t;/ {print "typedef long int ssize_t;"} /./{print}'`

D=`dirname $dst_file`
if [ ! -e $D ]; then
    mkdir -p $D
fi

cat > $dst_file <<EOF
from cffi import FFI
ffi = FFI()
ffi.cdef("""
${cdef_content}
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
   ffi.dlopen("libcirllhist.so")
EOF
