#!/bin/sh
set -e
set -x

export LANG=C.UTF-8
export DEBIAN_FRONTEND=noninteractive

# update caches
apt-get update -qq

# Install module dependencies:
# * liblua5.3-dev for intan-rhx (Intan RHX)
# * tiscamera-dev for camera-tis (TIS Camera)
# * libminiscope-dev for miniscope (Miniscope)

# On Debian testing, instead of installing debs, we currently build stuff from source,
# so we don't run into dependency issues.

. /etc/os-release
if [ "$ID" = "debian" ] && [ "$VERSION_CODENAME" = "forky" ]; then
    eatmydata apt-get install -yq --no-install-recommends \
        liblua5.3-dev \
        gobject-introspection \
        libgirepository1.0-dev \
        libusb-1.0-0-dev \
        libzip-dev \
        libudev-dev

    mkdir -p 3rdparty && cd 3rdparty
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
else
    eatmydata apt-get install -yq --no-install-recommends \
        liblua5.3-dev \
        tiscamera-dev \
        libminiscope-dev
fi;
