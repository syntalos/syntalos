#!/usr/bin/env bash
set -e

BASEDIR=$(dirname "$0")
cd $BASEDIR

app_svg="syntalos.svg"
sizes="48 64 128"

inkscape --version
optipng -v

echo -e "\n========================="
echo "Rendering vector graphics"
echo "========================="

for size in $sizes; do
    icon_dir="appicons/${size}x${size}/apps"
    mkdir -p $icon_dir
    target="$icon_dir/"$( echo $app_svg | cut -d . -f -1 ).png
    echo "converting $app_svg to $target"
    inkscape \
        --export-type=png \
        --export-filename=$target \
        --export-dpi=96 \
        --export-background-opacity=0 \
        --export-width=$size \
        --export-height=$size \
        "$app_svg"
    optipng -o7 $target
done
