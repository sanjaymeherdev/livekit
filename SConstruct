#!/usr/bin/env python
"""
SConstruct — Godot GDExtension: LiveKit
Builds for: linux, windows, macos, android (arm64-v8a, x86_64)

Usage:
  scons platform=linux target=template_release
  scons platform=android android_arch=arm64v8 target=template_release
  scons platform=windows target=template_release
"""

import os, sys
from pathlib import Path

# ── Scons environment bootstrap ─────────────────────────────────────────────
env = SConscript("godot-cpp/SConstruct")   # inherits godot-cpp flags

# ── Project sources ──────────────────────────────────────────────────────────
env.Append(CPPPATH=["include/"])

sources = Glob("src/*.cpp")

# ── LiveKit C++ SDK paths ────────────────────────────────────────────────────
# Set LIVEKIT_SDK env var to the root of the compiled livekit-client-sdk-cpp
LIVEKIT_SDK = os.environ.get("LIVEKIT_SDK", "third_party/livekit-sdk")

env.Append(CPPPATH=[
    f"{LIVEKIT_SDK}/include",
    f"{LIVEKIT_SDK}/include/livekit",
    f"{LIVEKIT_SDK}/third_party/webrtc/include",
])

# ── Platform-specific linking ─────────────────────────────────────────────────
platform = env["platform"]

if platform == "android":
    arch = env.get("android_arch", "arm64v8")
    arch_dir = "arm64-v8a" if arch == "arm64v8" else "x86_64"

    env.Append(LIBPATH=[
        f"{LIVEKIT_SDK}/build/android/{arch_dir}",
    ])
    env.Append(LIBS=[
        "livekit_client",
        "webrtc",
        "log",        # Android log (liblog)
        "OpenSLES",   # Android audio
        "android",
        "EGL",
        "GLESv2",
    ])
    env.Append(CPPFLAGS=["-DLIVEKIT_ANDROID"])

elif platform in ("linux", "linuxbsd"):
    env.Append(LIBPATH=[f"{LIVEKIT_SDK}/build/linux"])
    env.Append(LIBS=["livekit_client", "webrtc", "pthread", "dl"])

elif platform == "windows":
    env.Append(LIBPATH=[f"{LIVEKIT_SDK}/build/windows"])
    env.Append(LIBS=["livekit_client", "webrtc", "ws2_32", "secur32", "msdmo"])

elif platform == "macos":
    env.Append(LIBPATH=[f"{LIVEKIT_SDK}/build/macos"])
    env.Append(LIBS=["livekit_client", "webrtc"])
    env.Append(FRAMEWORKS=["CoreAudio", "AVFoundation", "Foundation"])

# ── C++ standard ──────────────────────────────────────────────────────────────
env.Append(CXXFLAGS=["-std=c++17"])

# ── Output library ────────────────────────────────────────────────────────────
if env["platform"] == "macos":
    library = env.SharedLibrary(
        "godot_project/addons/livekit/bin/liblivekit.{}.{}.framework/liblivekit.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "godot_project/addons/livekit/bin/liblivekit{}{}".format(
            env["suffix"], env["SHLIBSUFFIX"]
        ),
        source=sources,
    )

Default(library)
