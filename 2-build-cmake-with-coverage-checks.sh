#!/usr/bin/env bash

set -e

WORKDIR=$(dirname "$0")
cd "${WORKDIR}"
WORKDIR=$(pwd)

BUILDDIR=cmake-build

if [[ "$(uname -s)" =~ ^MSYS_NT.* ]] || [[ "$(uname -s)" =~ Msys$ ]]
then
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:/opt/mlt-7.2.0/lib/pkgconfig"
fi

mkdir -p "${BUILDDIR}" && cd "${BUILDDIR}"

mode="Release"
if [[ -n $1 ]]
then
mode=$1
fi

cmake -GNinja -DENABLE_TESTS=ON -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_BUILD_TYPE="${mode}" -DCMAKE_C_FLAGS="-ftest-coverage -fprofile-arcs" -DCMAKE_CXX_FLAGS="-ftest-coverage -fprofile-arcs" ..
cmake --build .
cmake --install . >/dev/null
ctest --output-on-failure

echo
echo
echo "Done. Please check your Synfig installation in"
echo " ${WORKDIR}/${BUILDDIR}/output/$mode/"
echo
echo "You can start Synfig by executing"
echo " ${WORKDIR}/${BUILDDIR}/output/$mode/bin/synfigstudio"
echo
