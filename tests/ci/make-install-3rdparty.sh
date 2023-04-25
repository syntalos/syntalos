#!/bin/sh
set -e
set -x

mkdir -p 3rdparty && cd 3rdparty

# TOML++ to read and write TOML files
git clone --depth 1 https://github.com/marzer/tomlplusplus.git toml++
mkdir toml++/b && cd toml++/b/
meson -Dbuild_tests=false ..
ninja && ninja install
cd ../..

# PoMiDAQ for Miniscope support
git clone --depth 1 https://github.com/bothlab/pomidaq.git pomidaq
mkdir pomidaq/b && cd pomidaq/b/
cmake -GNinja -DPYTHON=OFF ..
ninja && ninja install
cd ../..

# Support for "The Imaging Source" cameras
git clone --depth 1 --branch=v-tiscamera-1.1.0 https://github.com/TheImagingSource/tiscamera.git tiscamera
mkdir tiscamera/b && cd tiscamera/b/
cmake -GNinja \
    -DTCAM_BUILD_DOCUMENTATION=OFF \
    -DTCAM_BUILD_WITH_GUI=OFF \
    -DTCAM_DOWNLOAD_MESON=OFF \
    ..
ninja && ninja install
cd ../..

# cleanup
cd .. && rm -rf 3rdparty
