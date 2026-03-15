#!/bin/sh
set -e

#
# This script is supposed to be run by the GitHub CI system.
#

if [ -z "$CI" ]; then
    echo "Not in a CI build environment. This script should not be run manually!"
    exit 1
fi

#
# Install packaging tools
#
export DEBIAN_FRONTEND=noninteractive
eatmydata apt-get install -yq --no-install-recommends \
    ca-certificates \
    git-core \
    dpkg-dev \
    debhelper \
    devscripts \
    libdistro-info-perl

#
# Build Debian package
#

./tests/ci/configure-deb-build.sh "UNRELEASED" "deb"

# Actually build the package.
# Use a helper like `debspawn` or `debuild` when building manually!
mkdir -p result
dpkg-buildpackage -d
mv ../*.deb ./result/
mv ../*.build* ./result/
