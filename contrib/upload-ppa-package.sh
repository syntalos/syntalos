#!/bin/bash
set -euo pipefail

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

git_commit=$(git rev-parse --short HEAD)
git_current_tag=$(git describe --tags --abbrev=0 2>/dev/null || echo v2.0.0)
git_commit_no=$(git rev-list --count "${git_current_tag}..HEAD" 2>/dev/null)
upstream_version=${git_current_tag#v}; upstream_version=${upstream_version//-/.}
if [ "$git_commit_no" -gt 0 ]; then
  upstream_version+="+git$git_commit_no"
fi

mv contrib/debian .
dch --distribution "resolute" --newversion="${upstream_version}" -b \
    "New automated build from: ${upstream_version} - ${git_commit}"

# build the source package
debuild -S -sa -d

# cleanup & upload
cd ..
rm -r syntalos
dput ppa:ximion/syntalos *.changes
rm *.changes
