#!/bin/sh
set -e

#
# This script is supposed to run inside the Syntalos CI container
# on the CI system.
#

echo "C compiler: $CC"
echo "C++ compiler: $CXX"
export LANG=C.UTF-8

set -x
$CXX --version

# intan-rhx only builds on amd64
if [ "$(uname -m)" = "x86_64" ]; then
    extra_modules=",intan-rhx,open-ephys-acq"
else
    extra_modules=""
fi

# configure Syntalos build with all flags enabled
# NOTE: We do enable optimizations (but not LTO), so tests can run a bit
# faster and more reliably on the low core-count CI runners.
mkdir build && cd build
meson setup \
    -Dbuildtype=debugoptimized \
    -Db_lto=false \
    -Dmaintainer=true \
    -Dgui-tests=true \
    -Dmodules="camera-arv,camera-lc,camera-tis,miniscope${extra_modules}" \
    ..

# Build & Install
ninja
DESTDIR=/tmp/install_root/ ninja install
