#include "register_types.h"
#include "livekit_room.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

// ─────────────────────────────────────────────────────────────────────────────
//  Called by Godot when the extension is loaded
// ─────────────────────────────────────────────────────────────────────────────
void initialize_livekit_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;

    // Register LiveKitConnectionState enum so GDScript can access the constants
    // (constants are already bound inside LiveKitRoom::_bind_methods via
    //  BIND_ENUM_CONSTANT, but we still need to register the class itself)

    ClassDB::register_class<LiveKitRoom>();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Called by Godot when the extension is unloaded
// ─────────────────────────────────────────────────────────────────────────────
void uninitialize_livekit_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    // Nothing to do — Godot ClassDB handles class cleanup automatically.
}

// ─────────────────────────────────────────────────────────────────────────────
//  GDExtension entry point — Godot discovers this symbol via the .gdextension file
// ─────────────────────────────────────────────────────────────────────────────
extern "C" {
GDExtensionBool GDE_EXPORT livekit_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr   p_library,
    GDExtensionInitialization*         r_initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(
        p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_livekit_module);
    init_obj.register_terminator(uninitialize_livekit_module);
    init_obj.set_minimum_library_initialization_level(
        MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
} // extern "C"
