#!/bin/sh
set -e
set -x

export DEBIAN_FRONTEND=noninteractive

# update caches
apt-get update -qq

# install build essentials
apt-get install -yq \
    eatmydata curl build-essential gdb gcc g++

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
    libkf5archive-dev \
    libkf5texteditor-dev \
    libqtermwidget5-1-dev \
    libvips-dev \
    libopencv-dev \
    libpipewire-0.3-dev \
    libqt5opengl5-dev \
    libiceoryx-introspection-dev \
    libqt5serialport5-dev \
    libqt5svg5-dev \
    libswscale-dev \
    libsystemd-dev \
    libusb-1.0-0-dev \
    libv4l-dev \
    libxml2-dev \
    libxxhash-dev \
    meson \
    ninja-build \
    ocl-icd-opencl-dev \
    pybind11-dev \
    python3-dev \
    python3-numpy \
    qtbase5-dev \
    qtmultimedia5-dev \
    udev \
    uuid-dev

# install additional dependencies
eatmydata apt-get install -yq --no-install-recommends \
    gobject-introspection \
    libgirepository1.0-dev \
    libusb-1.0-0-dev \
    libzip-dev \
    libudev-dev
