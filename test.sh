#!/bin/bash
# test.sh — Build and run all C++ unit tests for this repo.
#
# Discovers every test_*.cpp under RealSR-NCNN-Android-CLI/common/,
# compiles it with the host C++ compiler, and runs it.
#
# Usage:
#   ./test.sh            build + run all unit tests
#   ./test.sh --asan     build + run with AddressSanitizer/UBSan
#   ./test.sh --clean    remove the unit-test build directory
#
# Note: these are host-side unit tests (no ncnn/Vulkan needed).  The
# Android integration tests live in RealSR-NCNN-Android-CLI/assets/scripts/
# and must be run separately on a device/emulator.

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
UNIT_DIR="$SCRIPT_DIR/RealSR-NCNN-Android-CLI/common"
BUILD_DIR="$UNIT_DIR/build_unit_tests"

CXX="${CXX:-}"
if [[ -z "$CXX" ]]; then
    if command -v g++ >/dev/null 2>&1; then
        CXX=g++
    elif command -v clang++ >/dev/null 2>&1; then
        CXX=clang++
    else
        echo "Error: no C++ compiler found (need g++ or clang++)." >&2
        exit 1
    fi
fi

CXXFLAGS="-std=c++11 -O2 -Wall -Wextra"

for arg in "$@"; do
    case "$arg" in
        --asan)
            CXXFLAGS="$CXXFLAGS -fsanitize=address,undefined -g"
            ;;
        --clean)
            echo "Removing $BUILD_DIR"
            rm -rf "$BUILD_DIR"
            exit 0
            ;;
        -h|--help)
            sed -n '2,14p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 1
            ;;
    esac
done

mkdir -p "$BUILD_DIR"

total=0
passed=0
failed_tests=""

for src in "$UNIT_DIR"/test_*.cpp; do
    [[ -f "$src" ]] || continue
    name="$(basename "$src" .cpp)"
    bin="$BUILD_DIR/$name"
    total=$((total + 1))

    echo "=== Building $name"
    if ! "$CXX" $CXXFLAGS -I"$UNIT_DIR" -o "$bin" "$src"; then
        echo "    BUILD FAILED: $name"
        failed_tests="$failed_tests $name(build)"
        continue
    fi

    echo "=== Running  $name"
    if "$bin"; then
        passed=$((passed + 1))
    else
        echo "    TEST FAILED: $name"
        failed_tests="$failed_tests $name"
    fi
    echo ""
done

if [[ "$total" -eq 0 ]]; then
    echo "No unit tests found in $UNIT_DIR" >&2
    exit 1
fi

echo "========================================"
echo "Unit tests: $passed / $total passed"
if [[ -n "$failed_tests" ]]; then
    echo "Failed:$failed_tests"
    exit 1
fi
echo "All unit tests PASSED."
