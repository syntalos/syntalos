#!/bin/sh
set -e

echo "C compiler: $CC"
echo "C++ compiler: $CXX"
set -x

#
# This script is supposed to run inside the Syntalos Docker container
# on the CI system.
#

$CC --version

# configure Syntalos build with all flags enabled
mkdir build && cd build
cmake -G Ninja \
      -DMAINTAINER=ON \
      -DMOD_CAMERA_UEYE=OFF \
      ..

# Build, Test & Install
# (the number of Ninja jobs needs to be limited, so Travis doesn't kill us)
ninja -j8
DESTDIR=/tmp/install_root/ ninja install
xvfb-run -a -s "-screen 0 1024x768x24 +extension GLX" \
	ninja test
