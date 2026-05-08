#include "decoder.h"
#include "time_analyzer.hpp"
#include <iostream>
#include <cstring>
#include <limits>
#include <vector>
#include <fstream>
#include <string>
#include <filesystem>

namespace VTR
{

// Helper to find preferred VAAPI device (Intel: 0x8086, AMD: 0x1002)
static std::string find_preferred_hw_device() {
    for (int i = 128; i < 135; ++i) {
        std::string base = "renderD" + std::to_string(i);
        std::filesystem::path device_path = "/dev/dri/" + base;
        std::filesystem::path vendor_path = "/sys/class/drm/" + base + "/device/vendor";

        if (std::filesystem::exists(vendor_path)) {
            std::ifstream ifs(vendor_path.string());
            if (ifs.is_open()) {
                std::string vendor;
                ifs >> vendor;
                // Intel (0x8086) or AMD (0x1002)
                if (vendor.find("0x8086") != std::string::npos || 
                    vendor.find("0x1002") != std::string::npos) {
                    return device_path.string();
                }
            }
        }
    }
    return "";
}

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
            std::cout << "[Decoder] VA-API hardware format (AV_PIX_FMT_VAAPI) selected for decoding." << std::endl;
            return *p;
        }
    }
    std::cerr << "[Decoder] VA-API hardware acceleration is not supported by the codec/stream. Falling back to software decoding." << std::endl;
    // Return the first available software format (usually YUV420P or NV12)
    return pix_fmts[0];
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

    // Prioritize device selection:
    // 1. Environment variable MASTERPILOT_VAAPI_DEVICE
    // 2. Preferred iGPU (Intel/AMD)
    // 3. Auto-detection (NULL)
    const char* device_env = std::getenv("MASTERPILOT_VAAPI_DEVICE");
    std::string preferred_device;
    const char* target_device = NULL;

    if (device_env) {
        target_device = device_env;
        std::cout << "[Decoder] Using VA-API device from environment: " << target_device << std::endl;
    } else {
        preferred_device = find_preferred_hw_device();
        if (!preferred_device.empty()) {
            target_device = preferred_device.c_str();
            std::cout << "[Decoder] Found preferred hardware device (Intel/AMD): " << target_device << std::endl;
        } else {
            std::cout << "[Decoder] No preferred Intel/AMD device found. Using FFmpeg auto-detection." << std::endl;
        }
    }

    // Initialize VAAPI hardware context
    int err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, target_device, NULL, 0);
    if (err < 0) {
        // If specific device failed, try one last time with auto-detection
        if (target_device != NULL) {
            std::cerr << "[Decoder] VAAPI failed on " << target_device << ", trying FFmpeg auto-detection..." << std::endl;
            err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0);
        }
    }

    if (err < 0) {
        char errbuf[128];
        av_strerror(err, errbuf, sizeof(errbuf));
        std::cerr << "[Decoder] Failed to create a VAAPI device (error: " << errbuf << "). The decoder will use CPU software decoding." << std::endl;
    } else {
        codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        codec_ctx->get_format = get_hw_format;
        std::cout << "[Decoder] VA-API hardware device context successfully created." << std::endl;
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

    // 核心修复：确保始终输出 NV12。
    // 如果不是 NV12（例如真实源输出 yuv420p），我们利用 sws_scale 转换。
    if (tmp_frame->format != AV_PIX_FMT_NV12) {
        sws_ctx = sws_getCachedContext(sws_ctx,
            current_width, current_height, (AVPixelFormat)tmp_frame->format,
            current_width, current_height, AV_PIX_FMT_NV12,
            SWS_POINT, NULL, NULL, NULL);
        
        if (sws_ctx) {
            if (frame_rgb->format != AV_PIX_FMT_NV12 || frame_rgb->width != current_width || frame_rgb->height != current_height) {
                av_frame_unref(frame_rgb);
                frame_rgb->format = AV_PIX_FMT_NV12;
                frame_rgb->width = current_width;
                frame_rgb->height = current_height;
                if (av_frame_get_buffer(frame_rgb, 0) < 0) {
                    return;
                }
            }
            sws_scale(sws_ctx, tmp_frame->data, tmp_frame->linesize, 0, current_height,
                      frame_rgb->data, frame_rgb->linesize);
            tmp_frame = frame_rgb;
        }
    }

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
