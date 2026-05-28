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
mkdir build && cd build
meson \
    -Dmaintainer=true \
    -Dgui-tests=true \
    -Dmodules="camera-arv,camera-tis,miniscope${extra_modules}" \
    ..

# Build & Install
ninja
DESTDIR=/tmp/install_root/ ninja install
