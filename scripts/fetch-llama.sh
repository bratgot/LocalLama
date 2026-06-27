#!/usr/bin/env bash
# Build llama.cpp (CUDA multi-arch by default) and stage llama-server + its shared
# libs into runtime/llama, mirroring scripts/fetch-llama.ps1 on Windows.
#
# Usage:
#   ./scripts/fetch-llama.sh            # CUDA/GPU build
#   ./scripts/fetch-llama.sh --cpu      # CPU-only build
#
# Env overrides:
#   LLAMA_DIR   where llama.cpp lives/clones to (default <repo>/llama.cpp)
#   CUDA_ARCHS  CMAKE_CUDA_ARCHITECTURES list. Default covers Turing..Blackwell.
#               Drop "120-real;120-virtual" if your CUDA toolkit is < 12.8.
#   CUDA_HOME   CUDA toolkit prefix (default /usr/local/cuda)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="${LLAMA_DIR:-$REPO_ROOT/llama.cpp}"
STAGE="$REPO_ROOT/runtime/llama"
CUDA_HOME="${CUDA_HOME:-/usr/local/cuda}"
CUDA_ARCHS="${CUDA_ARCHS:-75-real;80-real;86-real;89-real;90-real;120-real;120-virtual}"

CPU=0
[[ "${1:-}" == "--cpu" || "${1:-}" == "-cpu" || "${1:-}" == "--cpu-only" ]] && CPU=1

command -v cmake >/dev/null || { echo "ERROR: cmake not found on PATH." >&2; exit 1; }

# 1. clone if missing
if [[ ! -f "$DIR/CMakeLists.txt" ]]; then
    command -v git >/dev/null || { echo "ERROR: git not found and no llama.cpp at $DIR." >&2; exit 1; }
    echo ">> Cloning llama.cpp into $DIR ..."
    git clone https://github.com/ggml-org/llama.cpp "$DIR"
fi

# 2. configure + build. Disable curl/OpenSSL (loopback-only app, no model URLs).
BUILD="$DIR/build"
args=(-DCMAKE_BUILD_TYPE=Release -DLLAMA_CURL=OFF -DLLAMA_OPENSSL=OFF)
if [[ $CPU -eq 0 ]]; then
    args+=(-DGGML_CUDA=ON "-DCMAKE_CUDA_ARCHITECTURES=$CUDA_ARCHS")
    mode="CUDA/GPU (archs: $CUDA_ARCHS)"
else
    mode="CPU-only"
fi
echo ">> Building llama.cpp ($mode) ..."
cmake -S "$DIR" -B "$BUILD" "${args[@]}"
cmake --build "$BUILD" --config Release -j"$(nproc)"

# 3. stage llama-server + every shared lib it produced into runtime/llama
SRV="$(find "$BUILD/bin" -maxdepth 1 -name 'llama-server' -type f | head -n1)"
[[ -n "$SRV" ]] || { echo "ERROR: llama-server not found under $BUILD/bin" >&2; exit 1; }
mkdir -p "$STAGE"
find "$STAGE" -maxdepth 1 -type f ! -name 'README.txt' -delete
cp -fv "$SRV" "$STAGE/"
# ggml/llama/mtmd shared objects (locations vary by version)
find "$BUILD" -name '*.so' -o -name '*.so.*' 2>/dev/null | while read -r so; do
    case "$(basename "$so")" in
        libggml*|libllama*|libmtmd*) cp -fnv "$so" "$STAGE/" ;;
    esac
done

# 4. CUDA runtime libs (GPU build) so it runs without the full toolkit installed.
#    The DRIVER (libcuda.so.1) is NOT bundleable -- it must be present on the target
#    (it ships with the NVIDIA driver), exactly like nvcuda.dll on Windows.
if [[ $CPU -eq 0 ]]; then
    cudalib=""
    for d in "$CUDA_HOME/lib64" "$CUDA_HOME/lib" /usr/lib/x86_64-linux-gnu /usr/lib64; do
        [[ -d "$d" ]] || continue
        if ls "$d"/libcudart.so.* >/dev/null 2>&1; then cudalib="$d"; break; fi
    done
    if [[ -n "$cudalib" ]]; then
        for pat in 'libcudart.so.*' 'libcublas.so.*' 'libcublasLt.so.*'; do
            cp -fv "$cudalib"/$pat "$STAGE/" 2>/dev/null || \
                echo "WARN: no $pat in $cudalib"
        done
    else
        echo "WARN: CUDA runtime libs not found; bundle libcudart/libcublas/libcublasLt .so manually."
    fi
fi

echo
echo ">> Staged into $STAGE :"
ls -1 "$STAGE"
echo ">> Next: download a model to runtime/models/model.gguf, then ./scripts/package.sh"
