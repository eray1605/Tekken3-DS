# Tekken 3 DS: port plan

The Nintendo DS build of the game: the remake's simulation ported to C, drawn and played on the DS's own hardware. This plan fixes its goals, decisions, architecture and milestones. The game's behaviour stays specified by [`research/`](research/README.md) and implemented first in the Godot remake ([remake-plan.md](remake-plan.md)); the DS port follows the remake and does not restate its rules.

Date of the decisions: 2026-10-22.

## Goals and non-goals

**Goals**

- **The same game logic as the remake.** Every frame of a fight and of the game flow matches the original, proven by the same golden traces.
- **Runs on a real DS** (and DS Lite, DSi in DS mode) at 60 Hz, and in emulators (melonDS, DeSmuME).
- **No game data in the repository.** The ROM's data is converted from the owner's disc, like the remake's. A ROM built with it is private.

**Non-goals**

- Compiling the decompiled code. Ghidra's C output (`work/decomp/`) is a reading aid for PlayStation MIPS code: it cannot be built, and it addresses hardware (GTE, GPU, SPU, CD) the DS does not have.
- Emulating the PlayStation.
- Matching the remake's modernised graphics. The DS draws with its own limits (see [Hardware budget](#hardware-budget)).

## Key decisions

| Area | Decision | Reason |
|---|---|---|
| SDK | [BlocksDS](https://blocksds.skylyrac.net) (libnds, maxmod, NitroFS), pinned to v1.16.0. ARM9-only ROM layout with BlocksDS's default ARM7 core. | Open source (zlib/MIT), maintained, standard GCC. Pinning keeps the toolchain reproducible. |
| Toolchain | BlocksDS's own route (Wonderful toolchain) where it can be installed; elsewhere `tools/ds/setup_toolchain.sh`: the distribution's `arm-none-eabi-gcc`, picolibc built from Wonderful's fork, BlocksDS built from source. `ds/Makefile` finds either. | The Wonderful hosts are not reachable from every machine (the cloud sessions among them); the fallback needs only apt and GitHub. |
| Language | C (GNU C17, GCC 13 and later). No C++: the fallback toolchain's libstdc++ is built for newlib. | Small runtime, predictable code size and speed on a 67 MHz ARM9. |
| Simulation | `remake/core` ported to C module by module, integers only, same names and same step order. The port is written from the GDScript (which is written from the documented rules), not from the decompiled code. | One behaviour, two implementations, the same traces. |
| Verification | The C core also builds for the host (Linux/macOS `cc`) and replays the golden traces (`work/traces/`) field by field, like the remake's suites. ROM smoke tests run in DeSmuME (`tools/ds/screenshot.sh`). | Tests run in seconds on the host; the emulator proves the ROM boots and draws. |
| Assets | A DS converter (`tools/ds_import/`, D1) reads the remake's converted tree (`remake/imported/`) and writes DS formats into `ds/nitrofs/`, which the ROM build packs into NitroFS. Never committed. | The disc code stays in one place (`tools/remake_import/`); the DS step only re-encodes documented formats. |
| Original bugs | As in the remake: gameplay bugs reproduced, fixed together by the Gameplay-fixes option; `RuleSet` flags carried over. | Same game. |

## Hardware budget

Figures from the DS hardware documentation (GBATEK); the game's needs are to be measured in D1–D2.

| Resource | DS | Use |
|---|---|---|
| ARM9 | 67 MHz, 4 KB data cache, 32 KB ITCM / 16 KB DTCM | Simulation, geometry submission. Hot integer loops (pose, collision) go to ITCM. |
| Main RAM | 4 MB | Code, simulation state, the two fighters' motion banks, stage. Banks are loaded per fight from NitroFS. |
| Texture VRAM | up to 512 KB (banks A–D) + palettes | Fighters and stage. PlayStation textures are 4/8 bpp paletted, which the DS draws natively. |
| 3D engine | 2048 polygons / 6144 vertices per frame, 4.12 vertices, 20.12 matrices, a 31-slot matrix stack | Two fighters, stage, effects. One matrix per joint (18) fits the stack; seam vertices that blend two parts are transformed on the CPU. |
| Screens | two 256 × 192 | Fight on the top screen; HUD, menus and touch buttons (L2/R2 macros) on the bottom one. |
| Sound | 16 hardware channels, PCM8/PCM16/IMA-ADPCM, ARM7 mixing | SPU samples re-encoded to IMA-ADPCM; music streamed. |

The PlayStation's own numbers fit this frame: 4096 angle units per turn and 4.12 fixed point (the DS's 3D engine uses 4.12 vertices and 20.12 matrices; libnds angles are 32768 per turn, `game << 3`).

## Architecture

```text
ds/
  Makefile         BlocksDS rom_arm9 build: tekken3.nds
  source/          ARM9 code
    core/          (D1+) the C port of remake/core: no libnds, builds for the host too
    content/       (D1+) loaders for NitroFS data
    presentation/  (D1+) 3D (fighters, stage, effects), 2D screens, HUD, sound
    platform/      (D1+) pads, save data, timing
  nitrofs/         converted data (git-ignored)
tools/ds/          setup_toolchain.sh, screenshot.sh, (D1+) host test runner
tools/ds_import/   (D1) the DS converter
```

Layers and dependencies as in the remake: `core` → nothing but C; `content` → `core`; `presentation` → both; `platform` and `main.c` wire them. Output leaves the simulation only as per-frame events.

Input: the game's pad word (PsyQ layout, `remake/platform/pad_state.gd`) from the DS keys, face buttons in the PlayStation's places (Y □, X △, B ✕, A ○), L/R as L1/R1, L2/R2 on the touch screen (`ds/source/pad.c`).

## Milestones

### D0: Bootstrap

**Status: done (2026-10-22).** `make nds` builds `ds/tekken3.nds`; it boots in DeSmuME, draws a 3D test picture (checkered floor, turning cube; state in game units) on the top screen and a status console on the bottom one (frame, pad word, CPU use). Keys map to the pad word as above, checked with held keys in `tools/ds/screenshot.sh`. Open: a run on real hardware or melonDS.

- `tools/ds/setup_toolchain.sh` (BlocksDS v1.16.0, picolibc, compiler wrappers) and root `make nds`, `make run-nds`, `make nds-screenshot`, `make clean-nds`.

### D1: One fighter

- `tools/ds_import/`: one character's model (parts, seams, textures as 4/8 bpp) and motion banks from `remake/imported/` into `ds/nitrofs/`; size report against the RAM and VRAM budgets.
- C port of `core/math` and `PoseSolver`; host build passes the pose traces (`divmot00`, `divmot99`) bit for bit.
- The fighter in its stance on the DS, animated by the C core, at 60 Hz with its CPU time shown.

### D2: The fight

- C port of the fight simulation (`core/fight`, `core/fighter`, `core/combat`, `core/camera`, `core/effects`); host build passes the fight traces.
- Stage floor and backdrop, HUD on the bottom screen, two-player input (one DS: player 2 by CPU from D3; local wireless later).

### D3: Sound and CPU

- SPU samples and music converted; sound events played through maxmod or an own ARM7 mixer.
- C port of `core/ai`; the CPU opponent passes its traces.

### D4: The game

- `GameFlow` and the screens (title, menus, select, VS, results, ranking), laid out anew for 256 × 192 × 2 from the game's pictures and fonts; save data on the cartridge/SD.

### D5: Modes and hardware

- Tekken Force, Tekken Ball, Theater (movies as far as space allows); performance and memory on real hardware.

## Risks and open questions

- **Polygon and vertex budget.** Two fighters plus stage and effects must stay under 2048 polygons; unknown until D1 counts the models. Fallback: simpler stage backdrops (2D layers on the top screen).
- **RAM.** Motion banks and the game flow's tables must fit 4 MB with code; banks may have to be streamed per move set.
- **CPU time.** The pose pipeline (sampling, FK, IK) for two fighters at 60 Hz on 67 MHz: to measure in D1. ITCM and Thumb/ARM choice per module.
- **Movies.** Full-motion video is likely out of reach; at most low-resolution clips.
