#!/bin/sh
set -e
set -x

mkdir -p 3rdparty && cd 3rdparty

# PoMiDAQ for Miniscope support
git clone --depth 1 https://github.com/bothlab/pomidaq.git pomidaq
mkdir pomidaq/b && cd pomidaq/b/
cmake -GNinja -DPYTHON=OFF ..
ninja && ninja install
cd ../..

# Support for "The Imaging Source" cameras
git clone --depth 1 --branch=master https://github.com/TheImagingSource/tiscamera.git tiscamera
mkdir tiscamera/b && cd tiscamera/b/
cmake -GNinja \
    -DTCAM_BUILD_DOCUMENTATION=OFF \
    -DTCAM_BUILD_WITH_GUI=OFF \
    -DTCAM_DOWNLOAD_MESON=OFF \
    -DTCAM_INTERNAL_ARAVIS=OFF \
    ..
ninja && ninja install
cd ../..

# cleanup
cd .. && rm -rf 3rdparty
