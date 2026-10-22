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
| Toolchain | BlocksDS's own route (Wonderful toolchain) where it can be installed; elsewhere `tools/ds/setup_toolchain.sh`: an `arm-none-eabi-gcc` (the distribution's on Linux, devkitARM's on macOS, where it installs into `~/.local` without root), picolibc built from Wonderful's fork, BlocksDS built from source. `ds/Makefile` finds each. | The Wonderful hosts are not reachable from every machine (the cloud sessions among them); the fallback needs only a cross GCC, meson/ninja and GitHub. |
| Language | C (GNU C17, GCC 13 and later). No C++: the fallback toolchain's libstdc++ is built for newlib. | Small runtime, predictable code size and speed on a 67 MHz ARM9. |
| Simulation | `remake/core` ported to C module by module, integers only, same names and same step order. The port is written from the GDScript (which is written from the documented rules), not from the decompiled code. | One behaviour, two implementations, the same traces. |
| Verification | The C core also builds for the host (Linux/macOS `cc`) and replays the golden traces (`work/traces/`) field by field, like the remake's suites. ROM smoke tests run in DeSmuME (`tools/ds/screenshot.sh`). | Tests run in seconds on the host; the emulator proves the ROM boots and draws. |
| Assets | A DS converter (`tools/ds_import/`, D1) reads the remake's converted tree (`remake/imported/`) and, for raw data the remake bakes away (motion banks, spline tables), the disc through `tools/remake_import/common.Disc`; it writes DS formats into `ds/nitrofs/`, which the ROM build packs into NitroFS. Never committed. | The disc code stays in one place (`tools/remake_import/`); the DS step only re-encodes documented formats. |
| Original bugs | As in the remake: gameplay bugs reproduced, fixed together by the Gameplay-fixes option; `RuleSet` flags carried over. | Same game. |

## Hardware budget

Figures from the DS hardware documentation (GBATEK); the game's needs are to be measured in D1–D2.

| Resource | DS | Use |
|---|---|---|
| ARM9 | 67 MHz, 4 KB data cache, 32 KB ITCM / 16 KB DTCM | Simulation, geometry submission. Hot integer loops (pose, sampling, collision) go to ITCM. |
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
    core/          the C port of remake/core: no libnds, builds for the host too
      math/        fx (4.12, GTE MVMVA/GPF, MulMatrix), gte (ApplyMatrix(LV), VectorNormal, ratan2,
                   RTPS, division), c_rand
      fighter/     pose_tables, joint_frame, pose_solver, anim_stream (sampling at run time),
                   motion_bank (banks read in place, MotionSet), fighter_skeleton
    content/       loaders for the converted data (byte readers, tables/pose.bin, tables/spline.bin, rigs/)
    dev/           measurements on the DS (pose_bench)
    presentation/  skeleton_view (D1); 3D (fighters, stage, effects), 2D screens, HUD, sound
    platform/      files (NitroFS); pads, save data, timing
    stance_scene.c D1's fighter in its stance
  tests/           host tests of core and content (C)
  nitrofs/         converted data (git-ignored)
tools/ds/          setup_toolchain.sh, screenshot.sh, run_tests.sh, core_cases.py, data_cases.py
Tekken3-DS.command macOS: double-click to bring the ROM up to date and start it in melonDS
tools/ds_import/   the DS converter: remake/imported and the disc → ds/nitrofs
```

Layers and dependencies as in the remake: `core` → nothing but C; `content` → `core`; `presentation` → both; `platform` and `main.c` wire them. Output leaves the simulation only as per-frame events.

Code conventions:

- Names follow the GDScript: `PoseSolver.build` is `pose_solver_build`, `Fx.mul_matrix` is `fx_mul_matrix`; matrices and vectors are the value types `Mat3` and `Vec3` (`int32_t`).
- The GDScript computes in 64-bit ints and wraps or saturates where the original does. The C keeps that split: sums of GTE products are `int64_t`; wrapped C arithmetic is 32-bit modular (`fx_mul32`, unsigned casts), never signed overflow (the host tests run with UBSan).
- Hot core files are named `*.itcm.arm.c`: ARM mode, where 64-bit products are single `smull` instructions (Thumb calls a library routine), placed in ITCM. GNU make 3.81 (macOS) would build `*.arm.c` as Thumb with BlocksDS's own rules; `ds/Makefile` adds the ARM flags itself.

Input: the game's pad word (PsyQ layout, `remake/platform/pad_state.gd`) from the DS keys, face buttons in the PlayStation's places (Y □, X △, B ✕, A ○), L/R as L1/R1, L2/R2 on the touch screen (`ds/source/pad.c`).

## Milestones

### D0: Bootstrap

**Status: done (2026-10-22).** `make nds` builds `ds/tekken3.nds`; it boots in DeSmuME, draws a 3D test picture (checkered floor, turning cube; state in game units) on the top screen and a status console on the bottom one (frame, pad word, CPU use). Keys map to the pad word as above, checked with held keys in `tools/ds/screenshot.sh`. Open: a run on real hardware or melonDS.

- `tools/ds/setup_toolchain.sh` (BlocksDS v1.16.0, picolibc, compiler wrappers) and root `make nds`, `make run-nds`, `make nds-screenshot`, `make clean-nds`.

### D1: One fighter

**Status: in progress (2026-10-22).** Done:

- `core/math` (fx, gte, c_rand) and the pose pipeline (`PoseTables`, `JointFrame`, `PoseSolver` with the Gameplay-fix flag `wide_elbow`) in C.
- Host tests (`make test-ds`, seconds): the math helpers as in `test_fx.gd`, and `PoseSolver` against the verified Python port (`tools/research/pose.py`) on 2,400 frames of random poses with synthetic tables of the game's shape (`tools/ds/core_cases.py`), slots carried over as in the traces. Branches covered: all but the bend clamps (cos above ±1), which reachable lengths do not produce. The comparison catches single-constant changes (checked by mutating the solver).
- Motion sampling at run time (`anim_stream`, `motion_bank`): the DS keeps the banks' compressed streams instead of the remake's baked poses (98 bytes per frame would not fit 4 MB). Tested against `tools/research/motion.py` (bit-exact with AnimDecodePose) on 300 random streams, 9,522 frames, with random spline tables; slot lookup through the common bank on a bank built in the test. On the converted game, every stream of the 22 banks sampled from `ds/nitrofs/motion/` with the converted spline tables equals the remake's baked poses (`remake/imported/motion/*.poses.bin.gz`): 321,269 frames.
- `FighterSkeleton` (pose → root → 22 joints; attachments rigid, their dynamics TODO) against `pose.py` and `fight_sim.root_rotation` with a random rig, 400 frames on one skeleton; on the converted game with three costumes' rigs (scales 45 %, 100 %, 106 %), the converted pose tables and 3,000 baked frames each, the rig files (`rigs/`) equal to the cases' rigs. The data cases come from `tools/ds/data_cases.py` (under `ds/build/`, game data), made by `make test-ds` when the game is converted.
- `PoseSolver` passes the pose traces bit for bit with the converted tables (`pose_divmot00`, `pose_divmot99`: 393 animations, 21,278 frames; converted from the USA disc). The traces come from `export_traces.py`, from the `work/` tree or, with `--image` (`make traces-ds DS_IMAGE=…`), from one disc of any supported release rebased onto Japan Rev.1's layout; without them the test is skipped.
- `tools/ds_import/` (`make convert-ds DS_IMAGE=…`, unittests with a stub disc in `make test-ds`) writes `tables/pose.bin` from `remake/imported/tables/pose.json`, `tables/characters.bin` (per costume key the costume slot, the bank type and the name, from `fight.json` and `fighter.json`), each costume's rig `rigs/costume_NN.bin` from its `model.json` (joint offsets x, y, −z, parents, scale), and from the disc the 22 motion banks as they are (`motion/divmotNN.bin`, 6.2 MB in NitroFS; a fight holds two own banks and the common one) and the spline tables (`tables/spline.bin`, EXE 0x80096B00…0x80097160).
- The fighter in its stance on the DS (`stance_scene.c`): the ROM packs `ds/nitrofs/` (NitroFS, 6.5 MB) and loads the pose and spline tables, the character table, the common bank and, per costume key, the own bank and the rig; slot 3's animation is sampled each frame and posed by `FighterSkeleton`, drawn as a wire skeleton (`presentation/skeleton_view.c`) over a floor at 60 Hz. Up/Down change the costume key, A/B the move slot, Left/Right turn the camera, Start goes back to the stance; the bottom screen logs each command with its frame. A key loads whole into a spare fighter slot (bank, rig, model, textures; texture banks A, B and D hold both for the switch) and shows only when all of it loaded, so a failing key leaves the fighter as it was. Checked in melonDS 1.1 with Paul (key 0).
- The model (`tools/ds_import/models.py` → `models/costume_NN.bin`, all 52 costumes, 85–100 KB each): the PlayStation's faces as they are (triangles and quads, one polygon each: 714–841 per costume, so two fighters fit the 2048 with about 360 to spare), each corner one part-local vertex or the 0.5/0.5 blend of two (the seams), from the same drawn rows and seam exchange as the remake's converter. Each material is its own texture, 4 or 8 bits per texel as on the PlayStation, the rectangle of its page its faces use with its CLUT as palette; the CLUT's transparent colours move to index 0, the only one the DS shows transparent. Textures take 46–138 KB of VRAM per costume (banks A and B for textures, E for palettes). On the DS (`presentation/fighter_view`) the vertices are posed on the CPU from the joint frames (`fighter_mesh`), lit by one directional light per normal, and sent as polygons; Select switches to the wire skeleton. Checked in melonDS with Paul, Law, Xiaoyu, Tiger, Gon and True Ogre. Host tests: the model file's checks, and the posing (corners with their seams, light levels) of Paul's converted model against `data_cases.py` for 60 frames.
- Input: `pad.c` maps keysHeld()'s bits (its own names, checked against libnds's at compile time) to the game's pad word and keeps its presses and releases (`PadState`); `stance_input.c` turns a frame's pad into the scene's commands, opposite buttons cancelling out, touch and lid ignored. Host tests cover every key, the edges (one command per press, however long held) and the commands. On the DS a developer build (`T3DS_DEFINES=-DT3DS_INPUT_SCRIPT`, `dev/input_script.c`) plays a fixed key sequence (thirteen costume changes, slots, views, a held turn, both directions at once) and ends in the state it predicts; checked in melonDS. Keys that seemed to press themselves in melonDS were the Mac's keyboard reaching the emulator, which takes the focus when it starts: started without focus (`open -g`) it saw no key in 1,000 frames.
- CPU time in melonDS 1.1, ARM mode in ITCM: sampling a pose 0.32–0.45 ms, the skeleton (pose solver included) 0.35 ms, about 0.8 ms per fighter, 5 % of a frame; the pose solver alone 0.19 ms (synthetic bench). Built as Thumb (the make 3.81 slip) the same took 2.0 ms. DeSmuME had shown 0.28 ms for the pose solver; real hardware to be measured.

Open (needs a converted disc: Japan Rev.1, Japan or USA; the European release is not supported):

- Drawing cost: 5.6 ms per fighter in melonDS (posing and lighting about 2 ms on the CPU, sending 2,600 corners about 3.5 ms), a third of a frame: too much for two fighters and a stage. Next: let the geometry engine transform the polygons whose corners all hang on one joint (its matrix loaded, part-local vertices, hardware lighting) and pose only the seams on the CPU; send the commands by DMA from a list; measure on hardware.
- The model's remaining parts: the hands' vertex variants (they keep the base block's shape), the blink and Gon's palette swap, the smooth normals' halves.
- Costume attachments' dynamics (`AttachmentDynamics`).
- Sampling cost: 49 channels per frame take longer than the whole skeleton; to look at before two fighters run (spline tables in DTCM, a cached segment per channel).

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
- **CPU time.** Sampling and posing take about 0.8 ms per fighter in melonDS (D1), 10 % of a frame for two; the fight logic is still to measure, on hardware too.
- **Movies.** Full-motion video is likely out of reach; at most low-resolution clips.
