#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/class_db.hpp>

// LiveKit C++ SDK headers (livekit-client-sdk-cpp)
#include "livekit/room.h"
#include "livekit/participant.h"
#include "livekit/track.h"
#include "livekit/track_publication.h"
#include "livekit/audio_track.h"
#include "livekit/video_track.h"
#include "livekit/rtc_engine.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

namespace godot {

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
class LiveKitParticipant;
class LiveKitTrack;

// ─────────────────────────────────────────────
//  Connection state enum (mirrors Godot-style)
// ─────────────────────────────────────────────
enum LiveKitConnectionState {
    STATE_DISCONNECTED = 0,
    STATE_CONNECTING   = 1,
    STATE_CONNECTED    = 2,
    STATE_RECONNECTING = 3,
    STATE_FAILED       = 4,
};

// ─────────────────────────────────────────────
//  LiveKitRoom — main GDExtension node
// ─────────────────────────────────────────────
class LiveKitRoom : public Node {
    GDCLASS(LiveKitRoom, Node)

public:
    // ── Lifecycle ──────────────────────────────
    LiveKitRoom();
    ~LiveKitRoom();

    // ── Godot virtual overrides ────────────────
    void _ready()   override;
    void _process(double delta) override;
    void _exit_tree() override;

    // ── Connection API ─────────────────────────
    void connect_to_room(const String& url, const String& token);
    void disconnect_from_room();
    bool is_connected_to_room() const;
    int  get_connection_state() const;

    // ── Publishing ─────────────────────────────
    void enable_microphone(bool enabled);
    void enable_camera(bool enabled);
    void publish_data(const PackedByteArray& data, bool reliable = true);

    // ── Room info ──────────────────────────────
    String get_room_name() const;
    String get_local_participant_identity() const;
    int    get_participant_count() const;
    Array  get_participant_identities() const;

    // ── Audio settings ─────────────────────────
    void set_audio_output_gain(float gain);
    float get_audio_output_gain() const;

    // ── Video texture access ───────────────────
    // Returns a Godot ImageTexture that mirrors a remote video track
    Ref<ImageTexture> get_participant_video_texture(const String& identity);

    // ── Properties (exported to inspector) ─────
    void set_auto_subscribe(bool value);
    bool get_auto_subscribe() const;

    void set_reconnect_attempts(int value);
    int  get_reconnect_attempts() const;

protected:
    static void _bind_methods();

private:
    // ── LiveKit SDK internals ──────────────────
    std::unique_ptr<livekit::Room>       _room;
    std::unique_ptr<livekit::RtcEngine>  _engine;

    // ── State ──────────────────────────────────
    LiveKitConnectionState _state     = STATE_DISCONNECTED;
    bool  _auto_subscribe             = true;
    int   _reconnect_attempts         = 3;
    float _audio_gain                 = 1.0f;
    bool  _mic_enabled                = false;
    bool  _cam_enabled                = false;

    // ── Thread-safe event queue ────────────────
    // Events are queued from LiveKit callbacks (worker thread)
    // and dispatched on the Godot main thread inside _process().
    struct RoomEvent {
        enum Type {
            CONNECTED, DISCONNECTED, RECONNECTING, FAILED,
            PARTICIPANT_JOINED, PARTICIPANT_LEFT,
            TRACK_SUBSCRIBED, TRACK_UNSUBSCRIBED,
            DATA_RECEIVED, SPEAKING_CHANGED,
        } type;
        String identity;
        String track_sid;
        PackedByteArray data;
        bool   speaking = false;
    };

    std::vector<RoomEvent> _event_queue;
    std::mutex             _queue_mutex;

    void _enqueue_event(RoomEvent event);
    void _flush_events();

    // ── LiveKit callback registration ──────────
    void _setup_room_callbacks();

    // ── Audio mixing helper ────────────────────
    // Holds per-participant audio buffers rendered from LiveKit audio tracks
    struct RemoteAudio {
        std::shared_ptr<livekit::RemoteAudioTrack> track;
        Ref<AudioStreamGeneratorPlayback>           playback;
        Ref<AudioStreamPlayer>                      player;
    };
    std::unordered_map<std::string, RemoteAudio> _remote_audio;

    void _attach_remote_audio(const String& identity,
                               std::shared_ptr<livekit::RemoteAudioTrack> track);
    void _detach_remote_audio(const String& identity);
    void _pump_audio_frames();     // called every _process tick

    // ── Video frame cache ──────────────────────
    struct RemoteVideo {
        std::shared_ptr<livekit::RemoteVideoTrack> track;
        Ref<ImageTexture>                           texture;
    };
    std::unordered_map<std::string, RemoteVideo> _remote_video;

    void _attach_remote_video(const String& identity,
                               std::shared_ptr<livekit::RemoteVideoTrack> track);
    void _update_video_frames();   // called every _process tick
};

} // namespace godot
