# LocalLama

A small **Windows desktop app for offline grammar, spelling, and clarity
refinement** — think "ChatGPT for proofreading," but everything runs locally with
no network access, so it's safe on airgapped machines.

A Qt 6 GUI launches a local [llama.cpp](https://github.com/ggml-org/llama.cpp)
server (bound to `127.0.0.1` only) and streams refined text back from a local
GGUF model on your GPU. Nothing leaves the machine.

```
type / paste text  ->  Qt app  ->  llama-server (127.0.0.1)  ->  GPU  ->  refined text streams back
```

## Features

- **Fully offline.** No telemetry, no cloud, loopback-only server. Builds a
  self-contained portable folder you can carry to an airgapped box.
- **Multiple candidates.** Generates several refinements in parallel (configurable)
  and shows them side by side so you pick the best, each with its own Copy button.
- **Modes.** Fix grammar & spelling, improve clarity, make it formal, make it concise.
- **GPU or CPU.** Builds against CUDA for speed, or CPU-only for zero VRAM use.
- **Streaming output**, resident-model loading for instant responses, or optional
  idle-unload to free VRAM between uses.

## What you supply

This repo is the **app source and build tooling only**. The heavyweight pieces are
not included (they're large, machine-specific, and have their own licenses):

- **Qt 6** (Widgets + Network) and an **MSVC** C++ toolchain + **CMake**.
- **llama.cpp** — cloned and built by `scripts/fetch-llama.ps1` (optionally with CUDA).
- A **GGUF model** — download one yourself (e.g. an Apache-2.0 or MIT-licensed
  instruct model) into `runtime/models/`.

## Repository layout

```
LocalLama/
├─ CMakeLists.txt          build definition
├─ CMakePresets.json       configure/build preset
├─ LICENSE                 MIT (app source only)
├─ assets/config.json      runtime config, copied next to the exe on build
├─ src/                    application sources (Qt C++)
│  ├─ main.cpp
│  ├─ MainWindow.{h,cpp}       UI + flow
│  ├─ ServerManager.{h,cpp}    launches/supervises llama-server
│  └─ LlamaClient.{h,cpp}      streaming HTTP client (OpenAI-compatible)
├─ scripts/
│  ├─ build.ps1 / .bat     configure + compile (Release)
│  ├─ fetch-llama.ps1/.bat clone + build llama.cpp, stage into runtime/llama
│  ├─ package.ps1 / .bat   assemble portable dist/ (windeployqt + runtime + VC runtime)
│  └─ organize.ps1 / .bat  reorganise an old flat checkout into this layout
├─ runtime/                you supply these (gitignored); see the READMEs inside
│  ├─ llama/               -> llama-server.exe + DLLs
│  └─ models/              -> your .gguf model
└─ docs/
   ├─ BUILD.md             full build + airgapped transfer guide
   └─ DEVELOPMENT_RULES.md gotchas & conventions (toolchain, packaging, flags)
```

## Quick start

Requires CMake, an MSVC C++ toolchain (Visual Studio or Build Tools), and Qt 6.

```powershell
# 1. point at your Qt MSVC kit (use your installed version)
$env:QT_DIR = "C:\Qt\6.5.3\msvc2019_64"

# 2. build llama.cpp and stage it (add -Cpu for a CPU-only build)
.\scripts\fetch-llama.bat

# 3. download a GGUF model into runtime\models\model.gguf  (any instruct model)

# 4. build the app and assemble the portable folder
.\scripts\build.bat -Clean
.\scripts\package.bat            # -> dist\GrammarRefine\

# 5. run it
.\dist\GrammarRefine\GrammarRefine.exe
```

`build.ps1` forces the Visual Studio generator for an MSVC Qt kit (and autodetects
Qt under `C:\Qt` if `QT_DIR` isn't set). Full details — CUDA, model choice, and
moving the result to an airgapped machine — are in
**[docs/BUILD.md](docs/BUILD.md)**. Common pitfalls are in
**[docs/DEVELOPMENT_RULES.md](docs/DEVELOPMENT_RULES.md)**.

## Configuration

`assets/config.json` (copied next to the exe on build):

| key | meaning |
|-----|---------|
| `model_path` / `server_binary` | paths (relative to the exe) to the model and llama-server |
| `n_gpu_layers` | `99` = all layers on GPU; `0` = CPU only |
| `ctx_size` | context tokens per output slot |
| `temperature` | sampling temperature (higher = more varied candidates) |
| `num_outputs` | how many candidates to generate in parallel (1–6) |
| `idle_unload_seconds` | `0` = keep model resident (fast); `>0` = unload after N s idle |
| `extra_args` | passthrough to llama-server, e.g. `["-fa","on"]` |

## License & acknowledgments

This app's source is released under the **MIT License** (see [LICENSE](LICENSE)).
It builds on [llama.cpp](https://github.com/ggml-org/llama.cpp) (MIT) and
[Qt](https://www.qt.io/) (LGPL when dynamically linked). The GGUF model you supply
has its own license — check it before use.
