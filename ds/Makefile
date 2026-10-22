# Tekken 3 DS: the Nintendo DS build (docs/ds-port-plan.md). `make` builds tekken3.nds.
#
# Needs BlocksDS: /opt/blocksds/core (BLOCKSDS) and its ARM toolchain (ARM_NONE_EABI_PATH), see
# tools/ds/setup_toolchain.sh. Builds that carry converted game data are private, like the remake's.

export BLOCKSDS ?= /opt/blocksds/core
ARM_NONE_EABI_PATH ?= $(if $(wildcard /opt/t3ds-toolchain/bin),/opt/t3ds-toolchain/bin/,/opt/wonderful/toolchain/gcc-arm-none-eabi/bin/)

NAME := tekken3
GAME_TITLE := Tekken 3 DS
GAME_SUBTITLE := Fan port, game data from your disc
GAME_AUTHOR := github.com/eray1605/Tekken3-DS

SOURCEDIRS := source
INCLUDEDIRS := source
DEFINES := -DT3DS_VERSION=\"$(shell git describe --always --dirty 2>/dev/null || echo dev)\"

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
