#!/bin/bash

pushd `dirname $0`
export LD_LIBRARY_PATH=/opt/circonus/lib/amd64:/opt/circonus/lib
export DYLD_LIBRARY_PATH=/opt/circonus/lib/amd64:/opt/circonus/lib:..

# force our known luajit to the front of the queue
export PATH=/opt/circonus/bin:$PATH

if command -v luajit || [ ! -z "$LUA_BIN" ]
then # lua in path or explicitly speficied
    LUA_BIN="$(command -v luajit)"
else # search default locations
    case $(uname) in
        SunOS)
            LUA_BIN=${LUA_BIN:-"/opt/circonus/bin/luajit"}
            ;;
        Linux)
            LUA_BIN=${LUA_BIN:-"/usr/local/bin/luajit"}
            ;;
    esac
fi

if [ -x "$LUA_BIN" ]
then
    echo "Using interpreter $LUA_BIN"
else
    echo "Lua executable not found: $LUA_BIN"
    exit 1
fi

export LUA_PATH="\
/opt/circonus/share/lua/5.1/?.lua;\
?.lua;\
../lua/?.lua;\
$($LUA_BIN -e "print(package.path)")"

export LUA_CPATH="\
/opt/circonus/share/lua/5.1/?.so;\
/opt/circonus/lib/amd64/lua/5.1/?.so;\
/opt/circonus/lib/lua/5.1/?.so;\
"

echo "set LUA_PATH=$LUA_PATH"

export LUA_INIT=''
arg=''
DEBUG_PREFIX=""

echo "Running C tests"
./histogram_test

echo "Running: $ $LUA_BIN $LUA_ARGS -l histogram_c_test -e \"histogram_c_test.runTests()\""
$LUA_BIN $LUA_ARGS -l histogram_c_test -e "histogram_c_test.runTests()"

popd
