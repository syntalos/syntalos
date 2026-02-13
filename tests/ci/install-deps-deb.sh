#!/bin/sh
set -e
set -x

export DEBIAN_FRONTEND=noninteractive

# update caches
apt-get update -qq

# install build essentials
apt-get install -yq \
    eatmydata curl build-essential gdb gcc g++

. /etc/os-release
if [ "$ID" = "ubuntu" ]; then
    extra_deps="libqtermwidget6-2-dev"
else
    extra_deps="libqtermwidget-dev"
fi;

# install build dependencies
eatmydata apt-get install -yq --no-install-recommends \
    git ca-certificates \
    xvfb xauth \
    python3-pip \
    cmake \
    gettext \
    libaravis-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libegl-dev \
    libeigen3-dev \
    libglib2.0-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libkf6archive-dev \
    libkf6texteditor-dev \
    libkf6dbusaddons-dev \
    libopencv-dev \
    libpipewire-0.3-dev \
    libqt6opengl6-dev \
    libiceoryx-introspection-dev \
    libqt6svg6-dev \
    libswscale-dev \
    libtomlplusplus-dev \
    libusb-1.0-0-dev \
    libv4l-dev \
    libxml2-dev \
    libxxhash-dev \
    libsystemd-dev \
    systemd-dev \
    meson \
    ninja-build \
    ocl-icd-opencl-dev \
    pybind11-dev \
    python3-dev \
    python3-numpy \
    python3-pdoc \
    qt6-base-dev \
    qt6-serialport-dev \
    qt6-multimedia-dev \
    udev \
    uuid-dev \
    liblua5.3-dev \
    $extra_deps

# NOTE: liblua5.3-dev is only needed for Intan RHX

# install additional dependencies
eatmydata apt-get install -yq --no-install-recommends \
    gobject-introspection \
    libgirepository1.0-dev \
    libusb-1.0-0-dev \
    libzip-dev \
    libudev-dev \
    cargo
