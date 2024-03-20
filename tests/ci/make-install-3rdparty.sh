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
mkdir tiscamera && cd tiscamera
curl -L -o tiscamera.deb https://github.com/TheImagingSource/tiscamera/releases/download/v-tiscamera-1.1.1/tiscamera_1.1.1.4142_amd64_ubuntu_1804.deb
curl -L -o tiscamera-dev.deb https://github.com/TheImagingSource/tiscamera/releases/download/v-tiscamera-1.1.1/tiscamera-dev_1.1.1.4142_amd64_ubuntu_1804.deb
apt-get install -yq ./tiscamera.deb ./tiscamera-dev.deb
cd ../

# cleanup
cd .. && rm -rf 3rdparty
