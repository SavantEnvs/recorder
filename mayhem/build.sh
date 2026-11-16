#!/usr/bin/env bash
#
# mayhem/build.sh — build recorder's fuzz harnesses, their standalone reproducers, and the oracle.
#
# Runs inside the commit image as `mayhem` in /mayhem; offline and re-runnable. Outputs:
#   /mayhem/<harness> and /mayhem/<harness>-standalone   sanitized, SanCov-instrumented, DWARF-3
#   /mayhem/ocat and /mayhem/mayhem/obj-test/selftest    normal flags, driven by mayhem/test.sh
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

# `=` (not `:=`): an explicit empty SANITIZER_FLAGS builds with no sanitizers.
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS STANDALONE_FUZZ_MAIN

cd "$SRC"

# Upstream's own configuration (MQTT, HTTP, TOURS, TZ; no Lua, no libsodium). config.mk is gitignored.
cmp -s config.mk.in config.mk || cp config.mk.in config.mk

# 1) Oracle build, project's normal flags: upstream's ocat, and a KAT binary over the same objects.
#    upstream's Makefile appends to CFLAGS/LDFLAGS from the environment, so they carry only the coverage flags.
export CFLAGS="$COVERAGE_FLAGS" LDFLAGS="$COVERAGE_FLAGS"
make -j"$MAYHEM_JOBS" ocat
rm -rf mayhem/obj-test
make -j"$MAYHEM_JOBS" -f mayhem/fuzz.mk OBJ=mayhem/obj-test selftest

# 2) Fuzz build (mayhem/lsan_off.c is linked into every binary to switch LeakSanitizer off): every project object and harness gets the sanitizers, SanCov (fuzzer-no-link, also
#    when SANITIZER_FLAGS is empty) and DWARF <= 3; -O1 keeps the fuzzer reasonably quick.
#    float-cast-overflow is relaxed: JSON numbers are doubles that recorder casts to long (storage.c tst/alt),
#    so any huge number in a mutated .rec line or payload halts on a benign cast; ASan and the rest of UBSan stay on.
rm -rf mayhem/obj-fuzz
unset CFLAGS LDFLAGS
make -j"$MAYHEM_JOBS" -f mayhem/fuzz.mk OBJ=mayhem/obj-fuzz OUT="$SRC" LSAN_OFF_SRC=mayhem/lsan_off.c \
    EXTRA="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -fno-sanitize=float-cast-overflow" fuzz
