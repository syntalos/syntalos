#!/bin/sh
set -e

#
# This script is supposed to run inside the Syntalos CI container
# on the CI system.
#

export LANG=C.UTF-8
set -x

cd build

# We need a (fake) display, because GUI tests are enabled
if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
    echo "Running headless with xvfb and dbus"
    xvfb-run -a -s "-screen 0 1400x900x24" \
            dbus-run-session -- \
            meson test -v --print-errorlogs
else
    meson test --print-errorlogs
fi
