#!/bin/sh
set -e
set -x
export LANG=C.UTF-8

mkdir -p 3rdparty && cd 3rdparty

# Iceoryx2
git clone --depth 1 https://github.com/eclipse-iceoryx/iceoryx2.git iceoryx2
cd iceoryx2
cmake -B build -GNinja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_CXX=ON \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DIOX2_FEATURE_LIBC_PLATFORM=ON
cmake --build build
cmake --install build
cd ..

# PoMiDAQ for Miniscope support
git clone --depth 1 https://github.com/bothlab/pomidaq.git pomidaq
mkdir pomidaq/b && cd pomidaq/b/
cmake -GNinja -DPYTHON=OFF ..
ninja && ninja install
cd ../..

# Support for "The Imaging Source" cameras
git clone --depth 1 --branch=development https://github.com/TheImagingSource/tiscamera.git tiscamera
mkdir tiscamera/b && cd tiscamera/b/
cmake -GNinja \
    -DTCAM_BUILD_DOCUMENTATION=OFF \
    -DTCAM_BUILD_WITH_GUI=OFF \
    -DTCAM_DOWNLOAD_MESON=OFF \
    -DTCAM_INTERNAL_ARAVIS=OFF \
    -DTCAM_INSTALL_FORCE_PREFIX=ON \
    ..
ninja && ninja install
cd ../..

# cleanup
cd .. && rm -rf 3rdparty
