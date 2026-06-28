# LlamaChat

A cross-platform (**Windows + Linux**) desktop app for **offline writing
refinement** — grammar, spelling, clarity, tone — like ChatGPT for proofreading,
but everything runs locally with no network access. Safe on airgapped machines.

A Qt 6 GUI launches a local [llama.cpp](https://github.com/ggml-org/llama.cpp)
server bound to `127.0.0.1` only and streams refined text back from a local GGUF
model on your GPU. Nothing leaves the machine at any point.

```
type / paste text  ->  Qt app  ->  llama-server (127.0.0.1)  ->  GPU  ->  refined text streams back
```

## Features

- **Multi-mode, side by side.** Tick any of four modes — Fix grammar & spelling /
  Improve clarity / Make it formal / Make it concise — and each ticked mode
  produces its own suggestion column, each with a Copy button.
- **Regenerate & navigate.** "New answers" reruns with fresh seeds; ◀ / ▶ step
  back through earlier results; ↶ / ↷ undo/redo your input edits.
- **History panel.** Every run is saved; click any past conversation to reload it.
  "Clear history" is itself undoable.
- **Themes & fonts.** Light / Dark / **Wild** (purple-pink-green) themes, plus a
  font family + size picker — all remembered.
- **Per-user, network-friendly.** Preferences and history live in each user's
  profile, so one read-only copy on a network share serves many users (see below).
- **Model info panel** + a persistent status line: model, load state, backend,
  GPU layers, context.
- **GPU-accelerated, multi-arch.** CUDA build runs on NVIDIA GPUs from Turing
  through Blackwell (sm_75–120); CPU-only build also supported.
- **Fully offline & airgap-safe.** No telemetry, loopback-only server, single
  portable folder. No installer, no admin rights on the target.

## Tested on

| Component | Windows | Linux |
|-----------|---------|-------|
| OS | Windows 11 | RHEL/Alma/Rocky 9 (glibc 2.34) |
| GPU | RTX 5060 Ti (Blackwell), RTX 4000 Ada | same (sm_75–120 in one build) |
| CUDA | 12.9 | 12.9 (via Docker build image) |
| Qt | 6.5.3 (MSVC) | 6.6 (system / EPEL) |
| Compiler | MSVC 19.40 (VS 2022) | GCC (Rocky 9) |
| Model | Qwen2.5-7B-Instruct Q4_K_M (Apache-2.0) | same |

## What this repo contains

**App source and build tooling only** — the heavyweight, separately-licensed
pieces are not included. You supply Qt 6 (Widgets + Network), a C++ toolchain
(MSVC on Windows / GCC on Linux), CMake, and a GGUF model. `llama.cpp` is cloned
and built automatically by the `fetch-llama` scripts.

## Repository layout

```
LlamaChat/
├─ CMakeLists.txt              build definition (target: LlamaChat)
├─ src/                        Qt C++ app: main, MainWindow, ServerManager, LlamaClient
├─ assets/config.json          runtime config, copied next to the exe on build
├─ scripts/
│  ├─ build.{ps1,bat} / build.sh         configure + compile the app
│  ├─ fetch-llama.{ps1,bat} / fetch-llama.sh   clone + build llama.cpp (CUDA), stage it
│  └─ package.{ps1,bat} / package.sh     assemble the portable dist/ folder
├─ docker/
│  ├─ Dockerfile               reproducible Linux (Rocky 9 + CUDA 12.9 + Qt6) build image
│  └─ build-in-container.sh    builds + packages the Linux portable folder
├─ runtime/                    gitignored — staged llama-server + model go here
└─ docs/
   ├─ BUILD.md                 full build + airgapped-transfer guide (Windows & Linux)
   └─ DEVELOPMENT_RULES.md     toolchain / packaging gotchas
```

## Quick start

### Windows

```powershell
git clone https://github.com/bratgot/LlamaChat   # (repo: bratgot/LocalLama)
cd LlamaChat
$env:QT_DIR = "C:\Qt\6.5.3\msvc2019_64"           # adjust to your kit
.\scripts\fetch-llama.bat                         # build llama.cpp (CUDA); -Cpu for CPU-only
# put a GGUF model at runtime\models\model.gguf  (see Model below)
.\scripts\build.bat -Clean
.\scripts\package.bat                             # -> dist\LlamaChat\
.\dist\LlamaChat\LlamaChat.exe
```

### Linux (portable folder + run.sh)

```bash
chmod +x scripts/*.sh
./scripts/fetch-llama.sh                           # CUDA build; --cpu for CPU-only
# put a GGUF model at runtime/models/model.gguf
QT_DIR=/opt/Qt/6.7.2/gcc_64 ./scripts/build.sh     # or rely on system Qt6
./scripts/package.sh                               # -> dist/LlamaChat/
./dist/LlamaChat/run.sh
```

For a **reproducible, glibc-matched** Linux build (recommended for airgapped
RHEL/Alma/Rocky 9 targets), use the Docker image — see **[docs/BUILD.md](docs/BUILD.md)**,
which also covers CUDA setup, the `run.sh` launcher, and airgapped transfer.
Toolchain gotchas are in **[docs/DEVELOPMENT_RULES.md](docs/DEVELOPMENT_RULES.md)**.

## Model

Download any GGUF instruct model and save it as `runtime/models/model.gguf`. A
good default that fits most NVIDIA GPUs:

```powershell
# Qwen2.5-7B-Instruct Q4_K_M (~4.7 GB, Apache-2.0)
curl.exe -fL "https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF/resolve/main/Qwen2.5-7B-Instruct-Q4_K_M.gguf?download=true" `
         -o "runtime\models\model.gguf"
```

**VRAM guide (Q4_K_M):** ~1.5B ≈ 2 GB, ~7–8B ≈ 6 GB. Always verify the license of
the exact model **and size** — Qwen2.5 is Apache-2.0 at 1.5B/7B but a
non-commercial license at 3B; Phi-3.5-mini is MIT; Llama models are **not** OSI
open source.

## Configuration

Edit `config.json` (next to the exe, or `assets/config.json` before building). A
single config works on both OSes — the app normalises `server_binary`
(`llama/llama-server.exe`) to the platform, dropping `.exe` on Linux.

| Key | Default | Meaning |
|-----|---------|---------|
| `server_binary` | `llama/llama-server.exe` | path to llama-server (relative to exe) |
| `model_path` | `models/model.gguf` | path to the GGUF model |
| `n_gpu_layers` | `99` | layers offloaded to GPU; `0` = CPU only |
| `ctx_size` | `4096` | context tokens per output slot |
| `temperature` | `0.6` | sampling temperature |
| `num_outputs` | `3` | concurrency for the server's parallel slots |
| `default_mode` | `Make it concise` | first-run default mode |
| `idle_unload_seconds` | `0` | `0` = stay resident (fast); `>0` = unload after N s idle |
| `extra_args` | `["-fa","on"]` | extra args passed to llama-server |

## Preferences, history & multi-user / network deployment

Per-user settings (theme, font, selected modes, window size) and conversation
history are stored in **each user's own profile**, not next to the exe:

- Windows: `%LOCALAPPDATA%\LlamaChat\` — `settings.ini`, `history.json`
- Linux:   `~/.config/LlamaChat/`

So a single copy of the (large) app + model folder can sit on a **read-only
network share** and be run by many users at once — each keeps independent prefs
and history, and nothing is written back to the shared folder. To ship shared
defaults, drop a `settings.ini` next to the exe: each user is seeded from it on
first run, then saves their own changes privately. **Reset preferences** in the
app restores the current user's defaults.

## Airgapped deployment

`scripts/package.bat` (Windows) / `scripts/package.sh` (Linux) assembles a fully
self-contained portable folder: app, Qt libraries, the C++ runtime, llama-server,
CUDA runtime libraries (and on Linux the X11/xcb client libs), plus your model.
Zip/tar it and transfer by approved media — no installer, no internet, no admin
rights. The only target prerequisite for the GPU build is the **NVIDIA driver**
(it provides `libcuda` + the GL libraries). Full details in
**[docs/BUILD.md](docs/BUILD.md)**.

## License & acknowledgments

App source: **MIT** — see [LICENSE](LICENSE).
Built on [llama.cpp](https://github.com/ggml-org/llama.cpp) (MIT) and
[Qt](https://www.qt.io/) (LGPL, dynamically linked).
The GGUF model you supply has its own license — verify before use.
