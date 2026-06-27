# Build & deploy guide

Two machines are involved. Everything that needs the internet happens on a
**connected** machine; then you move one folder to the **airgapped** machine.

---

## On the CONNECTED machine

### 1. Build llama.cpp with CUDA

```bat
git clone https://github.com/ggml-org/llama.cpp
cd llama.cpp
cmake -B build -DGGML_CUDA=ON
cmake --build build --config Release
```

> The CUDA flag has been renamed across versions (`LLAMA_CUBLAS` ->
> `LLAMA_CUDA` -> `GGML_CUDA`). If the build complains, check llama.cpp's current
> `docs/build.md` — it's the authoritative, always-up-to-date source.

Binaries land in `build/bin/Release/`. Copy **everything** there
(`llama-server.exe` plus every sibling `.dll`) into this project's
`runtime/llama/`. If the airgapped box lacks the matching CUDA runtime, also copy
the CUDA runtime DLLs (`cudart64_*.dll`, `cublas64_*.dll`, `cublasLt64_*.dll`)
from your CUDA Toolkit `bin` folder.

### 2. Get a model

Download a quantized instruct model in **GGUF** format (a `Q4_K_M` quant of a
3B–8B instruction-tuned model is a good start) into `runtime/models/`, and set
`model_path` in `assets/config.json` to match (default `models/model.gguf`).

VRAM rule of thumb for `Q4_K_M`: ~3B ≈ 3 GB, ~7–8B ≈ 6 GB, plus the KV cache for
your context size. If you run short, lower `n_gpu_layers` in the config (the rest
runs on CPU) or pick a smaller model/quant. Verify the license for the exact
model **and size** you pick — "open source" varies.

### 3. Build the app

Needs CMake, an MSVC C++ toolchain (Visual Studio or Build Tools), and Qt 6
(Widgets + Network).

```powershell
$env:QT_DIR = "C:\Qt\6.7.2\msvc2019_64"
.\scripts\build.ps1
```

Or directly:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.7.2\msvc2019_64"
cmake --build build --config Release
```

### 4. Assemble the portable folder

```powershell
.\scripts\package.ps1
```

This copies the exe + `config.json`, runs `windeployqt` to gather Qt's DLLs, and
pulls in whatever you staged under `runtime/llama` and `runtime/models`. Result:

```
dist/LlamaChat/
├─ LlamaChat.exe
├─ config.json
├─ Qt6*.dll, platforms/, ...   (windeployqt)
├─ llama/   llama-server.exe + its DLLs
└─ models/  your-model.gguf
```

The script warns if `llama/*.exe` or `models/*.gguf` are missing.

### 5. Transfer

Zip `dist/LlamaChat`, move it to the airgapped machine by your approved
method, unzip, and run `LlamaChat.exe`. First launch loads the model into
VRAM (a few seconds to ~30s); the status bar shows **Model loaded. Ready.**

---

## Building on Linux (CUDA, portable folder)

Linux uses the `scripts/*.sh` twins of the PowerShell scripts. Build host needs
CMake, GCC, the CUDA toolkit (for a GPU build), and Qt 6 (Widgets + Network) —
either a downloaded kit (`/opt/Qt/6.x/gcc_64`) or the distro packages
(`qt6-base-dev` / `qt6-qtbase-devel`).

```bash
chmod +x scripts/*.sh              # first checkout only

# 1. Build llama.cpp (CUDA, multi-arch incl. sm_89 Ada) and stage it.
#    Drop 120 from CUDA_ARCHS if your CUDA toolkit is < 12.8.
./scripts/fetch-llama.sh           # or: ./scripts/fetch-llama.sh --cpu
# CUDA_ARCHS="75-real;80-real;86-real;89-real;90-real" ./scripts/fetch-llama.sh

# 2. Put a model at runtime/models/model.gguf (download on the connected box).

# 3. Build the app and assemble the portable folder.
QT_DIR=/opt/Qt/6.7.2/gcc_64 ./scripts/build.sh     # QT_DIR optional if system Qt
./scripts/package.sh               # -> dist/LlamaChat/

# 4. Run it (the launcher sets LD_LIBRARY_PATH + the Qt plugin path).
./dist/LlamaChat/run.sh
```

`package.sh` bundles the Qt + CUDA shared libs (via `ldd`) into `lib/`, copies the
Qt `platforms/libqxcb.so` plugin, and writes `qt.conf` + `run.sh`. The result is a
self-contained folder you `tar` and carry to the airgapped box — **always launch
via `run.sh`**, not the bare binary.

Two things the target still needs (not bundleable): the **NVIDIA driver**
(`libcuda.so.1`, the analog of Windows' `nvcuda.dll`) and basic X11/xcb system
libraries (present on any desktop install). For a headless/RDP box you may also
need `libxcb-cursor0`. The app itself, Qt, CUDA runtime, and llama-server all
travel inside the folder.

## config.json

Lives in `assets/` in the repo and is copied next to the exe on build. Paths are
resolved relative to the exe. A single `config.json` works on both OSes — the app
normalises `server_binary` (`llama/llama-server.exe`) to the running platform,
dropping `.exe` on Linux.

| key             | meaning                                                       |
|-----------------|---------------------------------------------------------------|
| `server_binary` | path to `llama-server.exe` (default `llama/llama-server.exe`)  |
| `model_path`    | path to the `.gguf` model (default `models/model.gguf`)        |
| `host` / `port` | keep `127.0.0.1`; any free port                               |
| `n_gpu_layers`  | `99` = offload all layers to GPU; lower if VRAM is tight       |
| `ctx_size`      | context window in tokens                                       |
| `temperature`   | keep low (`0.2`) so corrections don't drift from your meaning  |
| `idle_unload_seconds` | unload the model after N seconds idle (`0` = stay loaded) |
| `extra_args`    | passthrough to llama-server, e.g. `["-fa","-ctk","q8_0","-ctv","q8_0"]` |

## Modes

The dropdown (Fix grammar & spelling / Improve clarity / Make it formal / Make it
concise) just swaps the system prompt. Edit the `kModes` array in
`src/MainWindow.cpp` to change wording or add your own.

## Troubleshooting

- **Output looks garbled or includes role tags** — the GGUF has no embedded chat
  template. Add `"extra_args": ["--chat-template", "chatml"]` (or your model's
  template) to `config.json`.
- **"Model file not found" / "llama-server not found"** — paths in `config.json`
  are wrong, or `llama/` / `models/` weren't transferred next to the exe.
- **Slow** — confirm GPU offload: llama-server's startup log (shown in the status
  bar before "Ready") should mention CUDA and offloaded layers.
- **`build.ps1` can't find Qt** — pass `-QtDir C:\Qt\<ver>\msvc<...>_64` or set
  `$env:QT_DIR`.
- **Security** — the server binds to loopback only and never listens on the
  network; nothing leaves the machine.
