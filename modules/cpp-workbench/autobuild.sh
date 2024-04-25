#!/bin/sh

run_command() {
    echo "$@"
    "$@"  # execute the command
    status=$?  # capture the exit status

    # exit if the build failed
    if [ $status -ne 0 ]; then
        echo "Build failed."
        # write status file
        echo "$status" > autobuild.status
        exit $status
    fi
}

if [ ! -f "build.ninja" ]; then
    run_command meson setup \
        --buildtype=debugoptimized \
        --pkg-config-path $PKG_CONFIG_PATH \
        ..
fi

run_command ninja

# if we didn't quit by now, the build was a success
echo "0" > autobuild.status
