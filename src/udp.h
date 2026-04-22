#pragma once

#include "safe_queue.hpp"
#include "reassembly_pool.hpp"
#include "video_muxer.h"
#include <cstdint>
#include <thread>
#include <atomic>
#include <string>
#include <unistd.h>
namespace VTR
{
class UDP
{
private:
    std::atomic<int> sockfd{-1};
    Que& output_queue;
    std::atomic<bool> running{false};

    std::thread recv_worker;
    void recv_loop();
    void process_packet(uint8_t* raw_buf, ssize_t n);

    ReassemblyPool asm_pool;
    VideoMuxer muxer;

public:
    UDP(Que& q) : output_queue(q){}
    ~UDP();
    bool start(int port);
    void stop();
    bool start_recording(const std::string& path, int64_t session_offset_usec);
    void stop_recording();
    bool is_recording() const;
    std::string get_last_recording_error() const;
};
}
