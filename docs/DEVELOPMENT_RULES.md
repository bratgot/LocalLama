# Development rules

Running notes and conventions for working on this repo. Add new rules here as you
hit them.

---

## 1. Running the build / package scripts (PowerShell execution policy)

**Symptom**

```
.\scripts\build.ps1 : File ...\build.ps1 cannot be loaded. The file ... is not
digitally signed. You cannot run this script on the current system.
... FullyQualifiedErrorId : UnauthorizedAccess
```

**Why** — Windows PowerShell blocks unsigned scripts by default. Scripts that
came out of a downloaded zip also carry the "mark of the web", so even a
`RemoteSigned` policy will refuse them until that mark is cleared.

**Fixes** (pick one)

- **Easiest — use the wrapper.** Each script has a `.bat` twin
  (`build.bat`, `package.bat`, `organize.bat`) that launches PowerShell with the
  policy bypassed for that one call, so it always works:
  ```powershell
  .\scripts\build.bat -Clean
  .\scripts\package.bat
  ```

- **This session only.** Bypass for the current terminal; reverts when you close
  it, changes nothing machine-wide:
  ```powershell
  Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
  .\scripts\build.ps1 -Clean
  .\scripts\package.ps1
  ```

- **Persistent for your user.** Allow local scripts permanently, and clear the
  mark of the web on the repo's scripts so `RemoteSigned` accepts them:
  ```powershell
  Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
  Get-ChildItem -Recurse .\scripts\*.ps1 | Unblock-File
  ```

Do **not** use `-Scope LocalMachine` (needs admin and changes policy for every
user). Process or CurrentUser scope is enough.

---

## 2. Build environment

- Point the scripts at Qt with `$env:QT_DIR` (e.g.
  `C:\Qt\6.7.2\msvc2019_64`), or pass `-QtDir <path>`. `build.ps1` autodetects
  the newest kit under `C:\Qt` if neither is set.
- Build from a shell that has the MSVC toolchain available (a normal terminal is
  fine as long as CMake can find a Visual Studio generator; "Developer
  PowerShell for VS" always works).
- Build order: `build.ps1` → stage `runtime/` → `package.ps1`.

## 3. What not to commit

- `build/`, `dist/`, and anything under `runtime/llama/` and `runtime/models/`
  are git-ignored. Never commit `llama-server.exe`, its DLLs, or `.gguf` models —
  they're large and have their own licenses. Only the placeholder `README.txt`
  files in those folders are tracked.

## 4. Runtime conventions

- The llama-server always binds to `127.0.0.1` only. Don't change `host` to
  `0.0.0.0` — this app is meant to stay off the network.
- Keep `temperature` low (≈0.2) in `config.json` so corrections don't drift from
  the user's meaning.
- Paths in `config.json` are resolved relative to the exe, matching the deployed
  layout (`llama/`, `models/` next to `LlamaChat.exe`).

---

## 5. Toolchain must match the Qt kit (MSVC vs MinGW)

**Symptom**

```
.../Strawberry/c/.../mingw32/.../ld.exe: undefined reference to
`__imp__ZN7QObject11connectImpl...`
collect2.exe: error: ld returned 1 exit status
```

**Why** — The Qt kit installed here is **MSVC** (`...\msvc2019_64`), but CMake
found a **MinGW GCC** on PATH (Strawberry Perl bundles one) and built with it.
MSVC-built Qt and MinGW/GCC are ABI-incompatible, so Qt's symbols never link.
The two cannot be mixed.

**Rule** — Match the compiler to the kit:

| Qt kit (QT_DIR)        | Generator / compiler            |
|------------------------|---------------------------------|
| `...\msvc2019_64`      | Visual Studio generator (cl.exe)|
| `...\mingw_64`         | Ninja + the matching MinGW GCC  |

`build.ps1` now does this automatically: an MSVC kit forces the Visual Studio
generator (`-G "Visual Studio NN ..." -A x64`) and ignores any GCC on PATH; a
MinGW kit uses Ninja + GCC. If you change kits, the build dir is auto-cleaned
because the generator changed.

**If the MSVC route fails with "no CMAKE_CXX_COMPILER found"** you don't have the
Microsoft C++ toolchain. Either install Visual Studio 2019/2022 or the standalone
Build Tools with the **Desktop development with C++** workload, or switch to a
MinGW Qt kit and point `QT_DIR` at it.

**Manual equivalent** (if not using the script):

```powershell
Remove-Item build -Recurse -Force
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH="$env:QT_DIR"
cmake --build build --config Release
```

> Side note: Strawberry Perl's `C:\Strawberry\c\bin` on PATH is a common cause of
> CMake picking the wrong compiler for unrelated projects too. Forcing the
> generator (as above) sidesteps it without removing Strawberry from PATH.

---

## 6. windeployqt must use the SAME Qt kit as the build (MSVC vs MinGW, again)

**Symptom** — The exe is built fine, but launching it shows a dialog:

```
LlamaChat.exe - Entry Point Not Found
The procedure entry point _Z26qt_QMetaEnum_debugOperator...QMetaObjectPKc
could not be located in the dynamic link library ...\LlamaChat.exe
```

Exit code `-1073741511` (0xC0000139, STATUS_ENTRYPOINT_NOT_FOUND).

**Why** — `windeployqt` deployed **MinGW** Qt DLLs next to the **MSVC**-built exe.
The two ABIs can't resolve each other's symbols. The giveaway is the **`_Z…`**
prefix on the symbol name: that's GCC/MinGW name mangling. MSVC Qt exports names
that look like `?qt_QMetaEnum...@@`, never `_Z…`. This happens because
`package.ps1` deploys from whatever `QT_DIR` points to, and `QT_DIR` was a MinGW
kit (or blank) at package time.

**The tell** — MinGW runtime DLLs land in the dist folder only on a MinGW deploy:

```powershell
Get-ChildItem .\dist\LlamaChat\libgcc*.dll, .\dist\LlamaChat\libstdc++*.dll, `
              .\dist\LlamaChat\libwinpthread*.dll -ErrorAction SilentlyContinue
```

If those exist, it's a MinGW deploy. They must NOT be there for this MSVC app.

**Rule** — Set `QT_DIR` to the **MSVC** kit and build *and* package in the **same
shell**, because environment variables don't survive a new terminal:

```powershell
$env:QT_DIR = "C:\Qt\6.7.2\msvc2019_64"
.\scripts\build.bat -Clean
.\scripts\package.bat
```

Then re-run the `libgcc` check above and confirm it returns nothing.

## 7. The portable build needs the MSVC runtime bundled

**Symptom** — The exe runs and exits immediately with **no window** (and no error
dialog), or exit code `-1073741515` (0xC0000135, "DLL not found"). During
packaging, windeployqt printed:

```
Warning: Cannot find Visual Studio installation directory, VCINSTALLDIR is not set.
```

**Why** — An MSVC-built exe depends on the Visual C++ runtime
(`vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`). `windeployqt` only
copies these when it can find the VS install (its `--compiler-runtime` step),
which needs `VCINSTALLDIR` — set inside a **Developer** prompt, not a plain one.
On a dev box with VS installed the exe still finds the runtime in `System32`, so
it may "work here" yet fail on a clean or airgapped machine.

**Check**

```powershell
Get-ChildItem .\dist\LlamaChat\vcruntime140*.dll, .\dist\LlamaChat\msvcp140.dll -ErrorAction SilentlyContinue
```

**Fix** — Deploy the runtime from a Developer prompt so it travels with the app:

```powershell
# in "Developer PowerShell for VS 2022" (or 2019):
windeployqt .\dist\LlamaChat\LlamaChat.exe --compiler-runtime
```

Or copy `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll` into the dist
folder manually, or install the VC++ x64 Redistributable on the target. Always
verify the three DLLs are present in `dist\LlamaChat\` before transferring to
the airgapped machine.

---

## 8. Lean distribution & VRAM budget (shared GPU)

The target also runs GPU compositing, so the grammar tool should sip resources —
but keep it responsive. The defaults favour **speed**; switch to idle-unload only
if you need the VRAM back.

**Multiple outputs.** `num_outputs` (default 3) generates that many candidates in
parallel — each shown in its own "Option" card with a Copy button, so you pick
the best. The app runs one request per card with a distinct seed; llama-server is
launched with `--parallel <num_outputs>` and its context scaled up
(`ctx_size * num_outputs`) so the candidates don't queue. Set `num_outputs: 1`
for a single fast answer.

**Model residency (speed vs VRAM).** Controlled by `idle_unload_seconds`:

| value | behaviour                                                          |
|-------|--------------------------------------------------------------------|
| `0`   | **default** — model preloads at startup and stays resident (fast)  |
| `300` | lazy-load; unload 5 min after the last refine, reload on next use   |
| `60`  | aggressive; frees VRAM quickly between uses (adds reload latency)   |

At `0` the model loads once and every refine is instant (this is the responsive
behaviour). Any value `>0` trades that for freeing memory while idle, at the cost
of a few seconds' reload — use it only when compositing needs the VRAM.

**CPU-only build (zero VRAM + smallest download).** If compositing needs every
GB of VRAM, run inference on the CPU and skip CUDA entirely:

1. Build llama.cpp **without** CUDA (omit `-DGGML_CUDA=ON`):
   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
   This produces no `ggml-cuda.dll` and needs no CUDA runtime DLLs, so the dist
   folder drops ~0.5-1 GB and there's nothing CUDA-related to stage.
2. Set `"n_gpu_layers": 0` in `config.json`.
3. `package.bat` then reports `Mode: CPU-only` and a smaller size.

Inference is slower on CPU but fine for short text. `package.ps1` auto-detects
which mode you built (it looks for `cudart64_*.dll`) and reports it.

**Inference flags (smaller GPU footprint).** `config.json` ships with
`"extra_args": ["-fa", "-ctk", "q8_0", "-ctv", "q8_0"]` — flash attention plus an
8-bit KV cache, which cuts the per-context VRAM use. Run `llama-server --help`
once on the box to confirm these flag names for your build; remove them if the
model complains.

**Model size.** Smaller = less memory and a smaller transfer. For grammar a 1.5-4B
model is plenty. Mind the license: Phi-3.5-mini (3.8B) is MIT; Qwen2.5 is Apache
at **1.5B / 7B** but a **non-commercial research** licence at **3B** — avoid 3B
for open-source-only use.

**What package.ps1 trims.** It runs windeployqt with `--no-opengl-sw` (drops the
~20 MB software-GL fallback), bundles the MSVC runtime from System32, and deletes
the `imageformats` / `iconengines` / `generic` plugin folders plus `Qt6Pdf.dll` /
`Qt6Svg.dll` (a plain text app uses none of them). If you ever run the app over
**RDP** or in a VM without GPU OpenGL, drop the `--no-opengl-sw` flag — that
fallback is what makes Qt render without a GPU.
