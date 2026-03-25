#!/bin/bash
set -e

#
# This script is supposed to be run by the GitHub CI system.
#

if [ -z "$1" ]
then
    echo "Needs a distribution as first parameter"
fi
if [ -z "$2" ]
then
    echo "Needs a distribution slug as second parameter"
fi

SUITE="$1"
SLUG_EXT="$2"

# Make subprojects available locally
git submodule update --init --recursive
meson subprojects download

#
# Set up the debian/ directory for the build.
#

git_commit=$(git rev-parse --short HEAD)
git_current_tag=$(git describe --tags --abbrev=0 2>/dev/null || echo v0.0.0)
git_commit_no=$(git rev-list --count "${git_current_tag}..HEAD" 2>/dev/null)
upstream_version=${git_current_tag#v}; upstream_version=${upstream_version//-/.}
if [ "$git_commit_no" -gt 0 ]; then
  upstream_version+="+git$git_commit_no"
fi

set +x
cp -dpr contrib/debian .
dch --distribution "${SUITE}"	--newversion="${upstream_version}~${SLUG_EXT}" -b \
    "New automated build for: ${upstream_version} - ${git_commit}"
