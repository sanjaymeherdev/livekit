# third_party/

This directory is **not committed to git**.

CI fetches and builds the LiveKit C++ SDK here automatically.

To build locally:

```bash
git clone https://github.com/livekit/client-sdk-cpp livekit-sdk
cd livekit-sdk

# Desktop
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../livekit-sdk-build
cmake --build build --parallel
cmake --install build

# Android arm64-v8a
cmake -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=../livekit-sdk-build/android/arm64-v8a
cmake --build build-arm64 --parallel
cmake --install build-arm64
```

Then set `LIVEKIT_SDK` when building the extension:

```bash
export LIVEKIT_SDK=$PWD/../livekit-sdk-build
scons platform=linux target=template_release
```
