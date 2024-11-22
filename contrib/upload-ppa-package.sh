#!/bin/sh
set -e

#
# This script will create the Syntalos Debian package & upload it to the PPA
#

set -x
mkdir -p _ppa-pkg-build
cd _ppa-pkg-build

# create clean source copy from current Git tree
mkdir -p syntalos
git -C "$(git rev-parse --show-toplevel)" archive HEAD | tar -x -C ./syntalos/
rsync -a $(git rev-parse --show-toplevel)/contrib/vendor/. ./syntalos/contrib/vendor/
cd syntalos

#
# Dirty hack to allow us to build the tiscam module without having the TIS packages
# in our PPA.
#
mkdir -p contrib/vendor/
cd contrib/vendor/
git clone --depth 1 --branch=master https://github.com/TheImagingSource/tiscamera.git tiscam
rm -rf tiscam/.git
cd ../../

#
# Build Debian source package
#

git_commit=$(git log --pretty="format:%h" -n1)
git_current_tag=$(git describe --abbrev=0 --tags 2> /dev/null || echo "v2.0.0")
git_commit_no=$(git rev-list --count HEAD)
upstream_version=$(echo "${git_current_tag}" | sed 's/^v\(.\+\)$/\1/;s/[-]/./g')
upstream_version="$upstream_version+git$git_commit_no"

mv contrib/debian .
dch --distribution "noble" --newversion="${upstream_version}" -b \
    "New automated build from: ${upstream_version} - ${git_commit}"

# build the source package
debuild -S -sa -d

# cleanup & upload
cd ..
rm -r syntalos
dput ppa:ximion/syntalos *.changes
rm *.changes
