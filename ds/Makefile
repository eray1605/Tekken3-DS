# Tekken 3 DS: the Nintendo DS build (docs/ds-port-plan.md). `make` builds tekken3.nds.
#
# Needs BlocksDS: /opt/blocksds/core or ~/.local/blocksds/core (BLOCKSDS) and its ARM toolchain
# (ARM_NONE_EABI_PATH), see tools/ds/setup_toolchain.sh. Builds that carry converted game data are
# private, like the remake's.

export BLOCKSDS ?= $(firstword $(wildcard /opt/blocksds/core $(HOME)/.local/blocksds/core) /opt/blocksds/core)
ARM_NONE_EABI_PATH ?= $(firstword $(addsuffix /,$(wildcard /opt/t3ds-toolchain/bin $(HOME)/.local/t3ds-toolchain/bin)) \
    /opt/wonderful/toolchain/gcc-arm-none-eabi/bin/)

NAME := tekken3
GAME_TITLE := Tekken 3 DS
GAME_SUBTITLE := Fan port, game data from your disc
GAME_AUTHOR := github.com/eray1605/Tekken3-DS

SOURCEDIRS := source
INCLUDEDIRS := source
# The converted data (make convert-ds), packed when it exists: game data, so the ROM is private.
NITROFSDIR := $(if $(wildcard nitrofs/tables),nitrofs)
# T3DS_DEFINES adds developer switches, e.g. -DT3DS_INPUT_SCRIPT (dev/input_script.h; build clean).
DEFINES := -DT3DS_VERSION=\"$(shell git describe --always --dirty 2>/dev/null || echo dev)\" $(T3DS_DEFINES)

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

# GNU make before 3.82 (macOS ships 3.81) takes the first matching pattern rule, BlocksDS's plain C
# rule, for *.arm.c files too and builds them as Thumb: give them ARM mode here (the later -marm
# wins over -mthumb). Newer makes pick BlocksDS's own *.arm.c rule; the flags then repeat.
$(BUILDDIR)/%.arm.c.o: CFLAGS += -marm -mlong-calls

# The ROM is packed again when a converted file changes, not only when the folder's entries do.
$(ROM): $(if $(NITROFSDIR),$(shell find $(NITROFSDIR) -type f))
