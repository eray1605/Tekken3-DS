#!/bin/sh
# Runs a ROM in DeSmuME on a virtual X display and saves both screens (256 x 384) as a PNG.
#   tools/ds/screenshot.sh <rom.nds> <out.png> [seconds] [key ...]
# Keys (xdotool names; DeSmuME's default keyboard map: arrows, x = A, z = B, s = X, a = Y, q = L,
# w = R, Return = Start, BackSpace = Select) are held from about one second after boot until the shot.
# Needs desmume-cli, Xvfb, xdotool and ImageMagick (tools/ds/setup_toolchain.sh installs them).
set -eu
ROM=$1
OUT=$2
SECS=${3:-4}
shift $(( $# < 3 ? $# : 3 ))
DISPLAY_NUM=${DS_DISPLAY:-:77}
export PATH="$PATH:/usr/games"

Xvfb "$DISPLAY_NUM" -screen 0 640x480x24 >/dev/null 2>&1 &
XVFB=$!
EMU=
trap 'kill $EMU $XVFB 2>/dev/null || true' EXIT
sleep 1
DISPLAY=$DISPLAY_NUM SDL_AUDIODRIVER=dummy desmume-cli --disable-sound --nojoy=1 "$ROM" >/dev/null 2>&1 &
EMU=$!
WINDOW=$(DISPLAY=$DISPLAY_NUM timeout 10 xdotool search --sync --onlyvisible --name Desmume | head -n 1)
for key in "$@"; do
    DISPLAY=$DISPLAY_NUM xdotool keydown --window "$WINDOW" "$key"
done
sleep "$SECS"
DISPLAY=$DISPLAY_NUM import -window "$WINDOW" "$OUT"
