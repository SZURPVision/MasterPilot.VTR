#pragma once

#include "safe_queue.hpp"
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
namespace VTR
{
class Decoder
{
public:

    using FrameCallback = std::function<void(const uint8_t* y_data, const uint8_t* uv_data, int, int, int y_stride, int uv_stride)>;

    Decoder(Que& q) : input_queue(q){}
    ~Decoder();

    bool start();
    void stop();
    void set_on_frame_decoded(FrameCallback cb);
private:
    Que& input_queue;
    std::atomic<bool> running{false};
    std::thread decode_worker;

    FrameCallback on_frame_ready;
    std::mutex cb_mtx;

    // FFmpeg context
    const AVCodec* codec = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* sw_frame = nullptr;
    AVFrame* frame_rgb = nullptr;
    AVPacket* pkt = nullptr;
    SwsContext* sws_ctx = nullptr;

    AVBufferRef* hw_device_ctx = nullptr;
    static enum AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts);

    uint8_t* rgb_buffer = nullptr;
    int current_width = 0;
    int current_height = 0;

    void decode_loop();
    void cleanup();
    void process_frame(AVFrame* frame);

};
}
