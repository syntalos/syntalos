#!/bin/sh
#
# Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
#
# Licensed under the GNU Lesser General Public License Version 3
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the license, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this software.  If not, see <http://www.gnu.org/licenses/>.

set -e
set -x

OPENCV_VERSION=4.1.0
WITH_PROTOBUF=OFF

apt-get install -yq --no-install-recommends \
    git \
    ca-certificates \
    cmake \
    build-essential \
    ninja-build \
    libavcodec-dev \
    libavformat-dev \
    libavresample-dev \
    libdc1394-22-dev \
    libeigen3-dev \
    libgdal-dev \
    libgdcm2-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libgoogle-glog-dev \
    libgphoto2-dev \
    libgtk-3-dev \
    libjpeg-dev \
    liblapack-dev \
    libleptonica-dev \
    libopenexr-dev \
    libpng-dev \
    libprotobuf-dev \
    libraw1394-dev \
    libswscale-dev \
    libtbb-dev \
    libtesseract-dev \
    libtiff-dev \
    libv4l-dev \
    libvtk7-dev \
    libvtkgdcm2-dev \
    libgdcm-tools \
    ocl-icd-opencl-dev \
    protobuf-compiler \
    python3-dev \
    python3-numpy \
    zlib1g-dev

if [ ! -d "opencv/" ]; then
  git clone --depth=1 --branch $OPENCV_VERSION https://github.com/opencv/opencv.git
fi

cd opencv
mkdir -p build && cd build

cmake -G Ninja \
    -DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON \
    -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON \
    -DCMAKE_INSTALL_RUNSTATEDIR=/run \
    -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu \
    -DANT_EXECUTABLE=/usr/bin/ant \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_PROTOBUF=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_opencv_face=ON \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_CXX_FLAGS_RELEASE=-g -O2 -fstack-protector-strong -Wformat -Werror=format-security" \
    "-DCMAKE_C_FLAGS_RELEASE=-g -O2 -fstack-protector-strong -Wformat -Werror=format-security" \
    "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=-Wl,-z,relro -Wl,-z,now" \
    -DCMAKE_SKIP_RPATH=ON \
    -DENABLE_PRECOMPILED_HEADERS=OFF \
    -DINSTALL_C_EXAMPLES=OFF \
    -DINSTALL_PYTHON_EXAMPLES=OFF \
    -DOPENCL_INCLUDE_DIR:PATH=/usr/include/CL/ \
    -DOPENCV_MATHJAX_RELPATH=/usr/share/javascript/mathjax/ \
    -DOPENCV_SKIP_PYTHON_LOADER=ON \
    -DOpenGL_GL_PREFERENCE=GLVND \
    -DPROTOBUF_UPDATE_FILES=ON \
    -DWITH_ADE=OFF \
    -DWITH_CAROTENE=OFF \
    -DWITH_CUDA=OFF \
    -DWITH_EIGEN=ON \
    -DWITH_FFMPEG=ON \
    -DWITH_GDAL=ON \
    -DWITH_GDCM=ON \
    -DWITH_GSTREAMER=OFF \
    -DWITH_GTK=ON \
    -DWITH_IPP=OFF \
    -DWITH_ITT=OFF \
    -DWITH_JASPER=OFF \
    -DWITH_JPEG=ON \
    -DWITH_OPENCL=ON \
    -DWITH_OPENEXR=ON \
    -DWITH_OPENGL=ON \
    -DWITH_PNG=ON \
    -DWITH_PVAPI=ON \
    -DWITH_QUIRC=OFF \
    -DWITH_TIFF=ON \
    -DWITH_UNICAP=OFF \
    -DWITH_VTK=ON \
    -DWITH_XINE=OFF \
    -DWITH_PROTOBUF=$WITH_PROTOBUF \
    -DCPU_DISPATCH= \
    -DCPU_BASELINE=SSE2 \
    -DCPU_BASELINE_REQUIRE=SSE2 \
    -DCPU_BASELINE_DISABLE=SSE3 \
    -DWITH_TBB=ON \
    -DWITH_1394=ON \
    -DWITH_V4L=ON \
    "-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=-Wl,-z,relro -Wl,-z,now" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_DOCS=OFF \
    -DBUILD_opencv_python2=OFF \
    ..


if [ -z "$NINJA_JOB_EXPLICIT_LIMIT" ]
then
    ninja
else
    NUMCPUS=`grep -c '^processor' /proc/cpuinfo`
    ninja -j$NUMCPUS -l$NUMCPUS
fi

ninja install
