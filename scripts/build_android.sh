#!/usr/bin/env bash
# build_android.sh — Build LiveKit GDExtension for Android (arm64-v8a + x86_64)
#
# Prerequisites:
#   - Android NDK r25c+  (set ANDROID_NDK_ROOT)
#   - Compiled LiveKit C++ SDK for Android (set LIVEKIT_SDK)
#   - Python 3 + SCons  (pip install scons)
#   - godot-cpp submodule initialised
#
# Usage:
#   export ANDROID_NDK_ROOT=/path/to/ndk
#   export LIVEKIT_SDK=/path/to/livekit-sdk
#   bash build_android.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

: "${ANDROID_NDK_ROOT:?Please set ANDROID_NDK_ROOT}"
: "${LIVEKIT_SDK:?Please set LIVEKIT_SDK}"

GODOT_CPP_DIR="$SCRIPT_DIR/godot-cpp"
PYTHON="python3"
SCONS="scons"

# ── Ensure godot-cpp is initialised ─────────────────────────────────────────
if [ ! -f "$GODOT_CPP_DIR/SConstruct" ]; then
    echo "[!] godot-cpp not found at $GODOT_CPP_DIR"
    echo "    Run:  git submodule update --init --recursive"
    exit 1
fi

ARCHS=("arm64v8" "x86_64")

for ARCH in "${ARCHS[@]}"; do
    echo ""
    echo "══════════════════════════════════════════════"
    echo "  Building android / $ARCH / template_release"
    echo "══════════════════════════════════════════════"

    $SCONS \
        platform=android \
        target=template_release \
        android_arch="$ARCH" \
        ANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
        LIVEKIT_SDK="$LIVEKIT_SDK" \
        -j"$(nproc)"
done

echo ""
echo "✅  Android build complete."
echo "    Libraries in: godot_project/addons/livekit/bin/"
