#!/bin/sh
# Configure helper for R packages that compile several SIMD translation units
# and select one at runtime. It checks compiler flag support, writes src/config.h,
# and substitutes src/Makevars.in into src/Makevars or src/Makevars.win.

set -eu

ROOT=${RSD_PACKAGE_ROOT:-$(pwd)}
MAKEVARS_IN=${RSD_MAKEVARS_IN:-src/Makevars.in}
MAKEVARS_OUT=${RSD_MAKEVARS_OUT:-src/Makevars}
CONFIG_OUT=${RSD_CONFIG_OUT:-src/config.h}

MAKEVARS_IN_PATH="$ROOT/$MAKEVARS_IN"
MAKEVARS_OUT_PATH="$ROOT/$MAKEVARS_OUT"
CONFIG_OUT_PATH="$ROOT/$CONFIG_OUT"

if [ ! -f "$MAKEVARS_IN_PATH" ]; then
    echo "ERROR: cannot find Makevars template: $MAKEVARS_IN_PATH" >&2
    exit 1
fi

if [ -z "${CC:-}" ]; then
    if [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/R" ]; then
        CC=$("${R_HOME}/bin/R" CMD config CC)
    else
        CC=cc
    fi
fi

if [ -z "${CFLAGS:-}" ] && [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/R" ]; then
    CFLAGS=$("${R_HOME}/bin/R" CMD config CFLAGS)
fi
if [ -z "${CPPFLAGS:-}" ] && [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/R" ]; then
    CPPFLAGS=$("${R_HOME}/bin/R" CMD config CPPFLAGS 2>/dev/null || true)
fi

RSD_LOG_PREFIX=${RSD_LOG_PREFIX:-RsimdDispatch}

if [ -z "${RSD_SIMDE_INCLUDE:-}" ]; then
    if [ -d "$ROOT/inst/include/simde" ]; then
        RSD_SIMDE_INCLUDE="-I$ROOT/inst/include"
    elif [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/Rscript" ]; then
        RSD_INC=$(
            "${R_HOME}/bin/Rscript" -e 'cat(system.file("include", package = "RsimdDispatch"))' 2>/dev/null || true
        )
        if [ -n "$RSD_INC" ] && [ -d "$RSD_INC/simde" ]; then
            RSD_SIMDE_INCLUDE="-I$RSD_INC"
        fi
    fi
fi
SIMDE_INCLUDE=${RSD_SIMDE_INCLUDE:-}

TMPDIR=${TMPDIR:-/tmp}
CONFDIR=$(mktemp -d "$TMPDIR/rsd-conf-XXXXXX")
trap 'rm -rf "$CONFDIR"' EXIT INT HUP TERM

check_cflag() {
    flags=$1
    header=${2:-}
    required_macros=${3:-}
    {
        if [ -n "$header" ]; then
            printf '#include <%s>\n' "$header"
        fi
        for macro in $required_macros; do
            printf '#ifndef %s\n#error "%s not defined"\n#endif\n' "$macro" "$macro"
        done
        printf 'int main(void) { return 0; }\n'
    } > "$CONFDIR/conftest.c"
    # shellcheck disable=SC2086
    ${CC} ${CPPFLAGS:-} ${CFLAGS:-} ${SIMDE_INCLUDE} ${flags} -c "$CONFDIR/conftest.c" -o "$CONFDIR/conftest.o" >/dev/null 2>&1
}

append_object() {
    if [ -z "$RSD_OBJECTS" ]; then
        RSD_OBJECTS=$1
    else
        RSD_OBJECTS="$RSD_OBJECTS $1"
    fi
}

RSD_OBJECTS=""
SSE2_CFLAGS=""
SSE41_CFLAGS=""
AVX2_CFLAGS=""
AVX512_CFLAGS=""
NEON_CFLAGS=""

HAVE_SSE2=0
HAVE_SSE41=0
HAVE_AVX2=0
HAVE_AVX512=0
HAVE_NEON=0

SIMDE_NATIVE_SSE2=0
SIMDE_NATIVE_SSE41=0
SIMDE_NATIVE_AVX2=0
SIMDE_NATIVE_AVX512=0
SIMDE_NATIVE_NEON=0

machine=$(${CC} -dumpmachine 2>/dev/null || true)
if [ -n "$machine" ]; then
    arch=$(printf '%s\n' "$machine" | sed 's/-.*//')
else
    arch=$(uname -m 2>/dev/null || echo unknown)
fi
case "$arch" in
    x86_64|amd64|i386|i686)
        if check_cflag "-msse2" "simde/x86/sse2.h" "SIMDE_X86_SSE2_NATIVE"; then
            append_object kernel_sse2.o
            SSE2_CFLAGS="-msse2"
            HAVE_SSE2=1
            SIMDE_NATIVE_SSE2=1
        fi
        if check_cflag "-msse4.1" "simde/x86/sse4.1.h" "SIMDE_X86_SSE4_1_NATIVE"; then
            append_object kernel_sse41.o
            SSE41_CFLAGS="-msse4.1"
            HAVE_SSE41=1
            SIMDE_NATIVE_SSE41=1
        fi
        if check_cflag "-mavx2" "simde/x86/avx2.h" "SIMDE_X86_AVX2_NATIVE"; then
            append_object kernel_avx2.o
            AVX2_CFLAGS="-mavx2"
            HAVE_AVX2=1
            SIMDE_NATIVE_AVX2=1
        fi
        if check_cflag "-mavx512f -mavx512bw -mavx512vl" "simde/x86/avx512.h" "SIMDE_X86_AVX512F_NATIVE SIMDE_X86_AVX512BW_NATIVE SIMDE_X86_AVX512VL_NATIVE"; then
            append_object kernel_avx512.o
            AVX512_CFLAGS="-mavx512f -mavx512bw -mavx512vl"
            HAVE_AVX512=1
            SIMDE_NATIVE_AVX512=1
        fi
        ;;
    aarch64|arm64)
        if check_cflag "" "simde/arm/neon.h" "SIMDE_ARM_NEON_A32V7_NATIVE"; then
            append_object kernel_neon.o
            HAVE_NEON=1
            SIMDE_NATIVE_NEON=1
        fi
        ;;
    armv7l|armv7*|arm*)
        if check_cflag "-mfpu=neon" "simde/arm/neon.h" "SIMDE_ARM_NEON_A32V7_NATIVE"; then
            append_object kernel_neon.o
            NEON_CFLAGS="-mfpu=neon"
            HAVE_NEON=1
            SIMDE_NATIVE_NEON=1
        fi
        ;;
esac

SIMDE_VERSION_FILE=""
if [ -f "$ROOT/inst/vendor/simde/VERSION" ]; then
    SIMDE_VERSION_FILE="$ROOT/inst/vendor/simde/VERSION"
elif [ -n "${R_HOME:-}" ] && [ -x "${R_HOME}/bin/Rscript" ]; then
    RSD_VERSION_FILE=$(
        "${R_HOME}/bin/Rscript" -e 'cat(system.file("vendor", "simde", "VERSION", package = "RsimdDispatch"))' 2>/dev/null || true
    )
    if [ -n "$RSD_VERSION_FILE" ] && [ -f "$RSD_VERSION_FILE" ]; then
        SIMDE_VERSION_FILE="$RSD_VERSION_FILE"
    fi
fi

SIMDE_VERSION="unknown"
SIMDE_COMMIT="unknown"
if [ -n "$SIMDE_VERSION_FILE" ]; then
    SIMDE_VERSION=$(sed -n 's/^Version: //p' "$SIMDE_VERSION_FILE" | head -n 1)
    SIMDE_COMMIT=$(sed -n 's/^Commit: //p' "$SIMDE_VERSION_FILE" | head -n 1)
fi
[ -n "$SIMDE_VERSION" ] || SIMDE_VERSION="unknown"
[ -n "$SIMDE_COMMIT" ] || SIMDE_COMMIT="unknown"

mkdir -p "$(dirname "$CONFIG_OUT_PATH")"
cat > "$CONFIG_OUT_PATH" <<EOF
#ifndef RSD_CONFIG_H
#define RSD_CONFIG_H

#define RSD_HAVE_SSE2 ${HAVE_SSE2}
#define RSD_HAVE_SSE41 ${HAVE_SSE41}
#define RSD_HAVE_AVX2 ${HAVE_AVX2}
#define RSD_HAVE_AVX512 ${HAVE_AVX512}
#define RSD_HAVE_NEON ${HAVE_NEON}
#define RSD_SIMDE_NATIVE_SSE2 ${SIMDE_NATIVE_SSE2}
#define RSD_SIMDE_NATIVE_SSE41 ${SIMDE_NATIVE_SSE41}
#define RSD_SIMDE_NATIVE_AVX2 ${SIMDE_NATIVE_AVX2}
#define RSD_SIMDE_NATIVE_AVX512 ${SIMDE_NATIVE_AVX512}
#define RSD_SIMDE_NATIVE_NEON ${SIMDE_NATIVE_NEON}
#define RSD_SIMDE_VERSION "${SIMDE_VERSION}"
#define RSD_SIMDE_COMMIT "${SIMDE_COMMIT}"

#endif
EOF

sed \
    -e "s|@RSD_OBJECTS@|${RSD_OBJECTS}|g" \
    -e "s|@RSD_SSE2_CFLAGS@|${SSE2_CFLAGS}|g" \
    -e "s|@RSD_SSE41_CFLAGS@|${SSE41_CFLAGS}|g" \
    -e "s|@RSD_AVX2_CFLAGS@|${AVX2_CFLAGS}|g" \
    -e "s|@RSD_AVX512_CFLAGS@|${AVX512_CFLAGS}|g" \
    -e "s|@RSD_NEON_CFLAGS@|${NEON_CFLAGS}|g" \
    "$MAKEVARS_IN_PATH" > "$MAKEVARS_OUT_PATH"

echo "${RSD_LOG_PREFIX} configure: objects='kernel_scalar.o ${RSD_OBJECTS}'"
echo "${RSD_LOG_PREFIX} configure: wrote ${CONFIG_OUT} and ${MAKEVARS_OUT}"
