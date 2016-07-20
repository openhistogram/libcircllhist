#!/bin/sh

if [ $# -lt 2 ]
then
	echo "Usage: generateLuaFile.sh <libcircllhist_src_dir> <destination_file>"
	exit
fi

src_dir=$1
dst_file=$2

if [ -x /usr/bin/gsed ]; then
    SED=gsed
else
    SED=sed
fi

cdef_content=`cat $src_dir/circllhist.h | egrep -v "#if|#endif|#define|#include" | $SED 's/u_int/uint/g' | $SED 's/API_EXPORT(\([a-z0-9_]*\))/\1/g' | $SED '/typedef struct histogram histogram_t;/ i\typedef long int ssize_t;\n'`

D=`dirname $dst_file`
if [ ! -e $D ]; then
    mkdir $D
fi

cat > $dst_file <<EOF
local ffi = require('ffi');

--------------------------------------------------------------------------------
-- circllhist.h
--------------------------------------------------------------------------------
ffi.cdef[[
${cdef_content}
]]

return ffi.load("libcircllhist")
EOF
