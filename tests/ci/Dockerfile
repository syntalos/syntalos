#
# Docker file for Syntalos tests
#
FROM ubuntu:focal

# prepare
RUN apt-get update -qq

# install build essentials
RUN DEBIAN_FRONTEND=noninteractive apt-get install -yq \
    eatmydata build-essential gdb gcc g++

# install build dependencies
RUN DEBIAN_FRONTEND=noninteractive eatmydata apt-get install -yq --no-install-recommends \
    git ca-certificates \
    xvfb xauth \
    meson \
    ninja-build \
    cmake \
    gettext \
    libegl-dev \
    qt5-default \
    libeigen3-dev \
    libglib2.0-dev \
    libqt5serialport5-dev \
    libqt5opengl5-dev \
    libqt5charts5-dev \
    libqt5svg5-dev \
    libqt5remoteobjects5-dev \
    libqt5remoteobjects5-bin \
    libboost-container-dev \
    libboost-python-dev \
    libopencv-dev \
    libavcodec-dev \
    libavutil-dev \
    libavformat-dev \
    libswscale-dev \
    uuid-dev \
    libkf5archive-dev \
    libkf5texteditor-dev \
    python3-numpy \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev

# build & install 3rd-party libraries
RUN mkdir -p /build/extra
WORKDIR /build/extra

# TOML++ to read and write TOML files
RUN git clone --depth 1 https://github.com/marzer/tomlplusplus.git toml++
RUN mkdir toml++/b
RUN cd toml++/b/ && meson -DBUILD_TESTS=disabled ..
RUN cd toml++/b/ && ninja && ninja install

# PoMiDAQ for Miniscope support
RUN git clone --depth 1 https://github.com/bothlab/pomidaq.git pomidaq
RUN mkdir pomidaq/b
RUN cd pomidaq/b/ && cmake -GNinja --prefix=/usr -DPYTHON=OFF ..
RUN cd pomidaq/b/ && ninja && ninja install

# Support for "The Imaging Source" cameras
RUN git clone --depth 1 https://github.com/TheImagingSource/tiscamera.git tiscamera
RUN mkdir tiscamera/b
RUN cd tiscamera/b/ && cmake -GNinja ..
RUN cd tiscamera/b/ && ninja && ninja install

# finish
RUN mkdir -p /build
WORKDIR /build
