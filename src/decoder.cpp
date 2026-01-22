#include "decoder.h"
#include <iostream>
#include <cstring>

namespace VTR
{

Decoder::~Decoder()
{
    stop();
    cleanup();
}

bool Decoder::start()
{
    if (running) return true;

    // Initialize FFmpeg codec
    // Note: older ffmpeg versions might need av_register_all(), but it's deprecated/removed in new ones.
    
    codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!codec) {
        std::cerr << "[Decoder] Codec not found: HEVC" << std::endl;
        return false;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "[Decoder] Could not allocate video codec context" << std::endl;
        return false;
    }

    // If you have extradata (SPS/PPS) it should be passed here, 
    // otherwise the decoder might assume in-band parameters.
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        std::cerr << "[Decoder] Could not open codec" << std::endl;
        return false;
    }

    frame = av_frame_alloc();
    frame_rgb = av_frame_alloc();
    pkt = av_packet_alloc();

    if (!frame || !frame_rgb || !pkt) {
        std::cerr << "[Decoder] Could not allocate frames or packet" << std::endl;
        return false;
    }

    running = true;
    decode_worker = std::thread(&Decoder::decode_loop, this);
    
    return true;
}

void Decoder::stop()
{
    running = false;
    // Push an empty vector to unblock the queue wait if currently blocked
    input_queue.push({}); 
    if (decode_worker.joinable()) {
        decode_worker.join();
    }
}

void Decoder::set_on_frame_decoded(FrameCallback cb)
{
    std::lock_guard<std::mutex> lock(cb_mtx);
    on_frame_ready = cb;
}

void Decoder::cleanup()
{
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
        codec_ctx = nullptr;
    }
    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if (frame_rgb) {
        av_frame_free(&frame_rgb);
        frame_rgb = nullptr;
    }
    if (pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
    }
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    if (rgb_buffer) {
        av_free(rgb_buffer);
        rgb_buffer = nullptr;
    }
}

void Decoder::decode_loop()
{
    std::vector<uint8_t> data;
    while (running)
    {
        // Blocking pop from safe queue
        if (!input_queue.pop(data)) {
            continue; 
        }

        if (data.empty()) continue; // Skip empty keep-alive packets

        // Prepare packet
        // Note: We use the data directly. We assume the vector stays valid until we process it.
        // Actually av_packet_from_data is unsafe if we don't own the buffer. 
        // Safer to copy or ensure lifetime. Here we copy into the packet struct ref.
        
        // Reset packet
        av_packet_unref(pkt);
        
        // We must create a new reference or copy because 'data' vector will be destroyed next iteration
        if (av_new_packet(pkt, data.size()) < 0) {
             std::cerr << "[Decoder] Packet allocation failed" << std::endl;
             continue;
        }
        memcpy(pkt->data, data.data(), data.size());

        // Send packet to decoder
        int ret = avcodec_send_packet(codec_ctx, pkt);
        if (ret < 0) {
            char errbuf[64];
            av_strerror(ret, errbuf, 64);
            std::cerr << "[Decoder] Error sending packet for decoding: " << errbuf << std::endl;
            continue;
        }

        // Receive available frames
        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                std::cerr << "[Decoder] Error during decoding" << std::endl;
                break;
            }

            // We have a valid frame here
            process_frame(frame);
        }
    }
}

void Decoder::process_frame(AVFrame* src_frame)
{
    // Check if resolution changed or we need to init sws context
    if (src_frame->width != current_width || src_frame->height != current_height || !sws_ctx) {
        
        current_width = src_frame->width;
        current_height = src_frame->height;

        // Cleanup old SWS context
        if (sws_ctx) {
            sws_freeContext(sws_ctx);
            sws_ctx = nullptr;
        }
        
        // Cleanup old buffer
        if (rgb_buffer) {
            av_free(rgb_buffer);
            rgb_buffer = nullptr;
        }

        // Create SWS Context: HEVC is usually YUV420P. Target is RGBA for Godot.
        sws_ctx = sws_getContext(
            current_width, current_height, codec_ctx->pix_fmt, // Input
            current_width, current_height, AV_PIX_FMT_RGBA,    // Output
            SWS_BILINEAR, NULL, NULL, NULL
        );

        if (!sws_ctx) {
            std::cerr << "[Decoder] Could not initialize SwsContext" << std::endl;
            return;
        }

        // Allocate buffer for RGBA
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, current_width, current_height, 1);
        rgb_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

        // Assign buffer to rgb frame
        av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, rgb_buffer,
                             AV_PIX_FMT_RGBA, current_width, current_height, 1);
    }

    // Convert YUV to RGBA
    sws_scale(sws_ctx, (const uint8_t * const*)src_frame->data, src_frame->linesize, 0,
              current_height, frame_rgb->data, frame_rgb->linesize);

    // Notify callback
    std::lock_guard<std::mutex> lock(cb_mtx);
    if (on_frame_ready) {
        // frame_rgb->data[0] points to the start of the packed RGBA buffer
        on_frame_ready(frame_rgb->data[0], current_width, current_height);
    }
}

}