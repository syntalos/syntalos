#!/bin/sh
set -e
set -x

pip install 'meson>=0.58'

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
curl https://github.com/TheImagingSource/tiscamera/releases/download/v-tiscamera-0.14.0/tiscamera_0.14.0.3054_amd64_ubuntu_1804.deb \
    -o tiscamera_0.14_amd64.deb --silent
apt-get install -y ./tiscamera_0.14_amd64.deb
rm tiscamera_0.14_amd64.deb
#git clone --depth 1 --branch=v-tiscamera-1.0.0 https://github.com/TheImagingSource/tiscamera.git tiscamera
#mkdir tiscamera/b && cd tiscamera/b/
#cmake -GNinja ..
#ninja && ninja install
#cd ../..
