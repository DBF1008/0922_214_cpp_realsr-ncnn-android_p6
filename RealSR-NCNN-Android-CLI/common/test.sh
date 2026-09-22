#!/bin/bash
# test.sh — Build and run all unit/regression tests for the shared
# GPU tile download fix (gpu_tile_download.h).
#
# Usage:
#   ./test.sh           Run all unit tests
#
# Tests covered:
#   1. test_gpu_tile_offset            — tile-row offset / write-back bounds
#   2. test_gpu_tile_offset (ASan)     — same tests under AddressSanitizer
#   3. static regression guard         — no backend may re-introduce the
#                                        unsafe external-pointer download

set -u
cd "$(dirname "$0")"

CXX="${CXX:-c++}"
CXXFLAGS="-std=c++11 -O2 -Wall"
TOTAL=0
FAILED=0

run_test() {
    local name="$1"; shift
    TOTAL=$((TOTAL + 1))
    echo "=== $name ==="
    if "$@"; then
        echo "[PASS] $name"
    else
        echo "[FAIL] $name"
        FAILED=$((FAILED + 1))
    fi
    echo
}

# --- 1. Offset / write-back regression tests (plain build) ----------------
build_plain() {
    $CXX $CXXFLAGS -o test_gpu_tile_offset test_gpu_tile_offset.cpp \
        && ./test_gpu_tile_offset
}
run_test "test_gpu_tile_offset" build_plain

# --- 2. Same tests under AddressSanitizer ---------------------------------
build_asan() {
    $CXX -std=c++11 -O1 -g -fsanitize=address \
        -o test_gpu_tile_offset_asan test_gpu_tile_offset.cpp \
        && ./test_gpu_tile_offset_asan
}
run_test "test_gpu_tile_offset (ASan)" build_asan

# --- 3. Static regression guard --------------------------------------------
# Every ncnn backend's GPU path must download via download_gpu_tile_to_output
# and must not resurrect the unsafe external-pointer record_clone pattern.
static_guard() {
    local rc=0
    local backends="
        ../Waifu2x/src/main/jni/waifu2x.cpp
        ../SRMD/src/main/jni/srmd.cpp
        ../RealCUGAN/src/main/jni/realcugan.cpp
        ../RealSR/src/main/jni/realsr.cpp
    "
    for f in $backends; do
        if ! grep -q "download_gpu_tile_to_output" "$f"; then
            echo "GUARD FAIL: $f does not use download_gpu_tile_to_output"
            rc=1
        fi
        # Unsafe pattern: cloning out_gpu straight into a (possibly
        # external-pointer) Mat instead of using the shared helper.
        if grep -q "record_clone(out_gpu, out, opt)" "$f"; then
            echo "GUARD FAIL: $f still clones out_gpu directly (external-pointer download)"
            rc=1
        fi
    done
    # The shared header must exist and be included by every backend.
    for f in $backends; do
        if ! grep -q "common/gpu_tile_download.h" "$f"; then
            echo "GUARD FAIL: $f does not include gpu_tile_download.h"
            rc=1
        fi
    done
    [ $rc -eq 0 ] && echo "All backends use the safe download helper."
    return $rc
}
run_test "static regression guard" static_guard

# --- Summary ----------------------------------------------------------------
echo "================================"
if [ $FAILED -eq 0 ]; then
    echo "All $TOTAL test groups PASSED."
    rm -f test_gpu_tile_offset_asan
    exit 0
else
    echo "$FAILED / $TOTAL test groups FAILED."
    exit 1
fi
