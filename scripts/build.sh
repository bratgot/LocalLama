#!/usr/bin/env bash
# Configure and build the LlamaChat Qt app on Linux, mirroring scripts/build.ps1.
#
# Usage:
#   ./scripts/build.sh                 # Release build
#   QT_DIR=/opt/Qt/6.7.2/gcc_64 ./scripts/build.sh
#
# Env:
#   QT_DIR   Qt 6 install prefix. If unset, relies on a system Qt6 that CMake's
#            find_package(Qt6) can locate (e.g. distro qt6-base-dev).
#   CONFIG   Release (default) | Debug | RelWithDebInfo
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$REPO_ROOT/build"
CONFIG="${CONFIG:-Release}"

command -v cmake >/dev/null || { echo "ERROR: cmake not found on PATH." >&2; exit 1; }

prefix_args=()
if [[ -n "${QT_DIR:-}" ]]; then
    [[ -d "$QT_DIR" ]] || { echo "ERROR: QT_DIR=$QT_DIR does not exist." >&2; exit 1; }
    prefix_args+=("-DCMAKE_PREFIX_PATH=$QT_DIR")
    echo ">> Qt: $QT_DIR"
else
    echo ">> Qt: relying on system Qt6 (set QT_DIR to use a specific kit)."
fi

echo ">> Configuring ($CONFIG) ..."
cmake -S "$REPO_ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE="$CONFIG" "${prefix_args[@]}"

echo ">> Building ..."
cmake --build "$BUILD" --config "$CONFIG" -j"$(nproc)"

if [[ -f "$BUILD/LlamaChat" ]]; then
    echo
    echo ">> Built: $BUILD/LlamaChat"
    echo ">> Next:  ./scripts/package.sh   (assembles the portable folder)"
else
    echo "WARN: build finished but $BUILD/LlamaChat not found." >&2
fi
