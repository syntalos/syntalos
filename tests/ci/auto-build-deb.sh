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

git_commit=$(git log --pretty="format:%h" -n1)
git_current_tag=$(git describe --abbrev=0 --tags 2> /dev/null || echo "v3.0.2")
git_commit_no=$(git rev-list --count HEAD)
upstream_version=$(echo "${git_current_tag}" | sed 's/^v\(.\+\)$/\1/;s/[-]/./g')
upstream_version="$upstream_version+git$git_commit_no"

set +x
mv contrib/debian .
dch --distribution "UNRELEASED"	--newversion="${upstream_version}" -b \
    "New automated build from: ${upstream_version} - ${git_commit}"

# Actually build the package.
# Use a helper like `debspawn` or `debuild` when building manually!
mkdir -p result
dpkg-buildpackage -d
mv ../*.deb ./result/
mv ../*.build* ./result/
