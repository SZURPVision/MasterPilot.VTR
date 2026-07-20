#pragma once

#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/string.hpp>
#include <atomic>

// Include subsystem headers
#include "udp.h"
#include "decoder.h"
#include "safe_queue.hpp"

namespace godot {

class VTRControl : public TextureRect {
    GDCLASS(VTRControl, TextureRect)

public:
    VTRControl();
    ~VTRControl();

    // Custom properties exposed to Godot Editor
    void set_port(const int32_t p_port);
    int32_t get_port() const;

    void set_active(const bool p_active);
    bool get_active() const;

    bool start_recording(const String& p_path, int64_t p_session_offset_usec);
    void stop_recording();
    bool is_recording() const;
    String get_last_recording_error() const;

    Ref<Image> capture_screenshot();

protected:
    static void _bind_methods();
    void _notification(int p_what);

private:
    // Properties
    int32_t port = 3334;
    bool active = false;

    // Subsystems
    VTR::Que queue;
    VTR::UDP udp;
    VTR::Decoder decoder;

    // State
    uint64_t last_frame_msec = 0;
    int32_t current_width = 0;
    int32_t current_height = 0;
    std::atomic<bool> tearing_down{false};
    int lock_fd = -1;

    Ref<ImageTexture> internal_texture;
    Ref<ShaderMaterial> internal_material;

    // Callbacks and Internal Logic
    void _sync_state();
    void _stop_runtime(bool p_is_teardown);
    
    // Callback running on Decoder thread
    void _on_decoder_frame(const uint8_t* y_data, const uint8_t* uv_data, int p_width, int p_height, int y_stride, int uv_stride);

    // Internal method called via call_deferred on Main Thread
    void _update_texture_on_main_thread(const PackedByteArray& p_data, int p_width, int p_height);

    void _ensure_texture();
    void _setup_shader();
};

} // namespace godot
