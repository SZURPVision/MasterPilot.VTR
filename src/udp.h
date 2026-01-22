#pragma once

#include "frame.h"
#include "safe_queue.hpp"
#include <cstdint>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <unordered_map>
namespace VTR
{
class UDP
{
    private:
    int sockfd = -1;
    Que& output_queue;
    std::atomic<bool> running{false};
    std::unordered_map<uint16_t, FrameBuffer> assembling_pool;
    std::mutex pool_mtx;
    
    uint16_t latest_frame_id = 0;

    std::thread recv_worker;
    void recv_loop();
    void process_packet(uint8_t* raw_buf, ssize_t n);
    public:
    UDP(Que& q) : output_queue(q){}
    bool start(int port);
    void stop();
    const decltype(UDPHeader::slice_idx) MAX_ASM_POOL_SIZE = 30;
};
}