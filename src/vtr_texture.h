#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/mutex_lock.hpp>
#include <atomic>

// Include subsystem headers
#include "udp.h"
#include "decoder.h"
#include "safe_queue.hpp"

namespace godot {

class VTRTexture : public Texture2D {
    GDCLASS(VTRTexture, Texture2D)

public:
    VTRTexture();
    ~VTRTexture();

    // Texture2D overrides
    int32_t _get_width() const override;
    int32_t _get_height() const override;
    bool _has_alpha() const override;
    RID _get_rid() const override;

    // Custom properties exposed to Godot Editor
    void set_port(const int32_t p_port);
    int32_t get_port() const;

    void set_active(const bool p_active);
    bool get_active() const;

    bool start_recording(const String& p_path, int64_t p_session_offset_usec);
    void stop_recording();
    bool is_recording() const;
    String get_last_recording_error() const;

protected:
    static void _bind_methods();
    void _notification(int p_what);

private:
    // Properties
    int32_t port = 5000;
    bool active = false;

    // Subsystems
    // Note: 'queue' must be declared before udp/decoder to ensure valid initialization order
    VTR::Que queue;
    VTR::UDP udp;
    VTR::Decoder decoder;

    // Rendering State
    RID texture_rid;
    int32_t width = 16;
    int32_t height = 9;
    std::atomic<bool> tearing_down{false};

    // Callbacks and Internal Logic
    void _sync_state(); // Starts/Stops components based on active flag
    void _stop_runtime(bool p_is_teardown);
    
    // Callback running on Decoder thread
    void _on_decoder_frame(const uint8_t* y_data, const uint8_t* uv_data, int p_width, int p_height, int y_stride, int uv_stride);

    // Internal method called via call_deferred on Main Thread
    void _update_texture_on_main_thread(const PackedByteArray& p_data, int p_width, int p_height);

	void _ensure_rid();
};

} // namespace godot
