#!/bin/sh
# Sets up the DS toolchain from packages and sources, for machines where BlocksDS's own route (the
# Wonderful toolchain and its packages, https://blocksds.skylyrac.net/docs/setup/) is not reachable
# or not installed.
#   tools/ds/setup_toolchain.sh
# Debian/Ubuntu (apt, run as root or with sudo rights): BlocksDS v1.16.0 in /opt/blocksds/core, and in
# /opt/t3ds-toolchain the distribution's arm-none-eabi GCC wrapped around a picolibc build (BlocksDS
# needs picolibc; the package has newlib). Sources go to $T3DS_SRC (default /opt/src).
# macOS (no root needed): the same in ~/.local (blocksds/core, t3ds-toolchain, src) around
# devkitARM's GCC ($T3DS_GCC_BIN, default /opt/devkitpro/devkitARM/bin); meson and ninja must be on
# the PATH (e.g. pip install meson ninja). ds/Makefile finds both places by default.
set -eu

if [ "$(uname -s)" = Darwin ]; then
    PREFIX=${T3DS_TOOLCHAIN:-$HOME/.local/t3ds-toolchain}
    BLOCKSDS_DIR=${BLOCKSDS:-$HOME/.local/blocksds/core}
    SRC=${T3DS_SRC:-$HOME/.local/src}
    GCC_BIN=${T3DS_GCC_BIN:-/opt/devkitpro/devkitARM/bin}
    JOBS=$(sysctl -n hw.ncpu)
else
    PREFIX=${T3DS_TOOLCHAIN:-/opt/t3ds-toolchain}
    BLOCKSDS_DIR=${BLOCKSDS:-/opt/blocksds/core}
    SRC=${T3DS_SRC:-/opt/src}
    GCC_BIN=${T3DS_GCC_BIN:-/usr/bin}
    JOBS=$(nproc)
fi
PATH="$GCC_BIN:$PATH"
BLOCKSDS_TAG=v1.16.0
# The Wonderful toolchain's picolibc fork, the one BlocksDS v1.16 was made with.
PICOLIBC_REPO=https://github.com/WonderfulToolchain/wf-picolibc.git
PICOLIBC_COMMIT=75b079dd10e90b3c7e7426e34f0fb5f14bda2e56

SUDO=
[ "$(id -u)" -eq 0 ] || [ "$(uname -s)" = Darwin ] || SUDO=sudo

log() { printf '== %s\n' "$*"; }

if [ "$(uname -s)" != Darwin ]; then
    log "packages"
    $SUDO apt-get update -q
    DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y -q \
        build-essential git meson ninja-build gcc-arm-none-eabi binutils-arm-none-eabi \
        desmume xvfb xdotool imagemagick
fi
for tool in meson ninja arm-none-eabi-gcc; do
    command -v "$tool" >/dev/null || { log "$tool is missing"; exit 1; }
done

$SUDO mkdir -p "$SRC" "$PREFIX/bin" "$BLOCKSDS_DIR" "$(dirname "$BLOCKSDS_DIR")/external"
$SUDO chown "$(id -u):$(id -g)" "$SRC" "$PREFIX" "$PREFIX/bin" "$BLOCKSDS_DIR" "$(dirname "$BLOCKSDS_DIR")/external"

log "picolibc"
if [ ! -d "$SRC/wf-picolibc/.git" ]; then
    git clone -q "$PICOLIBC_REPO" "$SRC/wf-picolibc"
fi
git -C "$SRC/wf-picolibc" fetch -q origin "$PICOLIBC_COMMIT" || true
git -C "$SRC/wf-picolibc" checkout -q "$PICOLIBC_COMMIT"
BUILD="$SRC/picolibc-build"
rm -rf "$BUILD" "$PREFIX/picolibc"
mkdir -p "$BUILD"
cat > "$BUILD/cross.txt" <<'EOF'
[binaries]
c = ['arm-none-eabi-gcc', '-nostdlib']
ar = 'arm-none-eabi-gcc-ar'
as = 'arm-none-eabi-as'
nm = 'arm-none-eabi-nm'
strip = 'arm-none-eabi-strip'
[host_machine]
system = 'none'
cpu_family = 'arm'
cpu = 'arm'
endian = 'little'
[properties]
skip_sanity_check = true
EOF
# The options of Wonderful's package (toolchain-gcc-arm-none-eabi-picolibc-generic), one library for
# ARMv4T: it runs on the ARM9 (ARMv5TE) and the ARM7 alike.
meson setup "$BUILD" "$SRC/wf-picolibc" --cross-file="$BUILD/cross.txt" \
    -Dmultilib=false -Dpicocrt=false -Dpicolib=false -Dsemihost=false -Dspecsdir=none -Dtests=false \
    -Dthread-local-storage=true -Dfast-bufio=true -Dio-long-long=true -Dio-pos-args=true \
    -Dio-percent-b=true -Dposix-console=true -Dformat-default=double -Dnewlib-nano-malloc=false \
    -Dprefix="$PREFIX/picolibc" -Dlibdir=lib -Dincludedir=include \
    -Dc_args="-march=armv4t -mfloat-abi=soft -O2" >/dev/null
ninja -C "$BUILD" >/dev/null
ninja -C "$BUILD" install >/dev/null
rm -f "$PREFIX"/picolibc/lib/picolibc*.ld
# Wonderful's picolibc.specs: local-exec TLS and the printf variants.
cat > "$PREFIX/picolibc/lib/picolibc.specs" <<'EOF'
%rename link	picolibc_link
%rename cc1	picolibc_cc1
%rename cc1plus	picolibc_cc1plus

*cc1:
%{!ftls-model:-ftls-model=local-exec} %(picolibc_cc1)

*cc1plus:
%{!ftls-model:-ftls-model=local-exec} %(picolibc_cc1plus)

*link:
%{DPICOLIBC_DOUBLE_PRINTF_SCANF:--defsym=vfprintf=__d_vfprintf} %{DPICOLIBC_DOUBLE_PRINTF_SCANF:--defsym=vfscanf=__d_vfscanf} %{DPICOLIBC_FLOAT_PRINTF_SCANF:--defsym=vfprintf=__f_vfprintf} %{DPICOLIBC_FLOAT_PRINTF_SCANF:--defsym=vfscanf=__f_vfscanf} %{DPICOLIBC_LONG_LONG_PRINTF_SCANF:--defsym=vfprintf=__l_vfprintf} %{DPICOLIBC_LONG_LONG_PRINTF_SCANF:--defsym=vfscanf=__l_vfscanf} %{DPICOLIBC_INTEGER_PRINTF_SCANF:--defsym=vfprintf=__i_vfprintf} %{DPICOLIBC_INTEGER_PRINTF_SCANF:--defsym=vfscanf=__i_vfscanf} %{DPICOLIBC_MINIMAL_PRINTF_SCANF:--defsym=vfprintf=__m_vfprintf} %{DPICOLIBC_MINIMAL_PRINTF_SCANF:--defsym=vfscanf=__m_vfscanf} %(picolibc_link)

*lib:
--start-group %(libgcc) -lc --end-group
EOF
# GCC 14's libgcc has __sync_synchronize_none (BlocksDS links memory barriers to it); older ones do not.
cat > "$BUILD/sync_none.s" <<'EOF'
	.arm
	.global __sync_synchronize_none
	.type __sync_synchronize_none, %function
__sync_synchronize_none:
	bx lr
EOF
arm-none-eabi-as -march=armv4t -o "$BUILD/sync_none.o" "$BUILD/sync_none.s"
arm-none-eabi-ar rcs "$PREFIX/picolibc/lib/libc.a" "$BUILD/sync_none.o"

log "compiler wrappers"
for tool in as ld ar nm objcopy objdump strip ranlib readelf size gcc-ar gcc-nm gcc-ranlib addr2line strings; do
    ln -sf "$GCC_BIN/arm-none-eabi-$tool" "$PREFIX/bin/arm-none-eabi-$tool"
done
for tool in gcc g++ cpp; do
    cat > "$PREFIX/bin/arm-none-eabi-$tool" <<EOF
#!/bin/sh
# $GCC_BIN's arm-none-eabi-$tool with picolibc in front of its newlib; C23 spelled the way
# GCC 13 knows it.
for a in "\$@"; do
    shift
    case "\$a" in
        -std=gnu23) a=-std=gnu2x ;;
        -std=c23) a=-std=c2x ;;
    esac
    set -- "\$@" "\$a"
done
exec $GCC_BIN/arm-none-eabi-$tool -B$PREFIX/picolibc/lib/ -isystem $PREFIX/picolibc/include -L$PREFIX/picolibc/lib "\$@"
EOF
    chmod +x "$PREFIX/bin/arm-none-eabi-$tool"
done
ln -sf arm-none-eabi-gcc "$PREFIX/bin/arm-none-eabi-cc"

log "BlocksDS $BLOCKSDS_TAG"
if [ ! -d "$SRC/blocksds/.git" ]; then
    git clone -q --branch "$BLOCKSDS_TAG" --recurse-submodules --depth 1 --shallow-submodules \
        https://github.com/blocksds/sdk.git "$SRC/blocksds"
fi
cd "$SRC/blocksds"
# macOS has no <malloc.h>; dldipatch needs only the standard allocator.
sed -i.orig 's/#include <malloc.h>/#include <stdlib.h>/' tools/dldipatch/dldipatch.c
export BLOCKSDS="$PWD" ARM_NONE_EABI_PATH="$PREFIX/bin/"
# mmutil (maxmod's converter) needs a newer host compiler on some systems: not needed yet, not fatal.
make -k -j"$JOBS" -C tools VERSION_STRING="$BLOCKSDS_TAG-t3ds" >/dev/null 2>&1 || log "some host tools failed (see make -C tools)"
make -j"$JOBS" -C libs VERSION_STRING="$BLOCKSDS_TAG-t3ds" >/dev/null
make -j"$JOBS" -C sys >/dev/null
rm -rf "${BLOCKSDS_DIR:?}"/*
make -C libs install INSTALLDIR="$BLOCKSDS_DIR/libs" >/dev/null
make -C sys install INSTALLDIR="$BLOCKSDS_DIR/sys" >/dev/null
make -k -C tools install INSTALLDIR="$BLOCKSDS_DIR/tools" >/dev/null 2>&1 || true
cp -r sys/cmake "$BLOCKSDS_DIR/cmake"
cp -r licenses "$BLOCKSDS_DIR"
echo "$BLOCKSDS_TAG-t3ds" > "$BLOCKSDS_DIR/version.txt"

log "done: make nds"
