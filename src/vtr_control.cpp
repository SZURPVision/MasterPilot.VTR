#include "vtr_control.h"
#include "time_analyzer.hpp"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <cstring>

using namespace godot;

constexpr const char* NV12_SHADER_CODE = R"SHADER(
shader_type canvas_item;

void fragment() {
    ivec2 itex_size = textureSize(TEXTURE, 0);
    int h_int = itex_size.y * 2 / 3;
    float h = float(h_int);
    
    // Y component
    float y_uv_y = clamp(UV.y * (h / float(itex_size.y)), 0.5 / float(itex_size.y), (h - 0.5) / float(itex_size.y));
    float y = texture(TEXTURE, vec2(UV.x, y_uv_y)).r;
    
    // UV component using texelFetch
    ivec2 pos = ivec2(UV * vec2(float(itex_size.x), h));
    pos = clamp(pos, ivec2(0), ivec2(itex_size.x - 1, h_int - 1));
    
    int uv_y = h_int + (pos.y / 2); int uv_x_u = (pos.x / 2) * 2; int uv_x_v = uv_x_u + 1;
    float u = texelFetch(TEXTURE, ivec2(uv_x_u, uv_y), 0).r;
    float v = texelFetch(TEXTURE, ivec2(uv_x_v, uv_y), 0).r;

    // Adjust YUV values (BT.601 Limited Range)
    y = 1.16438 * (y - 0.062745);
    u = u - 0.501961;
    v = v - 0.501961;

    // Convert to RGB
    float r = y + 1.596 * v;
    float g = y - 0.3918 * u - 0.813 * v;
    float b = y + 2.017 * u;

    COLOR = vec4(r, g, b, 1.0);
}
)SHADER";

VTRControl::VTRControl() 
    : udp(queue), decoder(queue)
{
    set_process(!Engine::get_singleton()->is_editor_hint());
    set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
    
	if(Engine::get_singleton()->is_editor_hint()) return;

    decoder.set_on_frame_decoded([this](const uint8_t* y_data, const uint8_t* uv_data, int w, int h, int ys, int uvs) {
        this->_on_decoder_frame(y_data, uv_data, w, h, ys, uvs);
    });
}

VTRControl::~VTRControl() {
	if(Engine::get_singleton()->is_editor_hint()) return;
    _stop_runtime(true);
}

void VTRControl::_setup_shader() {
    if (Engine::get_singleton()->is_editor_hint() || internal_material.is_valid()) return;
    
    Ref<Shader> shader;
    shader.instantiate();
    shader->set_code(NV12_SHADER_CODE);
    
    internal_material.instantiate();
    internal_material->set_shader(shader);
    
    set_material(internal_material);
}

void VTRControl::_ensure_texture() {
    if (Engine::get_singleton()->is_editor_hint() || internal_texture.is_valid()) return;
    
    int p_width = 128;
    int p_height = 128;
    int p_packed_height = p_height + p_height / 2;
    
    PackedByteArray pba;
    pba.resize(p_width * p_packed_height);
    uint8_t* ptr = pba.ptrw();
    
    memset(ptr, 128, p_width * p_packed_height);
    memset(ptr, 16, p_width * p_height);
    
    Ref<Image> placeholder = Image::create_from_data(p_width, p_packed_height, false, Image::FORMAT_L8, pba);
    internal_texture = ImageTexture::create_from_image(placeholder);
    
    set_texture(internal_texture);
    current_width = p_width;
    current_height = p_height;
}

void VTRControl::set_port(const int32_t p_port) {
    if (port == p_port) return;
    port = p_port;
    if (!Engine::get_singleton()->is_editor_hint() && active) {
        set_active(false);
        set_active(true);
    }
}

int32_t VTRControl::get_port() const {
    return port;
}

void VTRControl::set_active(const bool p_active) {
    if (active == p_active) return;
    active = p_active;
	if (!Engine::get_singleton()->is_editor_hint() || is_node_ready()) return;
    _sync_state();
}

bool VTRControl::get_active() const {
    return active;
}

bool VTRControl::start_recording(const String& p_path, int64_t p_session_offset_usec) {
	if(Engine::get_singleton()->is_editor_hint()) return true;
    String absolute_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    auto utf8 = absolute_path.utf8();
    bool started = udp.start_recording(utf8.get_data(), p_session_offset_usec);
    if (!started) {
        UtilityFunctions::printerr(vformat(
            "[VTRControl] Failed to start video recording: %s (%s)",
            absolute_path,
            get_last_recording_error()));
    }
    return started;
}

void VTRControl::stop_recording() {
	if(Engine::get_singleton()->is_editor_hint()) return;
    udp.stop_recording();
}

bool VTRControl::is_recording() const {
    return udp.is_recording();
}

String VTRControl::get_last_recording_error() const {
    return String(udp.get_last_recording_error().c_str());
}

void VTRControl::_notification(int p_what) {
	bool is_editor = Engine::get_singleton()->is_editor_hint();

    switch (p_what) {
        case NOTIFICATION_READY:
            add_to_group("recording.video_transmission");
			if(is_editor) return;

            _setup_shader();
            _ensure_texture();
            // Start fully transparent to hide placeholder if no active signal
            set_modulate(Color(1, 1, 1, 0));

			
			_sync_state();
            break;
            
        case NOTIFICATION_PROCESS: {
			if(is_editor) return;

            if (!active) {
                set_modulate(Color(1, 1, 1, 0));
                break;
            }
            
            uint64_t current_msec = Time::get_singleton()->get_ticks_msec();
            if (current_msec - last_frame_msec > 500) {
                set_modulate(Color(1, 1, 1, 0));
            } else {
                set_modulate(Color(1, 1, 1, 1));
            }
            break;
        }
            
        case NOTIFICATION_PREDELETE:
            _stop_runtime(true);
            break;
    }
}

void VTRControl::_stop_runtime(bool p_is_teardown) {
    if (p_is_teardown) {
        tearing_down.store(true);
        decoder.set_on_frame_decoded(nullptr);
    }

    active = false;
    udp.stop();
    decoder.stop();
}

void VTRControl::_sync_state() {
	if (Engine::get_singleton()->is_editor_hint()) return;
    if (active) {
        if (tearing_down.load()) {
            active = false;
            return;
        }

        bool udp_started = udp.start(port);
        bool dec_started = decoder.start();
        
        if (!udp_started || !dec_started) {
            UtilityFunctions::printerr("[VTRControl] Failed to start components.");
            active = false;
            _stop_runtime(false);
        }
        
        // Reset timestamp on start
        last_frame_msec = Time::get_singleton()->get_ticks_msec();
    } else {
        _stop_runtime(false);
    }
}

void VTRControl::_on_decoder_frame(const uint8_t* y_data, const uint8_t* uv_data, int p_width, int p_height, int y_stride, int uv_stride) {
    if (tearing_down.load() || !active) {
        return;
    }

	TimeAnalyzer timer{"VTRControl::on_decoder_frame"};
    
    int packed_height = p_height + p_height / 2;
    int size = p_width * packed_height; 
    
    PackedByteArray pba;
    pba.resize(size);
    uint8_t* dst = pba.ptrw();

    for (int i = 0; i < p_height; ++i) {
        memcpy(dst + i * p_width, y_data + i * y_stride, p_width);
    }

    uint8_t* uv_dst = dst + p_width * p_height;
    for (int i = 0; i < p_height / 2; ++i) {
        memcpy(uv_dst + i * p_width, uv_data + i * uv_stride, p_width);
    }
    
    call_deferred("_update_texture_on_main_thread", pba, p_width, p_height);
}

void VTRControl::_update_texture_on_main_thread(const PackedByteArray& p_data, int p_width, int p_height) {
    if (tearing_down.load() || !active || is_queued_for_deletion()) {
        return;
    }

	TimeAnalyzer timer{"VTRControl::update_texture_on_main_thread"};
    
    last_frame_msec = Time::get_singleton()->get_ticks_msec();

    _ensure_texture();
    
    int packed_height = p_height + p_height / 2;
    Ref<Image> img = Image::create_from_data(p_width, packed_height, false, Image::FORMAT_L8, p_data);
    
    if (current_width != p_width || current_height != p_height) {
        current_width = p_width;
        current_height = p_height;
        internal_texture->set_image(img);
    } else {
        internal_texture->update(img);
    }
}

void VTRControl::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_port"), &VTRControl::get_port);
    ClassDB::bind_method(D_METHOD("set_port", "p_port"), &VTRControl::set_port);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");

    ClassDB::bind_method(D_METHOD("get_active"), &VTRControl::get_active);
    ClassDB::bind_method(D_METHOD("set_active", "p_active"), &VTRControl::set_active);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "get_active");

    ClassDB::bind_method(D_METHOD("start_recording", "path", "session_offset_usec"), &VTRControl::start_recording);
    ClassDB::bind_method(D_METHOD("stop_recording"), &VTRControl::stop_recording);
    ClassDB::bind_method(D_METHOD("is_recording"), &VTRControl::is_recording);
    ClassDB::bind_method(D_METHOD("get_last_recording_error"), &VTRControl::get_last_recording_error);

    ClassDB::bind_method(D_METHOD("_update_texture_on_main_thread", "data", "width", "height"), &VTRControl::_update_texture_on_main_thread);
}
