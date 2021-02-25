#!/bin/sh
set -e
set -x

pip install git+https://github.com/mesonbuild/meson.git

mkdir -p 3rdparty

# TOML++ to read and write TOML files
git clone --depth 1 https://github.com/marzer/tomlplusplus.git toml++
mkdir toml++/b && cd toml++/b/
meson -DBUILD_TESTS=disabled ..
ninja && ninja install
cd ../..

# PoMiDAQ for Miniscope support
git clone --depth 1 https://github.com/bothlab/pomidaq.git pomidaq
mkdir pomidaq/b && cd pomidaq/b/
cmake -GNinja --prefix=/usr -DPYTHON=OFF ..
ninja && ninja install
cd ../..

# Support for "The Imaging Source" cameras
git clone --depth 1 https://github.com/TheImagingSource/tiscamera.git tiscamera
mkdir tiscamera/b && cd tiscamera/b/
cmake -GNinja ..
ninja && ninja install
cd ../..
