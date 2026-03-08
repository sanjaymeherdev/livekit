extends Node

## LiveKitVoiceChat.gd
## Minimal GDScript demo showing how to use LiveKitRoom from GDScript.
## Attach this script to any Node in your scene.

# ── Configuration ────────────────────────────────────────────────────────────
@export var server_url : String = "wss://your-livekit-server.example.com"
@export var auth_token : String = ""   # generate server-side with livekit-server-sdk

# ── Internal references ───────────────────────────────────────────────────────
var _lk : LiveKitRoom = null


func _ready() -> void:
	# Create the LiveKitRoom node and add it as a child so _process fires
	_lk = LiveKitRoom.new()
	_lk.auto_subscribe    = true
	_lk.reconnect_attempts = 3
	add_child(_lk)

	# Wire up all signals
	_lk.connected.connect(_on_connected)
	_lk.disconnected.connect(_on_disconnected)
	_lk.reconnecting.connect(_on_reconnecting)
	_lk.connection_failed.connect(_on_connection_failed)
	_lk.participant_joined.connect(_on_participant_joined)
	_lk.participant_left.connect(_on_participant_left)
	_lk.track_subscribed.connect(_on_track_subscribed)
	_lk.track_unsubscribed.connect(_on_track_unsubscribed)
	_lk.data_received.connect(_on_data_received)
	_lk.speaking_changed.connect(_on_speaking_changed)

	# Auto-connect if token is pre-set
	if auth_token != "":
		join_room()


# ── Public API ────────────────────────────────────────────────────────────────
func join_room() -> void:
	print("[LiveKit] Connecting to ", server_url)
	_lk.connect_to_room(server_url, auth_token)

func leave_room() -> void:
	_lk.disconnect_from_room()

func toggle_mic(enabled: bool) -> void:
	_lk.enable_microphone(enabled)

func send_game_event(event_dict: Dictionary) -> void:
	var json_bytes = JSON.stringify(event_dict).to_utf8_buffer()
	_lk.publish_data(json_bytes, true)   # reliable = ordered data channel


# ── Signal handlers ───────────────────────────────────────────────────────────
func _on_connected() -> void:
	print("[LiveKit] ✅ Connected  room='", _lk.get_room_name(),
		  "'  me='", _lk.get_local_participant_identity(), "'")
	_lk.enable_microphone(true)   # start mic immediately on connect


func _on_disconnected(reason: String) -> void:
	print("[LiveKit] ❌ Disconnected: ", reason)


func _on_reconnecting() -> void:
	print("[LiveKit] 🔄 Reconnecting…")


func _on_connection_failed(reason: String) -> void:
	push_error("[LiveKit] Connection failed: " + reason)


func _on_participant_joined(identity: String) -> void:
	print("[LiveKit] 👤 Joined: ", identity,
		  "  (total: ", _lk.get_participant_count(), ")")


func _on_participant_left(identity: String) -> void:
	print("[LiveKit] 👤 Left: ", identity)


func _on_track_subscribed(identity: String, track_sid: String, kind: String) -> void:
	print("[LiveKit] 🎵 Track subscribed  ", identity, " sid=", track_sid, " kind=", kind)

	if kind == "video":
		# Example: show remote video in a TextureRect called "RemoteVideo"
		var tex_rect := get_node_or_null("RemoteVideo") as TextureRect
		if tex_rect:
			# The texture updates automatically every _process frame
			tex_rect.texture = _lk.get_participant_video_texture(identity)


func _on_track_unsubscribed(identity: String, track_sid: String) -> void:
	print("[LiveKit] 🔇 Track unsubscribed  ", identity, " sid=", track_sid)


func _on_data_received(sender: String, data: PackedByteArray, reliable: bool) -> void:
	var text := data.get_string_from_utf8()
	var event : Variant = JSON.parse_string(text)
	if event == null:
		push_warning("[LiveKit] Received non-JSON data from " + sender)
		return
	print("[LiveKit] 📦 Data from ", sender, ": ", event)
	# Dispatch to your game logic here…


func _on_speaking_changed(identity: String, speaking: bool) -> void:
	print("[LiveKit] 🎤 ", identity, " speaking=", speaking)
	# Example: highlight avatar border when speaking
	var avatar := get_node_or_null("Avatars/" + identity)
	if avatar:
		avatar.modulate = Color.YELLOW if speaking else Color.WHITE
