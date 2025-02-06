#!/bin/sh
set -e

#
# This script is supposed to run inside the Syntalos Docker container
# on the CI system.
#

echo "C compiler: $CC"
echo "C++ compiler: $CXX"
export LANG=C.UTF-8

. /etc/os-release
maintainer_mode=true
if [ "$ID" = "ubuntu" ] && [ "$VERSION_CODENAME" = "focal" ]; then
    maintainer_mode=false
fi;

set -x
$CXX --version

# configure Syntalos build with all flags enabled
mkdir build && cd build
meson \
    -Dmaintainer=$maintainer_mode \
    ..

# Build, Test & Install
ninja
DESTDIR=/tmp/install_root/ ninja install
export CTEST_PROGRESS_OUTPUT=1
export CTEST_OUTPUT_ON_FAILURE=1
xvfb-run -a -s "-screen 0 1024x768x24 +extension GLX" \
    meson test --print-errorlogs
