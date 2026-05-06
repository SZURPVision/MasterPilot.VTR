#include "decoder.h"
#include "time_analyzer.hpp"
#include <iostream>
#include <cstring>
#include <limits>
#include <vector>

namespace VTR
{

Decoder::~Decoder()
{
    stop();
    cleanup();
}

enum AVPixelFormat Decoder::get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    const enum AVPixelFormat* p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_VAAPI) {
            return *p;
        }
    }
    std::cerr << "[Decoder] Failed to get HW surface format." << std::endl;
    return AV_PIX_FMT_NONE;
}

bool Decoder::start()
{
    if (running) return true;

    // Initialize FFmpeg codec
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

    // Initialize VAAPI hardware context
    int err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0);
    if (err < 0) {
        std::cerr << "[Decoder] Failed to create a VAAPI device. Falling back to software decoding." << std::endl;
    } else {
        codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        codec_ctx->get_format = get_hw_format;
        std::cout << "[Decoder] VAAPI hardware device context created." << std::endl;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        std::cerr << "[Decoder] Could not open codec" << std::endl;
        return false;
    }

    frame = av_frame_alloc();
    sw_frame = av_frame_alloc();
    frame_rgb = av_frame_alloc();
    pkt = av_packet_alloc();

    if (!frame || !sw_frame || !frame_rgb || !pkt) {
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
    if (sw_frame) {
        av_frame_free(&sw_frame);
        sw_frame = nullptr;
    }
    if (frame_rgb) {
        av_frame_free(&frame_rgb);
        frame_rgb = nullptr;
    }
    if (pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
    }
    if (hw_device_ctx) {
        av_buffer_unref(&hw_device_ctx);
        hw_device_ctx = nullptr;
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

        if (data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            std::cerr << "[Decoder] Packet too large: " << data.size() << std::endl;
            continue;
        }

        av_packet_unref(pkt);
        int ret = av_new_packet(pkt, static_cast<int>(data.size()));
        if (ret < 0) {
            char errbuf[64];
            av_strerror(ret, errbuf, 64);
            std::cerr << "[Decoder] Error allocating packet: " << errbuf << std::endl;
            continue;
        }

        memcpy(pkt->data, data.data(), data.size());
        ret = avcodec_send_packet(codec_ctx, pkt);
        av_packet_unref(pkt);
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
    TimeAnalyzer timer{"Decoder::process_frame"};
    AVFrame* tmp_frame = src_frame;

    // If it's a hardware frame, transfer it to CPU memory
    if (src_frame->format == AV_PIX_FMT_VAAPI) {
        av_frame_unref(sw_frame);
        int ret = av_hwframe_transfer_data(sw_frame, src_frame, 0);
        if (ret < 0) {
            std::cerr << "[Decoder] Error transferring data from GPU to CPU" << std::endl;
            return;
        }
        tmp_frame = sw_frame;
    }

    current_width = tmp_frame->width;
    current_height = tmp_frame->height;

    // Notify callback with NV12 planes
    std::lock_guard<std::mutex> lock(cb_mtx);
    if (on_frame_ready) {
        // NV12: data[0] is Y plane, data[1] is UV interleaved plane
        on_frame_ready(
            tmp_frame->data[0], 
            tmp_frame->data[1], 
            current_width, 
            current_height,
            tmp_frame->linesize[0],
            tmp_frame->linesize[1]
        );
    }
}

}
