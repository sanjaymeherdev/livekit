# Godot 4 LiveKit GDExtension

Native C++ GDExtension integrating [LiveKit](https://livekit.io) real-time audio/video into Godot 4, with full Android support via the NDK.

## Architecture

```
Godot Scene
    └── LiveKitRoom (GDExtension Node)
            │
            ├── LiveKit C++ SDK  ←── WebRTC (libwebrtc)
            │       ├── Room / Participants / Tracks
            │       └── RtcEngine (WebRTC thread pool)
            │
            ├── Audio Path
            │       LiveKit RemoteAudioTrack.pull_pcm_data()
            │           → AudioStreamGeneratorPlayback (per participant)
            │               → Godot audio bus
            │
            └── Video Path
                    LiveKit RemoteVideoTrack.get_latest_frame() [I420]
                        → CPU I420→RGBA conversion
                            → ImageTexture (per participant)
                                → TextureRect / Sprite2D / MeshInstance3D
```

## Prerequisites

| Tool | Version |
|---|---|
| Godot | 4.2+ |
| godot-cpp | matching Godot version |
| Android NDK | r25c+ |
| CMake | 3.22+ |
| Python + SCons | 3.x / 4.x |
| LiveKit C++ SDK | built for your targets |

## Project Structure

```
godot-livekit-gdextension/
├── include/
│   ├── livekit_room.h        ← GDExtension class declaration
│   └── register_types.h
├── src/
│   ├── livekit_room.cpp      ← Full implementation
│   └── register_types.cpp    ← GDExtension entry point
├── android/
│   ├── CMakeLists.txt        ← Android Studio / NDK build
│   ├── build.gradle          ← Gradle plugin fragment
│   └── AndroidManifest.xml   ← Required permissions
├── godot_project/
│   ├── addons/livekit/
│   │   ├── livekit.gdextension   ← Godot manifest
│   │   └── bin/                  ← compiled .so / .dll / .framework
│   └── LiveKitVoiceChat.gd       ← GDScript usage example
├── scripts/
│   └── build_android.sh      ← One-command Android build
├── SConstruct                ← Cross-platform build script
└── README.md
```

## Setup

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/yourname/godot-livekit-gdextension
# or after cloning:
git submodule update --init --recursive
```

### 2. Build the LiveKit C++ SDK

```bash
git clone https://github.com/livekit/client-sdk-cpp third_party/livekit-sdk
cd third_party/livekit-sdk

# Desktop (Linux example)
cmake -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j$(nproc)

# Android arm64-v8a
cmake -B build/android/arm64-v8a \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/android/arm64-v8a -j$(nproc)
```

### 3. Build the GDExtension

#### Desktop

```bash
export LIVEKIT_SDK=third_party/livekit-sdk

# Linux release
scons platform=linux target=template_release

# Windows release (cross-compile or on Windows)
scons platform=windows target=template_release

# macOS
scons platform=macos target=template_release
```

#### Android

```bash
export ANDROID_NDK_ROOT=/path/to/ndk
export LIVEKIT_SDK=third_party/livekit-sdk
bash scripts/build_android.sh
```

Compiled libraries land in `godot_project/addons/livekit/bin/`.

## Usage in Godot

### 1. Copy the addon

Copy `godot_project/addons/livekit/` into your Godot project's `addons/` folder.

### 2. Enable in Project Settings

*Project → Project Settings → Plugins → LiveKit → Enable*

### 3. GDScript

```gdscript
extends Node

var lk: LiveKitRoom

func _ready():
    lk = LiveKitRoom.new()
    lk.auto_subscribe = true
    add_child(lk)

    lk.connected.connect(func(): print("connected!"))
    lk.participant_joined.connect(func(id): print(id, " joined"))
    lk.data_received.connect(_on_data)

    lk.connect_to_room("wss://my-server.livekit.cloud", "TOKEN")

func _on_data(sender: String, data: PackedByteArray, reliable: bool):
    print(sender, ": ", data.get_string_from_utf8())

func send_msg(text: String):
    lk.publish_data(text.to_utf8_buffer(), true)
```

### 4. Displaying remote video

```gdscript
func _on_track_subscribed(identity: String, _sid: String, kind: String):
    if kind != "video": return
    $RemoteVideoRect.texture = lk.get_participant_video_texture(identity)
    # The texture auto-updates every frame — no further polling needed.
```

## API Reference

### Properties

| Property | Type | Default | Description |
|---|---|---|---|
| `auto_subscribe` | `bool` | `true` | Auto-subscribe to remote tracks |
| `reconnect_attempts` | `int` | `3` | Max reconnect retries |

### Methods

| Method | Returns | Description |
|---|---|---|
| `connect_to_room(url, token)` | `void` | Connect to a LiveKit room |
| `disconnect_from_room()` | `void` | Leave the room |
| `is_connected_to_room()` | `bool` | True if STATE_CONNECTED |
| `get_connection_state()` | `int` | See enum below |
| `enable_microphone(bool)` | `void` | Publish/mute local mic |
| `enable_camera(bool)` | `void` | Publish/stop local camera |
| `publish_data(bytes, reliable)` | `void` | Send raw data to all peers |
| `get_room_name()` | `String` | Name of the current room |
| `get_local_participant_identity()` | `String` | Local participant ID |
| `get_participant_count()` | `int` | Number of remote participants |
| `get_participant_identities()` | `Array[String]` | All remote identity strings |
| `set_audio_output_gain(float)` | `void` | Master volume for remote audio |
| `get_participant_video_texture(id)` | `ImageTexture` | Live video texture |

### Signals

| Signal | Arguments | Description |
|---|---|---|
| `connected` | — | Room joined successfully |
| `disconnected` | `reason: String` | Cleanly disconnected |
| `reconnecting` | — | Attempting reconnect |
| `connection_failed` | `reason: String` | Fatal connection error |
| `participant_joined` | `identity: String` | Remote participant entered |
| `participant_left` | `identity: String` | Remote participant left |
| `track_subscribed` | `identity, track_sid, kind` | New track available (`"audio"\|"video"`) |
| `track_unsubscribed` | `identity, track_sid` | Track removed |
| `data_received` | `sender, data: PackedByteArray, reliable: bool` | Peer data message |
| `speaking_changed` | `identity, speaking: bool` | VAD speaking state |

### Enum `LiveKitConnectionState`

```gdscript
LiveKitRoom.STATE_DISCONNECTED  # 0
LiveKitRoom.STATE_CONNECTING    # 1
LiveKitRoom.STATE_CONNECTED     # 2
LiveKitRoom.STATE_RECONNECTING  # 3
LiveKitRoom.STATE_FAILED        # 4
```

## Android Notes

- Minimum API level: **26** (Android 8.0) — required by LiveKit's WebRTC build.
- Permissions are declared in the manifest fragment and auto-merged by Godot.
- For **Android 14+**, mic/camera capture requires a foreground service —  
  the `LiveKitForegroundService` stub in the manifest handles this.
- Request `RECORD_AUDIO` at runtime before calling `enable_microphone(true)`.

```gdscript
# Runtime permission request (required on Android 6+)
if OS.get_name() == "Android":
    OS.request_permission("RECORD_AUDIO")
```

## License

MIT — see LICENSE file.
