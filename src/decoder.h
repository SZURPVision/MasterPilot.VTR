#pragma once

#include "safe_queue.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

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

    // Screenshot: on-demand NV12→RGB24 conversion on decoder thread
    void request_screenshot();
    bool wait_screenshot(int timeout_ms, std::vector<uint8_t>& out_rgb, int& out_w, int& out_h);
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

    // Screenshot infrastructure (on-demand only)
    std::atomic<bool> screenshot_requested{false};
    std::mutex screenshot_mtx;
    std::condition_variable screenshot_cv;
    std::vector<uint8_t> screenshot_rgb;
    int screenshot_w = 0;
    int screenshot_h = 0;
    bool screenshot_ready = false;

    SwsContext* sws_rgb = nullptr;
    AVFrame* frame_rgb_out = nullptr;

    void decode_loop();
    void cleanup();
    void process_frame(AVFrame* frame);

};
}
