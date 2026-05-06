#include "vtr_texture.h"
#include "time_analyzer.hpp"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rendering_server.hpp>

using namespace godot;

VTRTexture::VTRTexture() 
    : udp(queue), decoder(queue) // Initialize subsystems with the shared queue
{
    decoder.set_on_frame_decoded([this](const uint8_t* y_data, const uint8_t* uv_data, int w, int h, int ys, int uvs) {
        this->_on_decoder_frame(y_data, uv_data, w, h, ys, uvs);
    });
}

VTRTexture::~VTRTexture() {
    _stop_runtime(true);
    
    if (texture_rid.is_valid()) {
        RenderingServer::get_singleton()->free_rid(texture_rid);
        texture_rid = RID();
    }
}

int32_t VTRTexture::_get_width() const { return width; }
int32_t VTRTexture::_get_height() const { return height; }
bool VTRTexture::_has_alpha() const { return false; }

RID VTRTexture::_get_rid() const {
	if (!texture_rid.is_valid()) { const_cast<VTRTexture*>(this)->_ensure_rid();}
    return texture_rid;
}

void VTRTexture::_ensure_rid()
{
	if (texture_rid.is_valid()) return;
    Ref<Image> placeholder = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
    texture_rid = RenderingServer::get_singleton()->texture_2d_create(placeholder);
}

void VTRTexture::set_port(const int32_t p_port) {
    if (port == p_port) return;
    port = p_port;
    // Restart if currently running to apply new port
    if (active) {
        set_active(false);
        set_active(true);
    }
}

int32_t VTRTexture::get_port() const {
    return port;
}

void VTRTexture::set_active(const bool p_active) {
    if (active == p_active) return;
    active = p_active;
    _sync_state();
}

bool VTRTexture::get_active() const {
    return active;
}

bool VTRTexture::start_recording(const String& p_path, int64_t p_session_offset_usec) {
    String absolute_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    auto utf8 = absolute_path.utf8();
    bool started = udp.start_recording(utf8.get_data(), p_session_offset_usec);
    if (!started) {
        UtilityFunctions::printerr(vformat(
            "[VTRTexture] Failed to start video recording: %s (%s)",
            absolute_path,
            get_last_recording_error()));
    }
    return started;
}

void VTRTexture::stop_recording() {
    udp.stop_recording();
}

bool VTRTexture::is_recording() const {
    return udp.is_recording();
}

String VTRTexture::get_last_recording_error() const {
    return String(udp.get_last_recording_error().c_str());
}

void VTRTexture::_notification(int p_what) {
    if (p_what == NOTIFICATION_PREDELETE) {
        _stop_runtime(true);
    }
}

void VTRTexture::_stop_runtime(bool p_is_teardown) {
    if (p_is_teardown) {
        tearing_down.store(true);
        decoder.set_on_frame_decoded(nullptr);
    }

    active = false;
    udp.stop();
    decoder.stop();
}

void VTRTexture::_sync_state() {
    if (active) {
        if (tearing_down.load()) {
            active = false;
            return;
        }

        bool udp_started = udp.start(port);
        bool dec_started = decoder.start();
        
        if (!udp_started || !dec_started) {
            UtilityFunctions::printerr("[VTRTexture] Failed to start components.");
            active = false;
            _stop_runtime(false);
        }
    } else {
        _stop_runtime(false);
    }
}

void VTRTexture::_on_decoder_frame(const uint8_t* y_data, const uint8_t* uv_data, int p_width, int p_height, int y_stride, int uv_stride) {
    if (tearing_down.load() || !active) {
        return;
    }

	TimeAnalyzer timer{"VTRTexure::on_decoder_frame"};
    
    // For NV12, Y plane is W * H. UV plane is W * (H / 2).
    // Total height of the texture will be H + H / 2.
    int packed_height = p_height + p_height / 2;
    int size = p_width * packed_height; 
    
    PackedByteArray pba;
    pba.resize(size);
    uint8_t* dst = pba.ptrw();

    // Copy Y plane line by line (to handle stride)
    for (int i = 0; i < p_height; ++i) {
        memcpy(dst + i * p_width, y_data + i * y_stride, p_width);
    }

    // Copy UV plane line by line
    uint8_t* uv_dst = dst + p_width * p_height;
    for (int i = 0; i < p_height / 2; ++i) {
        memcpy(uv_dst + i * p_width, uv_data + i * uv_stride, p_width);
    }
    
    // Defer the actual update to the main thread
    call_deferred("_update_texture_on_main_thread", pba, p_width, p_height);
}

void VTRTexture::_update_texture_on_main_thread(const PackedByteArray& p_data, int p_width, int p_height) {
    if (tearing_down.load() || !active || is_queued_for_deletion()) {
        return;
    }

	TimeAnalyzer timer{"VTRTexure::update_texture_on_main_thread"};
    // [Main Thread]

    _ensure_rid();
    
    // NV12 packed into a single L8 texture with height = H * 1.5
    int packed_height = p_height + p_height / 2;
    Ref<Image> img = Image::create_from_data(p_width, packed_height, false, Image::FORMAT_L8, p_data);
    
    // Check if the resolution has changed (or if this is the first frame updating the 1x1 placeholder)
    if (width != p_width || height != p_height) {
        width = p_width;
        height = p_height;

        // Create a NEW texture with the correct size
        RID new_rid = RenderingServer::get_singleton()->texture_2d_create(img);

        // Replace the OLD texture's internal data with the NEW one.
        // This keeps 'texture_rid' valid but gives it the new size and data.
        // 'new_rid' is automatically freed by texture_replace.
        RenderingServer::get_singleton()->texture_replace(texture_rid, new_rid);
    } else {
        // Resolution matches; perform a fast update
        RenderingServer::get_singleton()->texture_2d_update(texture_rid, img, 0);
    }
}

void VTRTexture::_bind_methods() {
    // Bind properties
    ClassDB::bind_method(D_METHOD("get_port"), &VTRTexture::get_port);
    ClassDB::bind_method(D_METHOD("set_port", "p_port"), &VTRTexture::set_port);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");

    ClassDB::bind_method(D_METHOD("get_active"), &VTRTexture::get_active);
    ClassDB::bind_method(D_METHOD("set_active", "p_active"), &VTRTexture::set_active);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "get_active");

    ClassDB::bind_method(D_METHOD("start_recording", "path", "session_offset_usec"), &VTRTexture::start_recording);
    ClassDB::bind_method(D_METHOD("stop_recording"), &VTRTexture::stop_recording);
    ClassDB::bind_method(D_METHOD("is_recording"), &VTRTexture::is_recording);
    ClassDB::bind_method(D_METHOD("get_last_recording_error"), &VTRTexture::get_last_recording_error);

    // Bind internal method for call_deferred
    ClassDB::bind_method(D_METHOD("_update_texture_on_main_thread", "data", "width", "height"), &VTRTexture::_update_texture_on_main_thread);
}
