#!/bin/bash
set -e

cd $(dirname "$0")

build_shell=""
if [ ! -z "$1" ]
then
	echo "Interactive shell at step: $1"
	build_shell="--build-shell=$1"
fi
set -x

flatpak-builder -y \
	--install-deps-from=flathub \
	--force-clean \
	--repo=tmp_repo \
	$build_shell \
	build-dir \
	org.syntalos.syntalos.yaml

rm -f Syntalos.flatpak
flatpak build-bundle \
	--runtime-repo=https://flathub.org/repo/flathub.flatpakrepo \
	tmp_repo/ \
	Syntalos.flatpak \
	org.syntalos.syntalos

rm -r tmp_repo/
