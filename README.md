# LocalLama

A Windows 11 desktop app for **offline grammar, spelling, and writing
refinement** — like ChatGPT for proofreading, but everything runs locally with
no network access. Safe on airgapped machines.

A Qt 6 GUI launches a local [llama.cpp](https://github.com/ggml-org/llama.cpp)
server bound to `127.0.0.1` only, and streams refined text back from a local
GGUF model on your GPU. Nothing leaves the machine at any point.

```
type / paste text  ->  Qt app  ->  llama-server (127.0.0.1)  ->  GPU  ->  refined text streams back
```

## Features

- **Fully offline and airgap-safe.** No telemetry, no cloud calls, loopback-only
  server. Builds a single portable folder you can carry to any isolated machine.
- **Multiple candidates.** Generates several refinements in parallel and shows
  them side by side — pick the best, each has its own Copy button.
- **Four modes.** Fix grammar & spelling / Improve clarity / Make it formal /
  Make it concise. Default mode is configurable without recompiling.
- **GPU-accelerated.** Builds against CUDA for fast inference; CPU-only build
  also supported for zero VRAM impact.
- **Streaming output.** Text appears token by token. Model stays resident for
  instant responses, or can idle-unload to free VRAM between uses.
- **No installer.** The packaged output is a plain folder — xcopy deploy,
  zip and transfer, no admin rights needed on the target.

## Tested on

| Component | Version |
|-----------|---------|
| GPU | NVIDIA GeForce RTX 5060 Ti (16 GB, Blackwell) |
| CUDA | 12.x |
| Qt | 6.5.3 msvc2019_64 |
| Compiler | MSVC 19.40 (Visual Studio 2022) |
| llama.cpp | latest main (June 2026) |
| Model | Qwen2.5-7B-Instruct Q4_K_M (Apache-2.0) |
| Target OS | Windows 11 |

## What this repo contains

**App source and build tooling only.** The heavyweight pieces are not included
(large, machine-specific, separate licenses):

| You supply | How |
|-----------|-----|
| Qt 6 (Widgets + Network) | [qt.io](https://www.qt.io/download-open-source) — install the MSVC x64 kit |
| MSVC C++ toolchain | Visual Studio 2019/2022 or the standalone Build Tools (Desktop C++ workload) |
| CMake | bundled with VS, or [cmake.org](https://cmake.org/download/) |
| llama.cpp | cloned and built automatically by `scripts/fetch-llama.bat` |
| GGUF model | download separately — see [Model](#model) below |

## Repository layout

```
LocalLama/
├─ CMakeLists.txt              build definition
├─ CMakePresets.json           configure/build preset
├─ LICENSE                     MIT (app source only)
├─ assets/
│  └─ config.json              runtime config, copied next to the exe on build
├─ src/                        Qt C++ application
│  ├─ main.cpp
│  ├─ MainWindow.{h,cpp}       UI + flow
│  ├─ ServerManager.{h,cpp}    launches/supervises llama-server subprocess
│  └─ LlamaClient.{h,cpp}      streaming HTTP client (OpenAI-compatible endpoint)
├─ scripts/
│  ├─ build.{ps1,bat}          configure + compile (Release, forces MSVC generator)
│  ├─ fetch-llama.{ps1,bat}    clone + build llama.cpp, stage into runtime/llama/
│  ├─ package.{ps1,bat}        assemble portable dist/ folder (windeployqt + runtime)
│  └─ organize.{ps1,bat}       reorganise a flat checkout into this layout
├─ runtime/                    gitignored — you populate these; see READMEs inside
│  ├─ llama/                   llama-server.exe + DLLs + CUDA runtime DLLs
│  └─ models/                  your .gguf model file
└─ docs/
   ├─ BUILD.md                 full build and airgapped-transfer guide
   └─ DEVELOPMENT_RULES.md     hard-won gotchas: toolchain, packaging, llama flags
```

## Quick start

```powershell
# Clone
git clone https://github.com/bratgot/LocalLama
cd LocalLama

# Tell the scripts where your Qt MSVC kit is
$env:QT_DIR = "C:\Qt\6.5.3\msvc2019_64"   # adjust to your installed version

# Build llama.cpp with CUDA and stage it  (takes a few minutes on first run)
.\scripts\fetch-llama.bat
# CPU-only alternative (no CUDA, no VRAM use, slower):
# .\scripts\fetch-llama.bat -Cpu

# Download a GGUF model (see Model section below) into:
#   runtime\models\model.gguf

# Build the app and assemble the portable folder
.\scripts\build.bat -Clean
.\scripts\package.bat       # -> dist\LlamaChat\

# Run
.\dist\LlamaChat\LlamaChat.exe
```

The scripts auto-detect the newest Visual Studio toolchain via `vswhere` and the
newest Qt MSVC kit under `C:\Qt\` if `QT_DIR` is not set. They force the Visual
Studio generator — critical when Strawberry Perl or another MinGW GCC is on PATH,
which would otherwise be picked up and produce ABI-incompatible binaries.

Full details, CUDA setup, and airgapped transfer instructions are in
**[docs/BUILD.md](docs/BUILD.md)**. Toolchain gotchas (and how to avoid them)
are in **[docs/DEVELOPMENT_RULES.md](docs/DEVELOPMENT_RULES.md)**.

## Model

Download any GGUF instruct model and save it as `runtime\models\model.gguf`.
A good starting point that fits comfortably on most NVIDIA GPUs:

```powershell
# Qwen2.5-7B-Instruct Q4_K_M (~4.7 GB, Apache-2.0 license)
curl.exe -fL "https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF/resolve/main/Qwen2.5-7B-Instruct-Q4_K_M.gguf?download=true" `
         -o "runtime\models\model.gguf"
```

Or download in a browser from
[bartowski/Qwen2.5-7B-Instruct-GGUF](https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF)
and save as `runtime\models\model.gguf`.

**VRAM guide (Q4_K_M quant):**

| Model size | VRAM needed | Notes |
|-----------|------------|-------|
| 1.5B | ~2 GB | Phi-3.5-mini (MIT) or Qwen2.5-1.5B (Apache) |
| 7–8B | ~6 GB | Qwen2.5-7B (Apache) — recommended |

Always verify the license of the exact model **and size** you use — it varies.
Qwen2.5 is Apache-2.0 at 1.5B and 7B but carries a non-commercial research
license at 3B. Phi-3.5-mini is MIT. Llama models are **not** OSI open source.

## Configuration

Edit `dist\LlamaChat\config.json` (or `assets\config.json` before building)
to change behaviour without recompiling:

| Key | Default | Meaning |
|-----|---------|---------|
| `server_binary` | `llama/llama-server.exe` | path to llama-server (relative to exe) |
| `model_path` | `models/model.gguf` | path to the GGUF model |
| `n_gpu_layers` | `99` | layers offloaded to GPU; `0` = CPU only |
| `ctx_size` | `4096` | context tokens per output slot |
| `temperature` | `0.6` | sampling temperature (higher = more varied) |
| `num_outputs` | `3` | candidates generated in parallel (1–6) |
| `default_mode` | `Make it concise` | startup mode: `Fix grammar & spelling`, `Improve clarity`, `Make it formal`, `Make it concise` |
| `idle_unload_seconds` | `0` | `0` = model stays resident (fast); `>0` = unload after N s idle |
| `extra_args` | `["-fa","on"]` | extra args passed to llama-server |

> **Note on `-fa`:** recent llama.cpp builds require `-fa on` (not bare `-fa`).
> If the app hangs on "Loading model…", run `llama-server.exe --help` and check
> the exact syntax for your build.

## Airgapped deployment

`scripts/package.bat` assembles `dist\LlamaChat\` as a fully self-contained
portable folder: app exe, Qt DLLs, MSVC runtime, llama-server, CUDA runtime DLLs,
and the model. Zip it and transfer by approved media. No installer, no internet,
no admin rights needed on the target.

## License & acknowledgments

App source: **MIT** — see [LICENSE](LICENSE).  
Built on [llama.cpp](https://github.com/ggml-org/llama.cpp) (MIT) and
[Qt](https://www.qt.io/) (LGPL, dynamically linked).  
The GGUF model you supply has its own license — verify before use.
