#include "vtr_texture.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <iostream>

using namespace godot;

void initialize_vtr_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
	std::cout<<"[VTR]Registering ClassDB"<< std::endl;
    ClassDB::register_class<VTRTexture>();
}

void uninitialize_vtr_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
	std::cout<<"[VTR]Unregistering Module"<< std::endl;
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT vtr_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_vtr_module);
    init_obj.register_terminator(uninitialize_vtr_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}