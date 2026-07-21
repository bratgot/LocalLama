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

- **Two tabs: Refine + Chat.** *Refine* is the side-by-side proofreading view;
  *Chat* is a full offline conversation with the model (ChatGPT-style, multi-turn,
  streaming) for free-form rewriting, drafting, or asking how to phrase something.
- **Multi-mode, side by side.** In Refine, tick any of five modes — **Rewrite** /
  Fix grammar & spelling / Improve clarity / Make it formal / Make it concise — and
  each ticked mode produces its own suggestion column, each with a Copy button.
- **Context dictionaries.** Drop-in `.txt` glossaries (ships with VFX, Cinematography,
  and Computer-graphics term dictionaries) teach the model your jargon, initialisms,
  and abbreviations — applied to both tabs.
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

## What's new in v0.2

**New Chat tab** — a full offline, multi-turn, streaming conversation with the
model, alongside the existing Refine proofreader.
- Input-on-top layout; **Submit** / **Stop**; **Clear chat** / **Copy reply**; a
  **Show thinking** toggle for the model's reasoning.
- **Conversation-history panel** (like Refine's): every chat auto-saves; click a
  past conversation to reload and continue it.
- Categorised toggles — **Edit:** Rewrite (default on) / Improve clarity / Make it
  concise / Make it formal / Expand abbreviations, and **Tone:** Warm / Friendly /
  Professional / Assertive / Urgent / No worries (all multi-select).

**Refine** — added a **Rewrite** mode (default on) alongside the existing four.

**Context + dictionaries**
- **Global Context** — a standing instruction / tone / audience applied to **both**
  tabs. Save reusable **presets** (per-user, and **team-seedable** via a shared
  `settings.ini`). Replaces the old Refine-only "Intent" button.
- **File-based dictionaries** — drop a `.txt` into the `contexts/` folder (next to
  the exe, or your per-user folder) and it becomes a Context. Ships with **VFX terms
  & jargon**, **VFX production**, **Cinematography**, and **Computer graphics**
  glossaries so the model understands your abbreviations and initialisms.

**New default model: Qwen3-8B** (Apache-2.0) — stronger at instruction-following
than the old Qwen2.5-7B, same ~5 GB footprint. Refine runs it in fast no-think
mode; Chat keeps full reasoning, shown separately.

**Interface**
- **Theme is global** (in the header, reachable from both tabs) and the dark/wild
  themes now style the tabs.
- **Help / Tech info tab** — a built-in guide with examples, plus the Model info
  panel (moved here from Refine).
- **Live GPU VRAM** (used / total) in the status bar; version shown in the app
  name; app icon & taskbar polish.
- **Remembers more** — active tab, chat Edit/Tone toggles, and Context selection
  persist per-user; chat history saved to `chat.json`, like Refine history.

> **⚠️ Higher VRAM than v0.1.** v0.2 uses more GPU memory: the Chat tab adds a
> **5th parallel server slot** (KV cache scales as `ctx_size × slots`, up from 4→5
> slots) and Qwen3-8B is marginally larger than the old Qwen2.5-7B. Measured
> **~9.4 GB** on a 16 GB card. On cards with **< 10 GB** (e.g. an 8 GB Ada), lower
> `n_gpu_layers` or `ctx_size` in `config.json`, or switch to **Qwen3-4B** (~3 GB).

## Tested on

| Component | Windows | Linux |
|-----------|---------|-------|
| OS | Windows 11 | RHEL/Alma/Rocky 9 (glibc 2.34) |
| GPU | RTX 5060 Ti (Blackwell), RTX 4000 Ada | same (sm_75–120 in one build) |
| CUDA | 12.9 | 12.9 (via Docker build image) |
| Qt | 6.5.3 (MSVC) | 6.6 (system / EPEL) |
| Compiler | MSVC 19.40 (VS 2022) | GCC (Rocky 9) |
| Model | Qwen3-8B Q4_K_M (Apache-2.0, thinking) | same |

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

Download any GGUF instruct model and save it as `runtime/models/model.gguf`. The
default is **Qwen3-8B** — Apache-2.0, same ~5 GB footprint as the old Qwen2.5-7B
but stronger at following free-form instructions (which powers the Chat tab):

```powershell
# Qwen3-8B Q4_K_M (~5.0 GB, Apache-2.0)
curl.exe -fL "https://huggingface.co/bartowski/Qwen_Qwen3-8B-GGUF/resolve/main/Qwen_Qwen3-8B-Q4_K_M.gguf?download=true" `
         -o "runtime\models\model.gguf"
```

**Thinking (reasoning).** Qwen3 is a hybrid model with chain-of-thought **on by
default**. The app handles this automatically: the Chat tab shows the model's
thinking in a separate, collapsible block, while the Refine tab hides it so
proofreading columns stay clean and fast. Because thinking tokens add latency,
lighter GPUs may prefer **Qwen3-4B-Instruct-2507** (~2.5 GB, Apache-2.0), which
never emits thinking. To force no-thinking on any Qwen3 model, add
`"--chat-template-kwargs", "{\"enable_thinking\":false}"` to `extra_args`.

**VRAM guide (Q4_K_M):** ~4B ≈ 3 GB, ~8B ≈ 6 GB, ~14B ≈ 10 GB. Always verify the
license of the exact model **and size** — Qwen3 (4B/8B/14B) is Apache-2.0; Phi-4
is MIT; Gemma 3 uses Google's custom terms; Llama models are **not** OSI open
source.

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
| `default_mode` | `Rewrite` | first-run default mode |
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
