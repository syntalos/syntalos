#!/bin/sh
# Run a command inside a fresh Xvfb virtual display.
#
# Unlike xvfb-run, this script does NOT rely on Xvfb sending SIGUSR1 to signal
# readiness.  That mechanism is broken on Wayland desktop sessions (e.g. KDE/KWin)
# because KWin already uses SIGUSR1 for its own IPC, causing the signal to be
# swallowed and xvfb-run to hang indefinitely waiting for it.
#
# Instead, we start Xvfb and poll the display with xdpyinfo until it accepts
# connections, then run the command and clean up.
#
# Usage: run-with-xvfb.sh [--screen WxHxD] -- COMMAND [ARGS...]
#   --screen WxHxD   Xvfb screen geometry (default: 1024x768x24)

set -e

SCREEN="1024x768x24"
while [ $# -gt 0 ]; do
    case "$1" in
        --screen) SCREEN="$2"; shift 2 ;;
        --) shift; break ;;
        *) break ;;
    esac
done

if [ -z "$*" ]; then
    echo "Usage: $0 [--screen WxHxD] -- COMMAND [ARGS...]" >&2
    exit 2
fi

# Find a free display number
DISPNUM=100
while [ -f "/tmp/.X${DISPNUM}-lock" ]; do
    DISPNUM=$((DISPNUM + 1))
done
DISPLAY=":${DISPNUM}"

# Clean up Xvfb on exit
XVFB_PID=
cleanup() {
    if [ -n "$XVFB_PID" ]; then
        kill "$XVFB_PID" 2>/dev/null || true
        wait "$XVFB_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# Start Xvfb without relying on SIGUSR1 readiness signalling
Xvfb "$DISPLAY" -screen 0 "$SCREEN" -nolisten tcp +extension GLX 2>/dev/null &
XVFB_PID=$!

# Poll until the display is accepting connections (max 10 s)
TRIES=0
until DISPLAY="$DISPLAY" xdpyinfo >/dev/null 2>&1; do
    TRIES=$((TRIES + 1))
    if [ $TRIES -ge 100 ]; then
        echo "run-with-xvfb.sh: Xvfb did not become ready on $DISPLAY within 10 seconds" >&2
        exit 1
    fi
    # Also bail out early if Xvfb already died
    if ! kill -0 "$XVFB_PID" 2>/dev/null; then
        echo "run-with-xvfb.sh: Xvfb exited unexpectedly" >&2
        exit 1
    fi
    sleep 0.1
done

# Run the requested command with the virtual display
set +e
DISPLAY="$DISPLAY" XAUTHORITY="" "$@"
RETVAL=$?
set -e

exit $RETVAL
