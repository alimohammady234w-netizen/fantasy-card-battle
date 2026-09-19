#!/usr/bin/env bash
# Copyright (c) Fantasy Card Battle. All rights reserved.
#
# Compiles the engine-independent part of the game (rules + data + AI) with plain g++ and runs the
# headless harness. Used for unit tests and balance simulation without an Unreal install.
#
#   Tools/mock_build.sh              build + run tests
#   Tools/mock_build.sh run --sim 300 --report
#   Tools/mock_build.sh clean

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/Saved/Harness"
mkdir -p "$OUT"

CXX="${CXX:-g++}"
FLAGS=(-std=c++20 -O1 -g0 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
INCLUDES=(-I"$ROOT/Source/FantasyCardBattle/Public" -I"$ROOT/Tools/MockUE")

SOURCES=(
	"$ROOT/Source/FantasyCardBattle/Private/FCBCardDatabase.cpp"
	"$ROOT/Source/FantasyCardBattle/Private/FCBMatchRules.cpp"
	"$ROOT/Source/FantasyCardBattle/Private/FCBAiAgent.cpp"
	"$ROOT/Tools/MockUE/SelfTest.cpp"
)

if [[ "${1:-}" == "clean" ]]; then
	rm -rf "$OUT"
	echo "cleaned $OUT"
	exit 0
fi

echo "compiling ${#SOURCES[@]} translation units with $CXX ..."
"$CXX" "${FLAGS[@]}" "${INCLUDES[@]}" "${SOURCES[@]}" -o "$OUT/fcb_harness"
echo "built $OUT/fcb_harness"

if [[ "${1:-}" == "run" ]]; then
	shift
	cd "$ROOT"
	exec "$OUT/fcb_harness" "$@"
fi

if [[ "${1:-}" == "bench" ]]; then
	cd "$ROOT"
	time "$OUT/fcb_harness" --sim "${2:-100}"
fi
