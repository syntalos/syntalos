#
# Docker file for MazeAmaze tests
#
FROM debian:bullseye

# add experimental, we need it for remoteobjects for now
RUN echo "deb http://deb.debian.org/debian experimental main" > /etc/apt/sources.list.d/experimental.list

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
    libegl-dev \
    qt5-default \
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
    libczmq-dev \
    uuid-dev \
    libkf5archive-dev \
    libkf5texteditor-dev \
    python3-numpy

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
