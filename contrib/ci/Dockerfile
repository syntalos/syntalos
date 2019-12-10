#
# Docker file for MazeAmaze tests
#
FROM debian:bullseye

# prepare
RUN apt-get update -qq

# install build essentials
RUN apt-get install -yq eatmydata build-essential gcc g++

# install build dependencies
RUN DEBIAN_FRONTEND=noninteractive eatmydata apt-get install -yq --no-install-recommends \
    git ca-certificates \
    meson \
    ninja-build \
    cmake \
    gettext \
    qt5-default \
    libqt5serialport5-dev \
    libqt5opengl5-dev \
    libqt5charts5-dev \
    libqt5svg5-dev \
    libboost-container-dev \
    libboost-python-dev \
    libopencv-dev \
    libavcodec-dev \
    libavutil-dev \
    libavformat-dev \
    libswscale-dev \
    libczmq-dev \
    uuid-dev \
    libkf5archive-dev \
    libkf5texteditor-dev

# build & install 3rd-party libraries
RUN mkdir -p /build/extra
WORKDIR /build/extra

# PoMiDAQ for Miniscope support
RUN git clone --depth 1 https://github.com/bothlab/pomidaq.git pomidaq
RUN mkdir pomidaq/b
RUN cd pomidaq/b/ && cmake --prefix=/usr -DPYTHON=OFF ..
RUN cd pomidaq/b/ && make && make install

# finish
RUN mkdir -p /build
WORKDIR /build
