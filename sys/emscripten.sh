#!/bin/sh
# find root
cd `dirname $PWD/$0` ; cd ..
#TODO: add support for ccache

# XXX. fails with >1
[ -z "${MAKE_JOBS}" ] && MAKE_JOBS=8

OLD_LDFLAGS="${LDFLAGS}"
unset LDFLAGS

export CC="emcc --ignore-dynamic-linking -Oz"
export AR="emar"

CFGFLAGS="./configure --prefix=/usr --with-compiler=emscripten"
CFGFLAGS="${CFGFLAGS} --disable-debugger --without-pic --with-nonpic"

make mrproper
cp -f plugins.emscripten.cfg plugins.cfg
./configure-plugins

./configure ${CFGFLAGS} --host=emscripten && \
	make -s -j ${MAKE_JOBS} DEBUG=0
