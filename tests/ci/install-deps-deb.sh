#!/bin/sh
set -e
set -x

export DEBIAN_FRONTEND=noninteractive

# update caches
apt-get update -qq

# install build essentials
apt-get install -yq \
    eatmydata build-essential gdb gcc g++

# install build dependencies
eatmydata apt-get install -yq --no-install-recommends \
    git ca-certificates \
    xvfb xauth \
    python3-pip \
    ninja-build \
    cmake \
    gettext \
    libegl-dev \
    qtbase5-dev \
    libeigen3-dev \
    libglib2.0-dev \
    libqt5serialport5-dev \
    libqt5opengl5-dev \
    libqt5charts5-dev \
    libqt5svg5-dev \
    libqt5remoteobjects5-dev \
    libqt5remoteobjects5-bin \
    libopencv-dev \
    libavcodec-dev \
    libavutil-dev \
    libavformat-dev \
    libswscale-dev \
    libxxhash-dev \
    python3-dev \
    pybind11-dev \
    uuid-dev \
    libkf5archive-dev \
    libkf5texteditor-dev \
    python3-numpy \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    udev

# install additional dependencies
eatmydata apt-get install -yq --no-install-recommends \
    gobject-introspection \
    libgirepository1.0-dev \
    libusb-1.0-0-dev \
    libzip-dev \
    libudev-dev

pip install git+https://github.com/ximion/meson.git@wip/gencustomdep
