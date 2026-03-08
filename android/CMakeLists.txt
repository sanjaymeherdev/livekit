cmake_minimum_required(VERSION 3.22)
project(livekit_gdextension CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Godot-cpp (header-only for GDExtension ABI) ──────────────────────────────
# When building via SConstruct these are handled by godot-cpp's own SConstruct.
# This CMakeLists is provided as an alternative / Android Studio integration.
set(GODOT_CPP_DIR "${CMAKE_SOURCE_DIR}/../godot-cpp" CACHE PATH "Path to godot-cpp")

add_subdirectory(${GODOT_CPP_DIR} godot-cpp EXCLUDE_FROM_ALL)

# ── LiveKit SDK ───────────────────────────────────────────────────────────────
set(LIVEKIT_SDK "$ENV{LIVEKIT_SDK}" CACHE PATH "Path to LiveKit C++ SDK")
if(NOT LIVEKIT_SDK)
    set(LIVEKIT_SDK "${CMAKE_SOURCE_DIR}/../third_party/livekit-sdk")
endif()

# ── Extension shared library ──────────────────────────────────────────────────
add_library(livekit_gdextension SHARED
    src/livekit_room.cpp
    src/register_types.cpp
)

target_include_directories(livekit_gdextension PRIVATE
    include/
    ${LIVEKIT_SDK}/include
    ${LIVEKIT_SDK}/include/livekit
    ${LIVEKIT_SDK}/third_party/webrtc/include
)

target_link_libraries(livekit_gdextension
    godot-cpp

    # LiveKit + WebRTC (pre-built .a / .so for the target ABI)
    ${LIVEKIT_SDK}/build/android/${ANDROID_ABI}/liblivekit_client.a
    ${LIVEKIT_SDK}/build/android/${ANDROID_ABI}/libwebrtc.a

    # Android system libraries
    log
    android
    OpenSLES
    EGL
    GLESv2
)

target_compile_definitions(livekit_gdextension PRIVATE LIVEKIT_ANDROID)

# ── Strip symbols in release ──────────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(livekit_gdextension PRIVATE -O2 -fvisibility=hidden)
endif()
