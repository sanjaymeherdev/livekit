#include "livekit_room.h"

#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// ─────────────────────────────────────────────────────────────────────────────
//  _bind_methods  — expose everything to GDScript / C#
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::_bind_methods() {
    // ── Connection ──────────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("connect_to_room", "url", "token"),
                         &LiveKitRoom::connect_to_room);
    ClassDB::bind_method(D_METHOD("disconnect_from_room"),
                         &LiveKitRoom::disconnect_from_room);
    ClassDB::bind_method(D_METHOD("is_connected_to_room"),
                         &LiveKitRoom::is_connected_to_room);
    ClassDB::bind_method(D_METHOD("get_connection_state"),
                         &LiveKitRoom::get_connection_state);

    // ── Publishing ──────────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("enable_microphone", "enabled"),
                         &LiveKitRoom::enable_microphone);
    ClassDB::bind_method(D_METHOD("enable_camera", "enabled"),
                         &LiveKitRoom::enable_camera);
    ClassDB::bind_method(D_METHOD("publish_data", "data", "reliable"),
                         &LiveKitRoom::publish_data);

    // ── Room info ────────────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("get_room_name"),
                         &LiveKitRoom::get_room_name);
    ClassDB::bind_method(D_METHOD("get_local_participant_identity"),
                         &LiveKitRoom::get_local_participant_identity);
    ClassDB::bind_method(D_METHOD("get_participant_count"),
                         &LiveKitRoom::get_participant_count);
    ClassDB::bind_method(D_METHOD("get_participant_identities"),
                         &LiveKitRoom::get_participant_identities);

    // ── Audio / Video ────────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("set_audio_output_gain", "gain"),
                         &LiveKitRoom::set_audio_output_gain);
    ClassDB::bind_method(D_METHOD("get_audio_output_gain"),
                         &LiveKitRoom::get_audio_output_gain);
    ClassDB::bind_method(D_METHOD("get_participant_video_texture", "identity"),
                         &LiveKitRoom::get_participant_video_texture);

    // ── Properties ───────────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("set_auto_subscribe", "value"),
                         &LiveKitRoom::set_auto_subscribe);
    ClassDB::bind_method(D_METHOD("get_auto_subscribe"),
                         &LiveKitRoom::get_auto_subscribe);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_subscribe"),
                 "set_auto_subscribe", "get_auto_subscribe");

    ClassDB::bind_method(D_METHOD("set_reconnect_attempts", "value"),
                         &LiveKitRoom::set_reconnect_attempts);
    ClassDB::bind_method(D_METHOD("get_reconnect_attempts"),
                         &LiveKitRoom::get_reconnect_attempts);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "reconnect_attempts",
                              PROPERTY_HINT_RANGE, "0,10,1"),
                 "set_reconnect_attempts", "get_reconnect_attempts");

    // ── Signals ──────────────────────────────────────────────────────────────
    ADD_SIGNAL(MethodInfo("connected"));
    ADD_SIGNAL(MethodInfo("disconnected",
        PropertyInfo(Variant::STRING, "reason")));
    ADD_SIGNAL(MethodInfo("reconnecting"));
    ADD_SIGNAL(MethodInfo("connection_failed",
        PropertyInfo(Variant::STRING, "reason")));

    ADD_SIGNAL(MethodInfo("participant_joined",
        PropertyInfo(Variant::STRING, "identity")));
    ADD_SIGNAL(MethodInfo("participant_left",
        PropertyInfo(Variant::STRING, "identity")));

    ADD_SIGNAL(MethodInfo("track_subscribed",
        PropertyInfo(Variant::STRING, "identity"),
        PropertyInfo(Variant::STRING, "track_sid"),
        PropertyInfo(Variant::STRING, "kind")));   // "audio" | "video" | "data"
    ADD_SIGNAL(MethodInfo("track_unsubscribed",
        PropertyInfo(Variant::STRING, "identity"),
        PropertyInfo(Variant::STRING, "track_sid")));

    ADD_SIGNAL(MethodInfo("data_received",
        PropertyInfo(Variant::STRING, "sender_identity"),
        PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"),
        PropertyInfo(Variant::BOOL, "reliable")));

    ADD_SIGNAL(MethodInfo("speaking_changed",
        PropertyInfo(Variant::STRING, "identity"),
        PropertyInfo(Variant::BOOL, "speaking")));

    // ── Enum constants ────────────────────────────────────────────────────────
    BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
    BIND_ENUM_CONSTANT(STATE_CONNECTING);
    BIND_ENUM_CONSTANT(STATE_CONNECTED);
    BIND_ENUM_CONSTANT(STATE_RECONNECTING);
    BIND_ENUM_CONSTANT(STATE_FAILED);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
LiveKitRoom::LiveKitRoom() = default;

LiveKitRoom::~LiveKitRoom() {
    disconnect_from_room();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Godot overrides
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::_ready() {
    // Nothing to do until connect_to_room() is called.
    // Engine::get_singleton()->is_editor_hint() guard prevents running in editor.
    if (Engine::get_singleton()->is_editor_hint()) return;

    // Initialise LiveKit RTC engine (sets up WebRTC thread pool, SSL, etc.)
    livekit::RtcEngine::Options eng_opts;
    eng_opts.field_trials = "";   // optional performance field trials
    _engine = std::make_unique<livekit::RtcEngine>(eng_opts);
}

void LiveKitRoom::_process(double /*delta*/) {
    if (Engine::get_singleton()->is_editor_hint()) return;

    _flush_events();      // dispatch queued LiveKit callbacks → Godot signals
    _pump_audio_frames(); // pull PCM from LiveKit audio tracks → AudioStreamPlayer
    _update_video_frames(); // copy latest video frame → ImageTexture
}

void LiveKitRoom::_exit_tree() {
    disconnect_from_room();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Connection
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::connect_to_room(const String& p_url, const String& p_token) {
    if (_state == STATE_CONNECTING || _state == STATE_CONNECTED) {
        UtilityFunctions::push_warning("LiveKitRoom: already connected/connecting");
        return;
    }

    _state = STATE_CONNECTING;

    // Build room options
    livekit::Room::Options room_opts;
    room_opts.auto_subscribe          = _auto_subscribe;
    room_opts.reconnect_policy.max_retry_attempts = _reconnect_attempts;

    _room = std::make_unique<livekit::Room>(_engine.get(), room_opts);
    _setup_room_callbacks();

    // Connect asynchronously; result arrives via callbacks
    _room->connect(
        std::string(p_url.utf8().get_data()),
        std::string(p_token.utf8().get_data())
    );
}

void LiveKitRoom::disconnect_from_room() {
    if (_room) {
        _room->disconnect();
        _room.reset();
    }
    // Clean up audio/video resources
    _remote_audio.clear();
    _remote_video.clear();
    _state = STATE_DISCONNECTED;
}

bool LiveKitRoom::is_connected_to_room() const {
    return _state == STATE_CONNECTED;
}

int LiveKitRoom::get_connection_state() const {
    return static_cast<int>(_state);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Publishing
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::enable_microphone(bool p_enabled) {
    _mic_enabled = p_enabled;
    if (!_room) return;

    auto local = _room->local_participant();
    if (!local) return;

    if (p_enabled) {
        livekit::AudioCaptureOptions opts;
        opts.echo_cancellation  = true;
        opts.noise_suppression  = true;
        opts.auto_gain_control  = true;
        local->set_microphone_enabled(true, opts);
    } else {
        local->set_microphone_enabled(false);
    }
}

void LiveKitRoom::enable_camera(bool p_enabled) {
    _cam_enabled = p_enabled;
    if (!_room) return;

    auto local = _room->local_participant();
    if (!local) return;

    livekit::VideoCaptureOptions opts;
    opts.resolution = livekit::VideoResolution::VGA;
    opts.fps        = 30;
    local->set_camera_enabled(p_enabled, opts);
}

void LiveKitRoom::publish_data(const PackedByteArray& p_data, bool p_reliable) {
    if (!_room || _state != STATE_CONNECTED) return;

    auto local = _room->local_participant();
    if (!local) return;

    livekit::DataPublishOptions opts;
    opts.reliable = p_reliable;

    // Build std::vector<uint8_t> from Godot's PackedByteArray
    const uint8_t* ptr = p_data.ptr();
    std::vector<uint8_t> buf(ptr, ptr + p_data.size());

    local->publish_data(buf, opts);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Room info
// ─────────────────────────────────────────────────────────────────────────────
String LiveKitRoom::get_room_name() const {
    if (!_room) return "";
    return String(_room->name().c_str());
}

String LiveKitRoom::get_local_participant_identity() const {
    if (!_room) return "";
    auto local = _room->local_participant();
    if (!local) return "";
    return String(local->identity().c_str());
}

int LiveKitRoom::get_participant_count() const {
    if (!_room) return 0;
    return static_cast<int>(_room->remote_participants().size());
}

Array LiveKitRoom::get_participant_identities() const {
    Array result;
    if (!_room) return result;
    for (auto& [id, _] : _room->remote_participants()) {
        result.push_back(String(id.c_str()));
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Audio / Video
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::set_audio_output_gain(float p_gain) {
    _audio_gain = p_gain;
    for (auto& [_, ra] : _remote_audio) {
        if (ra.player.is_valid())
            ra.player->set_volume_db(Math::linear_to_db(p_gain));
    }
}

float LiveKitRoom::get_audio_output_gain() const { return _audio_gain; }

Ref<ImageTexture> LiveKitRoom::get_participant_video_texture(const String& p_identity) {
    auto it = _remote_video.find(std::string(p_identity.utf8().get_data()));
    if (it == _remote_video.end()) return Ref<ImageTexture>();
    return it->second.texture;
}

// Properties
void LiveKitRoom::set_auto_subscribe(bool v)    { _auto_subscribe = v; }
bool LiveKitRoom::get_auto_subscribe() const    { return _auto_subscribe; }
void LiveKitRoom::set_reconnect_attempts(int v) { _reconnect_attempts = v; }
int  LiveKitRoom::get_reconnect_attempts() const{ return _reconnect_attempts; }

// ─────────────────────────────────────────────────────────────────────────────
//  LiveKit callbacks — run on LiveKit worker thread; enqueue for Godot thread
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::_setup_room_callbacks() {
    if (!_room) return;

    _room->on_connected([this]() {
        _enqueue_event({RoomEvent::CONNECTED});
    });

    _room->on_disconnected([this](const std::string& reason) {
        RoomEvent ev{RoomEvent::DISCONNECTED};
        ev.identity = String(reason.c_str());
        _enqueue_event(ev);
    });

    _room->on_reconnecting([this]() {
        _enqueue_event({RoomEvent::RECONNECTING});
    });

    _room->on_connection_failed([this](const std::string& reason) {
        RoomEvent ev{RoomEvent::FAILED};
        ev.identity = String(reason.c_str());
        _enqueue_event(ev);
    });

    _room->on_participant_connected([this](std::shared_ptr<livekit::RemoteParticipant> p) {
        RoomEvent ev{RoomEvent::PARTICIPANT_JOINED};
        ev.identity = String(p->identity().c_str());
        _enqueue_event(ev);
    });

    _room->on_participant_disconnected([this](std::shared_ptr<livekit::RemoteParticipant> p) {
        RoomEvent ev{RoomEvent::PARTICIPANT_LEFT};
        ev.identity = String(p->identity().c_str());
        _enqueue_event(ev);
    });

    _room->on_track_subscribed([this](
        std::shared_ptr<livekit::RemoteTrack>         track,
        std::shared_ptr<livekit::RemoteTrackPublication> /*pub*/,
        std::shared_ptr<livekit::RemoteParticipant>   participant)
    {
        RoomEvent ev{RoomEvent::TRACK_SUBSCRIBED};
        ev.identity  = String(participant->identity().c_str());
        ev.track_sid = String(track->sid().c_str());

        // Wire up audio/video immediately (safe to call from any thread)
        if (track->kind() == livekit::TrackKind::AUDIO) {
            auto audio = std::static_pointer_cast<livekit::RemoteAudioTrack>(track);
            _attach_remote_audio(ev.identity, audio);
            ev.data.resize(5); // reuse data field as kind tag: "audio"
        } else if (track->kind() == livekit::TrackKind::VIDEO) {
            auto video = std::static_pointer_cast<livekit::RemoteVideoTrack>(track);
            _attach_remote_video(ev.identity, video);
        }
        _enqueue_event(ev);
    });

    _room->on_track_unsubscribed([this](
        std::shared_ptr<livekit::RemoteTrack>       track,
        std::shared_ptr<livekit::RemoteParticipant> participant)
    {
        RoomEvent ev{RoomEvent::TRACK_UNSUBSCRIBED};
        ev.identity  = String(participant->identity().c_str());
        ev.track_sid = String(track->sid().c_str());

        std::string id(participant->identity());
        _detach_remote_audio(ev.identity);
        _remote_video.erase(id);

        _enqueue_event(ev);
    });

    _room->on_data_received([this](
        const std::vector<uint8_t>&                data,
        std::shared_ptr<livekit::RemoteParticipant> sender,
        bool                                        reliable)
    {
        RoomEvent ev{RoomEvent::DATA_RECEIVED};
        ev.identity = String(sender ? sender->identity().c_str() : "");
        ev.data.resize(static_cast<int64_t>(data.size()));
        memcpy(ev.data.ptrw(), data.data(), data.size());
        ev.speaking = reliable; // reuse field
        _enqueue_event(ev);
    });

    _room->on_active_speakers_changed([this](
        const std::vector<std::shared_ptr<livekit::Participant>>& speakers)
    {
        // Emit per-participant speaking_changed events
        std::unordered_map<std::string, bool> speaking_map;
        for (auto& sp : speakers)
            speaking_map[sp->identity()] = true;

        for (auto& [id, _] : _room->remote_participants()) {
            bool is_speaking = speaking_map.count(id) > 0;
            RoomEvent ev{RoomEvent::SPEAKING_CHANGED};
            ev.identity = String(id.c_str());
            ev.speaking = is_speaking;
            _enqueue_event(ev);
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Event queue helpers
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::_enqueue_event(RoomEvent ev) {
    std::lock_guard<std::mutex> lock(_queue_mutex);
    _event_queue.push_back(std::move(ev));
}

void LiveKitRoom::_flush_events() {
    std::vector<RoomEvent> local;
    {
        std::lock_guard<std::mutex> lock(_queue_mutex);
        local.swap(_event_queue);
    }

    for (auto& ev : local) {
        switch (ev.type) {
            case RoomEvent::CONNECTED:
                _state = STATE_CONNECTED;
                emit_signal("connected");
                break;

            case RoomEvent::DISCONNECTED:
                _state = STATE_DISCONNECTED;
                emit_signal("disconnected", ev.identity);
                break;

            case RoomEvent::RECONNECTING:
                _state = STATE_RECONNECTING;
                emit_signal("reconnecting");
                break;

            case RoomEvent::FAILED:
                _state = STATE_FAILED;
                emit_signal("connection_failed", ev.identity);
                break;

            case RoomEvent::PARTICIPANT_JOINED:
                emit_signal("participant_joined", ev.identity);
                break;

            case RoomEvent::PARTICIPANT_LEFT:
                emit_signal("participant_left", ev.identity);
                break;

            case RoomEvent::TRACK_SUBSCRIBED: {
                String kind = "video";
                if (ev.data.size() == 5) kind = "audio";  // tag from callback
                emit_signal("track_subscribed", ev.identity, ev.track_sid, kind);
                break;
            }

            case RoomEvent::TRACK_UNSUBSCRIBED:
                emit_signal("track_unsubscribed", ev.identity, ev.track_sid);
                break;

            case RoomEvent::DATA_RECEIVED:
                emit_signal("data_received", ev.identity, ev.data, ev.speaking);
                break;

            case RoomEvent::SPEAKING_CHANGED:
                emit_signal("speaking_changed", ev.identity, ev.speaking);
                break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Audio integration — LiveKit PCM → Godot AudioStreamGenerator
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::_attach_remote_audio(
    const String& p_identity,
    std::shared_ptr<livekit::RemoteAudioTrack> p_track)
{
    std::string id(p_identity.utf8().get_data());
    if (_remote_audio.count(id)) return; // already attached

    // Create an AudioStreamPlayer with a generator stream
    auto player = memnew(AudioStreamPlayer);
    player->set_name(String("_lk_audio_") + p_identity);
    add_child(player);

    Ref<AudioStreamGenerator> gen;
    gen.instantiate();
    gen->set_mix_rate(48000.0f);  // LiveKit default sample rate
    gen->set_buffer_length(0.1f);
    player->set_stream(gen);
    player->set_volume_db(Math::linear_to_db(_audio_gain));
    player->play();

    auto playback = dynamic_cast<AudioStreamGeneratorPlayback*>(
        player->get_stream_playback().ptr());

    RemoteAudio ra;
    ra.track    = p_track;
    ra.player   = Ref<AudioStreamPlayer>(player);
    ra.playback = Ref<AudioStreamGeneratorPlayback>(playback);

    _remote_audio[id] = std::move(ra);
}

void LiveKitRoom::_detach_remote_audio(const String& p_identity) {
    std::string id(p_identity.utf8().get_data());
    auto it = _remote_audio.find(id);
    if (it == _remote_audio.end()) return;

    if (it->second.player.is_valid())
        it->second.player->queue_free();

    _remote_audio.erase(it);
}

void LiveKitRoom::_pump_audio_frames() {
    // Pull audio from each remote track and push into Godot's audio generator
    for (auto& [id, ra] : _remote_audio) {
        if (!ra.track || !ra.playback.is_valid()) continue;

        // How many stereo frames can the buffer accept?
        int frames_available = ra.playback->get_frames_available();
        if (frames_available <= 0) continue;

        // Pull PCM from LiveKit audio track (interleaved float stereo @ 48kHz)
        std::vector<float> pcm(static_cast<size_t>(frames_available) * 2);
        int frames_filled = ra.track->pull_pcm_data(pcm.data(), frames_available);
        if (frames_filled <= 0) continue;

        // Convert to Godot PackedVector2Array (L/R stereo)
        PackedVector2Array buf;
        buf.resize(frames_filled);
        Vector2* dst = buf.ptrw();
        for (int i = 0; i < frames_filled; ++i) {
            dst[i] = Vector2(pcm[i * 2], pcm[i * 2 + 1]);
        }
        ra.playback->push_buffer(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Video integration — LiveKit VideoFrame → Godot ImageTexture
// ─────────────────────────────────────────────────────────────────────────────
void LiveKitRoom::_attach_remote_video(
    const String& p_identity,
    std::shared_ptr<livekit::RemoteVideoTrack> p_track)
{
    std::string id(p_identity.utf8().get_data());
    if (_remote_video.count(id)) return;

    RemoteVideo rv;
    rv.track   = p_track;
    rv.texture.instantiate();
    _remote_video[id] = std::move(rv);
}

void LiveKitRoom::_update_video_frames() {
    for (auto& [id, rv] : _remote_video) {
        if (!rv.track || !rv.texture.is_valid()) continue;

        livekit::VideoFrame frame;
        if (!rv.track->get_latest_frame(&frame)) continue;
        if (!frame.data || frame.width == 0 || frame.height == 0) continue;

        // LiveKit delivers I420 by default; convert to RGBA for Godot
        int w = frame.width, h = frame.height;
        PackedByteArray rgba;
        rgba.resize(w * h * 4);
        uint8_t* dst = rgba.ptrw();

        // I420 → RGBA conversion (BT.601 full-swing)
        const uint8_t* y_plane  = frame.data;
        const uint8_t* u_plane  = y_plane + w * h;
        const uint8_t* v_plane  = u_plane + (w / 2) * (h / 2);

        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                int y = y_plane[row * w + col];
                int u = u_plane[(row / 2) * (w / 2) + col / 2] - 128;
                int v = v_plane[(row / 2) * (w / 2) + col / 2] - 128;

                int r = Math::clamp(y + (351 * v >> 8), 0, 255);
                int g = Math::clamp(y - (86 * u >> 8) - (179 * v >> 8), 0, 255);
                int b = Math::clamp(y + (444 * u >> 8), 0, 255);

                int idx = (row * w + col) * 4;
                dst[idx]     = static_cast<uint8_t>(r);
                dst[idx + 1] = static_cast<uint8_t>(g);
                dst[idx + 2] = static_cast<uint8_t>(b);
                dst[idx + 3] = 255;
            }
        }

        Ref<Image> img = Image::create_from_data(w, h, false, Image::FORMAT_RGBA8, rgba);
        rv.texture->set_image(img);
    }
}

} // namespace godot
