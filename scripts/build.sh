#!/usr/bin/env bash

set -Eeuo pipefail


# OpenTTD Build Helper
# Default: Debug
# ./build.sh

# Release
# ./build.sh release

# Optimized + debug symbols
# ./build.sh relwithdebinfo

# Size optimized
# ./build.sh minsizerel

# Completely clean debug build
# ./build.sh debug --clean

# Debug with Clang
# ./build.sh debug --compiler clang

# Address sanitizer
# ./build.sh debug --asan

# Address + UB sanitizer
# ./build.sh debug --asan --ubsan

# 16 parallel jobs
# ./build.sh release --jobs 16

# Dedicated server
# ./build.sh release --dedicated

# Build only OpenTTD's tools
# ./build.sh debug --tools-only

# Configure without compiling
# ./build.sh debug --configure-only

# Build an already configured tree
# ./build.sh debug --build-only

# Build and launch
# ./build.sh release --run


SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_DIR"

BUILD_TYPE="Debug"
BUILD_DIR="build/debug"

GENERATOR="Ninja"
COMPILER=""
JOBS="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

CLEAN=0
RUN=0
CONFIGURE_ONLY=0
BUILD_ONLY=0

ASAN=0
UBSAN=0
TSAN=0

DEDICATED=0
TOOLS_ONLY=0
ASSERTS=""

EXTRA_CXXFLAGS=""
EXTRA_CFLAGS=""

CMAKE_EXTRA_ARGS=()

if [[ -t 1 ]]; then
    RED=$'\033[31m'
    GREEN=$'\033[32m'
    YELLOW=$'\033[33m'
    BLUE=$'\033[34m'
    CYAN=$'\033[36m'
    BOLD=$'\033[1m'
    RESET=$'\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    CYAN=''
    BOLD=''
    RESET=''
fi


die() {
    echo -e "${RED}error:${RESET} $*" >&2
    exit 1
}

info() {
    echo -e "${BLUE}==>${RESET} $*"
}

success() {
    echo -e "${GREEN}==>${RESET} $*"
}

warning() {
    echo -e "${YELLOW}warning:${RESET} $*" >&2
}

has_command() {
    command -v "$1" >/dev/null 2>&1
}

usage() {
    cat <<EOF

${BOLD}OpenTTD Build Helper${RESET}

${BOLD}Usage:${RESET}
    ./build.sh [configuration] [options]

${BOLD}Configurations:${RESET}
    debug
        Debug build with assertions enabled.

    release
        Optimized release build.

    relwithdebinfo
        Optimized build with debug information.

    minsizerel
        Optimized build for smaller binaries.

${BOLD}Build options:${RESET}
    --clean
        Remove the build directory before configuring.

    --jobs N
        Build using N parallel jobs.
        Default: ${JOBS}

    --generator NAME
        Select a CMake generator.
        Default: Ninja

    --compiler gcc
        Use GCC.

    --compiler clang
        Use Clang.

    --configure-only
        Configure CMake but do not build.

    --build-only
        Build an existing configuration without re-running CMake.

    --run
        Run OpenTTD after a successful build.

${BOLD}OpenTTD options:${RESET}
    --dedicated
        Build the dedicated-server version.

    --tools-only
        Build only OpenTTD's tools.

    --asserts
        Enable assertions.

    --no-asserts
        Disable assertions.

${BOLD}Sanitizers:${RESET}
    --asan
        Enable AddressSanitizer.

    --ubsan
        Enable UndefinedBehaviorSanitizer.

    --tsan
        Enable ThreadSanitizer.

${BOLD}Compiler flags:${RESET}
    --cxxflags FLAGS
        Add extra C++ compiler flags.

    --cflags FLAGS
        Add extra C compiler flags.

${BOLD}Examples:${RESET}
    ./build.sh
    ./build.sh debug
    ./build.sh release

    ./build.sh debug --clean

    ./build.sh debug --asan
    ./build.sh debug --ubsan
    ./build.sh debug --asan --ubsan

    ./build.sh debug --compiler clang

    ./build.sh relwithdebinfo --jobs 16

    ./build.sh debug --dedicated
    ./build.sh debug --tools-only

    ./build.sh debug --run

EOF
}

CONFIGURATION="debug"

while [[ $# -gt 0 ]]; do
    case "$1" in

        debug)
            CONFIGURATION="debug"
            shift
            ;;

        release)
            CONFIGURATION="release"
            shift
            ;;

        relwithdebinfo)
            CONFIGURATION="relwithdebinfo"
            shift
            ;;

        minsizerel)
            CONFIGURATION="minsizerel"
            shift
            ;;

        --clean)
            CLEAN=1
            shift
            ;;

        --run)
            RUN=1
            shift
            ;;

        --configure-only)
            CONFIGURE_ONLY=1
            shift
            ;;

        --build-only)
            BUILD_ONLY=1
            shift
            ;;

        --dedicated)
            DEDICATED=1
            shift
            ;;

        --tools-only)
            TOOLS_ONLY=1
            shift
            ;;

        --asserts)
            ASSERTS="ON"
            shift
            ;;

        --no-asserts)
            ASSERTS="OFF"
            shift
            ;;

        --asan)
            ASAN=1
            shift
            ;;

        --ubsan)
            UBSAN=1
            shift
            ;;

        --tsan)
            TSAN=1
            shift
            ;;

        --jobs|-j)
            [[ $# -ge 2 ]] || die "$1 requires a value"
            JOBS="$2"
            shift 2
            ;;

        --generator|-G)
            [[ $# -ge 2 ]] || die "$1 requires a value"
            GENERATOR="$2"
            shift 2
            ;;

        --compiler)
            [[ $# -ge 2 ]] || die "$1 requires gcc or clang"
            COMPILER="$2"
            shift 2
            ;;

        --cxxflags)
            [[ $# -ge 2 ]] || die "$1 requires a value"
            EXTRA_CXXFLAGS="$2"
            shift 2
            ;;

        --cflags)
            [[ $# -ge 2 ]] || die "$1 requires a value"
            EXTRA_CFLAGS="$2"
            shift 2
            ;;

        --help|-h)
            usage
            exit 0
            ;;

        --)
            shift

            while [[ $# -gt 0 ]]; do
                CMAKE_EXTRA_ARGS+=("$1")
                shift
            done

            ;;

        -*)
            die "Unknown option: $1"

            ;;

        *)
            die "Unknown argument: $1"
            ;;

    esac
done


case "$CONFIGURATION" in
    debug)
        BUILD_TYPE="Debug"
        BUILD_DIR="build/debug"
        ;;

    release)
        BUILD_TYPE="Release"
        BUILD_DIR="build/release"
        ;;

    relwithdebinfo)
        BUILD_TYPE="RelWithDebInfo"
        BUILD_DIR="build/relwithdebinfo"
        ;;

    minsizerel)
        BUILD_TYPE="MinSizeRel"
        BUILD_DIR="build/minsizerel"
        ;;

    *)
        die "Invalid configuration: $CONFIGURATION"
        ;;
esac

has_command cmake || die "CMake was not found."

if [[ "$GENERATOR" == "Ninja" ]]; then
    has_command ninja || die "Ninja was not found. Install ninja-build."
fi

if [[ "$COMPILER" == "gcc" ]]; then
    has_command gcc || die "gcc was not found."
    has_command g++ || die "g++ was not found."
fi

if [[ "$COMPILER" == "clang" ]]; then
    has_command clang || die "clang was not found."
    has_command clang++ || die "clang++ was not found."
fi


if [[ -n "$COMPILER" ]]; then
    case "$COMPILER" in
        gcc)
            export CC="${CC:-gcc}"
            export CXX="${CXX:-g++}"
            ;;

        clang)
            export CC="${CC:-clang}"
            export CXX="${CXX:-clang++}"
            ;;

        *)
            die "Unknown compiler '$COMPILER'. Use gcc or clang."
            ;;
    esac
fi


if [[ -n "$EXTRA_CXXFLAGS" ]]; then
    export CXXFLAGS="${CXXFLAGS:-} ${EXTRA_CXXFLAGS}"
fi

if [[ -n "$EXTRA_CFLAGS" ]]; then
    export CFLAGS="${CFLAGS:-} ${EXTRA_CFLAGS}"
fi


SANITIZERS=()

if [[ "$ASAN" -eq 1 ]]; then
    SANITIZERS+=("address")
fi

if [[ "$UBSAN" -eq 1 ]]; then
    SANITIZERS+=("undefined")
fi

if [[ "$TSAN" -eq 1 ]]; then
    SANITIZERS+=("thread")
fi

if [[ "${#SANITIZERS[@]}" -gt 0 ]]; then
    SANITIZER_LIST="$(IFS=,; echo "${SANITIZERS[*]}")"

    CMAKE_EXTRA_ARGS+=(
        "-DCMAKE_C_FLAGS=-fsanitize=${SANITIZER_LIST}"
        "-DCMAKE_CXX_FLAGS=-fsanitize=${SANITIZER_LIST}"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=${SANITIZER_LIST}"
        "-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=${SANITIZER_LIST}"
    )
fi


if [[ "$DEDICATED" -eq 1 ]]; then
    CMAKE_EXTRA_ARGS+=(
        "-DOPTION_DEDICATED=ON"
    )
fi

if [[ "$TOOLS_ONLY" -eq 1 ]]; then
    CMAKE_EXTRA_ARGS+=(
        "-DOPTION_TOOLS_ONLY=ON"
    )
fi

if [[ -n "$ASSERTS" ]]; then
    CMAKE_EXTRA_ARGS+=(
        "-DOPTION_USE_ASSERTS=${ASSERTS}"
    )
fi


echo
echo -e "${BOLD}${CYAN}OpenTTD Build${RESET}"
echo "----------------------------------------"
echo "Configuration : $BUILD_TYPE"
echo "Build dir     : $BUILD_DIR"
echo "Generator     : $GENERATOR"
echo "Jobs          : $JOBS"

if [[ -n "$COMPILER" ]]; then
    echo "Compiler      : $COMPILER"
else
    echo "Compiler      : system default"
fi

echo "Dedicated     : $([[ "$DEDICATED" -eq 1 ]] && echo yes || echo no)"
echo "Tools only    : $([[ "$TOOLS_ONLY" -eq 1 ]] && echo yes || echo no)"
echo "ASan          : $([[ "$ASAN" -eq 1 ]] && echo yes || echo no)"
echo "UBSan         : $([[ "$UBSAN" -eq 1 ]] && echo yes || echo no)"
echo "TSan          : $([[ "$TSAN" -eq 1 ]] && echo yes || echo no)"
echo

if [[ "$CLEAN" -eq 1 ]]; then
    info "Removing $BUILD_DIR..."

    rm -rf -- "$BUILD_DIR"
fi

if [[ "$BUILD_ONLY" -eq 1 ]]; then
    [[ -d "$BUILD_DIR" ]] ||
        die "Build directory '$BUILD_DIR' does not exist."

    info "Building existing configuration..."

    cmake --build "$BUILD_DIR" --parallel "$JOBS"

    success "Build completed."

    if [[ "$RUN" -eq 1 ]]; then
        exec "$BUILD_DIR/openttd"
    fi

    exit 0
fi


mkdir -p "$BUILD_DIR"

CMAKE_ARGS=(
    -S "$REPO_DIR"
    -B "$BUILD_DIR"
    "-G${GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
)

if [[ "${#CMAKE_EXTRA_ARGS[@]}" -gt 0 ]]; then
    CMAKE_ARGS+=("${CMAKE_EXTRA_ARGS[@]}")
fi

info "Configuring CMake..."

cmake "${CMAKE_ARGS[@]}"

success "Configuration completed."


if [[ "$CONFIGURE_ONLY" -eq 1 ]]; then
    success "Configure-only requested; stopping."
    exit 0
fi

echo
info "Building OpenTTD..."

cmake --build "$BUILD_DIR" --parallel "$JOBS"

success "Build completed successfully!"

if [[ "$RUN" -eq 1 ]]; then
    if [[ "$TOOLS_ONLY" -eq 1 ]]; then
        die "Cannot run OpenTTD with --tools-only."
    fi

    TARGET_BIN="$BUILD_DIR/openttd"
    [[ -x "$TARGET_BIN" ]] || die "Executable not found at $TARGET_BIN"

    info "Launching OpenTTD..."
    exec "$TARGET_BIN"
fi

echo
echo -e "${GREEN}${BOLD}Done!${RESET}"
echo "Binary: $BUILD_DIR/openttd"
echo