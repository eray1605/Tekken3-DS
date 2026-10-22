#!/bin/sh
# Starts the DS build (ds/tekken3.nds) in melonDS: double-click it in the Finder (macOS).
# Brings the ROM up to date first (make nds) when the DS toolchain is installed
# (tools/ds/setup_toolchain.sh); otherwise it starts the ROM as it is. MELONDS may name the
# emulator's binary; by default melonDS.app in /Applications or ~/Applications.
cd "$(dirname "$0")" || exit 1
ROM="$PWD/ds/tekken3.nds"

if [ -d "$HOME/.local/blocksds/core" ] || [ -d /opt/blocksds/core ]; then
    make nds || echo "make nds failed: starting the last built ROM"
fi
if [ ! -f "$ROM" ]; then
    echo "no ROM at $ROM: run make convert-ds and make nds first (docs/ds-port-plan.md)"
    exit 1
fi

if [ -z "${MELONDS:-}" ]; then
    for app in /Applications/melonDS.app "$HOME/Applications/melonDS.app"; do
        if [ -x "$app/Contents/MacOS/melonDS" ]; then
            MELONDS="$app/Contents/MacOS/melonDS"
            break
        fi
    done
fi
if [ -z "${MELONDS:-}" ]; then
    echo "melonDS not found: put melonDS.app into /Applications or ~/Applications, or set MELONDS"
    exit 1
fi

nohup "$MELONDS" "$ROM" >/dev/null 2>&1 &
echo "melonDS started with $ROM"
