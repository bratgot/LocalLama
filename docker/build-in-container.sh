#!/usr/bin/env bash
# Runs INSIDE the llamachat-build container. Assembles a clean working copy from
# the read-only source mount (/src), builds llama.cpp (CUDA) + the Qt app, packages
# the portable folder, and drops a tarball into the output mount (/out).
#
#   docker run --rm \
#     --mount type=bind,source=<repo>,target=/src,readonly \
#     --mount type=bind,source=<outdir>,target=/out \
#     llamachat-build:rocky9 bash /src/docker/build-in-container.sh
set -euo pipefail

# Match the Windows build's GPU range: Turing(75)..Blackwell(120) + PTX fallback.
# sm_120 requires the CUDA 12.9 base image.
export CUDA_ARCHS="${CUDA_ARCHS:-75-real;80-real;86-real;89-real;90-real;120-real;120-virtual}"

# Refresh source from the read-only mount, but KEEP llama.cpp/build/runtime/dist so
# re-runs against a persistent /work volume are incremental (no re-clone/recompile).
mkdir -p /work
rm -rf /work/src /work/scripts /work/assets
cp -r /src/src /src/scripts /src/assets /work/
cp /src/CMakeLists.txt /work/
[[ -f /src/CMakePresets.json ]] && cp /src/CMakePresets.json /work/ || true
cd /work
# Safety net: strip any CRLF the Windows host may have introduced, or the shell
# fails with "bad interpreter: ...^M".
find . -name '*.sh' -exec sed -i 's/\r$//' {} +
chmod +x scripts/*.sh

echo "=================== fetch-llama (CUDA $CUDA_ARCHS) ==================="
./scripts/fetch-llama.sh

echo "=================== build app ======================================="
./scripts/build.sh

echo "=================== package ========================================="
./scripts/package.sh

echo "=================== tar (no model) =================================="
mkdir -p /out
tar -C /work/dist -czf /out/LlamaChat-linux-rocky9.tar.gz LlamaChat
echo "DONE -> /out/LlamaChat-linux-rocky9.tar.gz"
ls -la /out
echo "--- ldd of the built binary (sanity) ---"
ldd /work/dist/LlamaChat/LlamaChat | head -40 || true
