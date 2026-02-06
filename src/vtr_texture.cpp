#include "vtr_texture.h"
#include "time_analyzer.hpp"

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rendering_server.hpp>

using namespace godot;

VTRTexture::VTRTexture() 
    : udp(queue), decoder(queue) // Initialize subsystems with the shared queue
{
    decoder.set_on_frame_decoded([this](const uint8_t* data, int w, int h) {
        this->_on_decoder_frame(data, w, h);
    });
}

VTRTexture::~VTRTexture() {
    // 1. Stop threads first to prevent callbacks during destruction
    set_active(false);
    
    // 2. Clean up the GPU resource
    if (texture_rid.is_valid()) {
        RenderingServer::get_singleton()->free_rid(texture_rid);
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

void VTRTexture::_sync_state() {
    if (active) {
        // Clear queue before starting to avoid processing old data
        // (Accessing queue directly might require friend access or a clear() method on SafeQueue, 
        //  but simpler is just to let the decoder chew through it or logic in UDP::start)
        
        bool udp_started = udp.start(port);
        bool dec_started = decoder.start();
        
        if (!udp_started || !dec_started) {
            UtilityFunctions::printerr("[VTRTexture] Failed to start components.");
            active = false;
            udp.stop();
            decoder.stop();
        }
    } else {
        udp.stop();
        decoder.stop();
    }
}

void VTRTexture::_on_decoder_frame(const uint8_t* data, int p_width, int p_height) {
	TimeAnalyzer timer{"VTRTexure::on_decoder_frame"};
    // [Decoder Thread]
    // We cannot touch RenderingServer or Ref<Image> safely here.
    // We copy the raw data into a Godot PackedByteArray.
    
    PackedByteArray pba;
    int size = p_width * p_height * 4; // RGBA8 = 4 bytes per pixel
    pba.resize(size);
    
    // memcpy is fast; copy data into the Godot-managed buffer
    memcpy(pba.ptrw(), data, size);
    
    // Defer the actual update to the main thread
    call_deferred("_update_texture_on_main_thread", pba, p_width, p_height);
}

void VTRTexture::_update_texture_on_main_thread(const PackedByteArray& p_data, int p_width, int p_height) {
	TimeAnalyzer timer{"VTRTexure::update_texture_on_main_thread"};
    // [Main Thread]
    
    // Create an image wrapper around the data (efficient, mostly metadata)
    Ref<Image> img = Image::create_from_data(p_width, p_height, false, Image::FORMAT_RGBA8, p_data);
    
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

    // Bind internal method for call_deferred
    ClassDB::bind_method(D_METHOD("_update_texture_on_main_thread", "data", "width", "height"), &VTRTexture::_update_texture_on_main_thread);
}