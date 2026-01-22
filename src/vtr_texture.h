#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/mutex_lock.hpp>

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

protected:
    static void _bind_methods();

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

    // Callbacks and Internal Logic
    void _sync_state(); // Starts/Stops components based on active flag
    
    // Callback running on Decoder thread
    void _on_decoder_frame(const uint8_t* data, int p_width, int p_height);

    // Internal method called via call_deferred on Main Thread
    void _update_texture_on_main_thread(const PackedByteArray& p_data, int p_width, int p_height);
};

} // namespace godot