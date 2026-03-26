#!/bin/sh
set -e

#
# This script is supposed to run inside the Syntalos Docker container
# on the CI system.
#

echo "C compiler: $CC"
echo "C++ compiler: $CXX"
export LANG=C.UTF-8

set -x
$CXX --version

# intan-rhx only builds on amd64
if [ "$(uname -m)" = "x86_64" ]; then
    extra_modules=",intan-rhx"
else
    extra_modules=""
fi

# configure Syntalos build with all flags enabled
mkdir build && cd build
meson \
    -Dmaintainer=true \
    -Dgui-tests=true \
    -Dmodules="camera-arv,camera-tis,miniscope,plot${extra_modules}" \
    ..

# Build, Install & Test
ninja
DESTDIR=/tmp/install_root/ ninja install

# We need a (fake) display, because GUI tests are enabled
if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
    echo "Running headless with xvfb and dbus"
    xvfb-run -a -s "-screen 0 1400x900x24" \
            dbus-run-session -- \
            meson test -v --print-errorlogs
else
    meson test --print-errorlogs
fi
