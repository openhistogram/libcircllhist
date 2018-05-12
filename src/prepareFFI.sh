#!/bin/sh

if [ -x /usr/bin/gsed ]; then
    SED=gsed
else
    SED=sed
fi
AWK=awk

cat |\
  grep -v -F '/* FFI_SKIP */' |\
  grep -v "^$" |\
  $SED 's|//.*$||' |\
  egrep -v "#if|#endif|#define|#include" |\
  $SED 's/u_int/uint/g' |\
  $SED 's/API_EXPORT(\([^\)]*\))/\1/g' |\
  $AWK '/typedef struct histogram histogram_t;/ {print "typedef long int ssize_t;"} /./{print}'
