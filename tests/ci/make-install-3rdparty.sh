#!/bin/sh
set -e
set -x

mkdir -p 3rdparty

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
# FIXME: We need to migrate off of the 0.14.x branch
git clone --depth 1 --branch=v-tiscamera-0.14.0 https://github.com/TheImagingSource/tiscamera.git tiscamera
mkdir tiscamera/b && cd tiscamera/b/
cmake -GNinja ..
ninja && ninja install
cd ../..
