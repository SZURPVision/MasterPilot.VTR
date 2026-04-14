#pragma once

#include "safe_queue.hpp"
#include "reassembly_pool.hpp"
#include <cstdint>
#include <thread>
#include <atomic>
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

public:
    UDP(Que& q) : output_queue(q){}
    ~UDP();
    bool start(int port);
    void stop();
};
}
