#!/usr/bin/env bash
# Assemble a portable dist/LlamaChat folder on Linux: the binary, bundled Qt +
# CUDA shared libs (gathered via ldd), Qt plugins, the staged llama runtime, the
# model, a qt.conf and a run.sh launcher that sets LD_LIBRARY_PATH. Mirrors
# scripts/package.ps1. Run scripts/build.sh and scripts/fetch-llama.sh first.
#
# Env:
#   QT_DIR   Qt 6 prefix (for plugins). If unset, queried from qtpaths6/qmake6.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$REPO_ROOT/build"
DIST="$REPO_ROOT/dist/LlamaChat"
EXE="$BUILD/LlamaChat"

[[ -f "$EXE" ]] || { echo "ERROR: $EXE not found. Run scripts/build.sh first." >&2; exit 1; }

# --- locate the Qt install (for the plugins, which ldd cannot discover) --------
qt_prefix() {
    if [[ -n "${QT_DIR:-}" ]]; then echo "$QT_DIR"; return; fi
    for q in qtpaths6 qtpaths qmake6 qmake; do
        if command -v "$q" >/dev/null 2>&1; then
            "$q" -query QT_INSTALL_PREFIX 2>/dev/null && return
        fi
    done
}
QTP="$(qt_prefix || true)"
QT_PLUGINS=""
[[ -n "$QTP" && -d "$QTP/plugins" ]] && QT_PLUGINS="$QTP/plugins"
[[ -z "$QT_PLUGINS" ]] && echo "WARN: Qt plugins dir not found; set QT_DIR. The 'platforms/libqxcb.so' plugin is required to launch."

# --- libraries present on essentially every target: do NOT bundle these -------
# (glibc + loader + GPU driver + windowing-system libs expected on the host)
is_excluded() {
    case "$1" in
        ld-linux*|libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libresolv.so*|\
        libcuda.so*|libnvidia*|\
        libGL.so*|libGLX*|libEGL*|libOpenGL*|libGLdispatch*|\
        libX11*|libxcb*|libXext*|libXrender*|libXrandr*|libXi*|libXfixes*|libXcursor*|\
        libxkbcommon*|libwayland*|libdrm*|libgbm*|libdbus*) return 0 ;;
        *) return 1 ;;
    esac
}

mkdir -p "$DIST/lib" "$DIST/llama" "$DIST/models" "$DIST/plugins/platforms"
# clear contents in place (robust if a file manager holds the folder open)
find "$DIST" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
mkdir -p "$DIST/lib" "$DIST/llama" "$DIST/models" "$DIST/plugins/platforms"

cp -fv "$EXE" "$DIST/"
cp -fv "$REPO_ROOT/assets/config.json" "$DIST/"

# --- recursively bundle a binary's non-system shared-lib deps into lib/ --------
bundle_deps() {
    local target="$1"
    ldd "$target" 2>/dev/null | awk '/=>/ {print $3} !/=>/ {print $1}' | while read -r p; do
        [[ -f "$p" ]] || continue
        local name; name="$(basename "$p")"
        is_excluded "$name" && continue
        [[ -e "$DIST/lib/$name" ]] && continue
        cp -fL "$p" "$DIST/lib/$name"
    done
}

# --- stage the llama runtime (server + its .so + CUDA libs) --------------------
if compgen -G "$REPO_ROOT/runtime/llama/*" >/dev/null; then
    find "$REPO_ROOT/runtime/llama" -maxdepth 1 -type f ! -name 'README.txt' \
        -exec cp -fv {} "$DIST/llama/" \;
fi
[[ -x "$DIST/llama/llama-server" ]] && chmod +x "$DIST/llama/llama-server"

# --- the model ----------------------------------------------------------------
if compgen -G "$REPO_ROOT/runtime/models/*.gguf" >/dev/null; then
    cp -fv "$REPO_ROOT"/runtime/models/*.gguf "$DIST/models/"
fi

# --- gather Qt + other deps for both binaries ---------------------------------
bundle_deps "$DIST/LlamaChat"
[[ -f "$DIST/llama/llama-server" ]] && bundle_deps "$DIST/llama/llama-server"

# --- Qt plugins (loaded at runtime; ldd never sees them) ----------------------
if [[ -n "$QT_PLUGINS" ]]; then
    # the xcb platform plugin is mandatory; the rest are nice-to-have
    cp -fv "$QT_PLUGINS/platforms/libqxcb.so" "$DIST/plugins/platforms/" 2>/dev/null || \
        echo "WARN: libqxcb.so not found in $QT_PLUGINS/platforms"
    for grp in tls styles imageformats iconengines; do
        [[ -d "$QT_PLUGINS/$grp" ]] && { mkdir -p "$DIST/plugins/$grp"; cp -fv "$QT_PLUGINS/$grp"/*.so "$DIST/plugins/$grp/" 2>/dev/null || true; }
    done
    # pull the plugins' own deps into lib/
    while IFS= read -r so; do bundle_deps "$so"; done < <(find "$DIST/plugins" -name '*.so')
fi

# --- qt.conf so Qt finds the bundled plugins/libs relative to the exe ----------
cat > "$DIST/qt.conf" <<'EOF'
[Paths]
Prefix = .
Plugins = plugins
Libraries = lib
EOF

# --- launcher -----------------------------------------------------------------
cat > "$DIST/run.sh" <<'EOF'
#!/usr/bin/env bash
# Portable launcher: point the loader at the bundled Qt + CUDA + llama libs.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$DIR/llama:${LD_LIBRARY_PATH:-}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/plugins/platforms"
exec "$DIR/LlamaChat" "$@"
EOF
chmod +x "$DIST/run.sh" "$DIST/LlamaChat"

# --- summary ------------------------------------------------------------------
echo
echo ">> Portable folder ready: $DIST"
have_srv=$([[ -f "$DIST/llama/llama-server" ]] && echo yes || echo NO)
have_gguf=$(compgen -G "$DIST/models/*.gguf" >/dev/null && echo yes || echo NO)
have_cuda=$(compgen -G "$DIST/llama/libcudart.so*" >/dev/null && echo "GPU (CUDA libs bundled)" || echo "CPU-only (no CUDA libs)")
have_qxcb=$([[ -f "$DIST/plugins/platforms/libqxcb.so" ]] && echo yes || echo NO)
size=$(du -sh "$DIST" | cut -f1)
echo "   llama-server: $have_srv   model: $have_gguf   xcb plugin: $have_qxcb"
echo "   Mode: $have_cuda   Size: $size"
echo "   Launch with:  $DIST/run.sh"
[[ "$have_srv" == "NO" ]]  && echo "   WARN: no llama-server -- run scripts/fetch-llama.sh first."
[[ "$have_gguf" == "NO" ]] && echo "   WARN: no .gguf -- put a model in runtime/models/."
[[ "$have_qxcb" == "NO" ]] && echo "   WARN: no xcb platform plugin -- the GUI will not start without it."
