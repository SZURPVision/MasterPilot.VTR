#include "udp.h"
#include "frame.h"
#include "udp_utils.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

using namespace VTR;

bool UDP::start(int port)
{
    std::cout<<"UDP started"<<std::endl;
    if (running) return true;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr
    {
        .sin_family = AF_INET, //IPv4
        .sin_port = htons(port),
        .sin_addr {INADDR_ANY}, //所有网卡接口
    };

    if(bind(sockfd, (const sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        close(sockfd);
        return false;
    }

    running = true;
    recv_worker = std::thread(&UDP::recv_loop,this);
    recv_worker.detach();
    return true;
}
void UDP::stop()
{
    running = false;
    if (sockfd != -1)
    {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
    }
}
void UDP::recv_loop()
{
    uint8_t buffer[65536];
    sockaddr_in cliaddr{};
    socklen_t len = sizeof(cliaddr);
    while(running)
    {
        auto n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&cliaddr, &len);
        if (n < sizeof(UDPHeader)) continue;
        process_packet(buffer, n);
    }
    close(sockfd);
    sockfd = 0;
}
void UDP::process_packet(uint8_t* raw_buf, ssize_t n)
{
    auto header = reinterpret_cast<UDPHeader*>(raw_buf);
    
    //不管过时包
    if (!is_later_than(header->frame_id, latest_frame_id)) return;

    std::lock_guard<std::mutex> lock{pool_mtx};
    auto& fb = assembling_pool[header->frame_id];
    uint8_t* payload = raw_buf + sizeof(UDPHeader);
    size_t payload_size = n - sizeof(UDPHeader);

    fb.total_size = header->total_size;
    fb.slices[header->slice_idx] = {payload, payload + payload_size};
    fb.current_received_byte += payload_size;

    //所有数据就绪
    if (fb.current_received_byte >= fb.total_size) {
        //组装
        std::vector<uint8_t> full_frame;
        full_frame.reserve(fb.total_size);
        uint16_t expected = 0;
        for (auto& [_,slice] : fb.slices) {
            full_frame.insert(full_frame.end(), slice.begin(), slice.end());
        }

        output_queue.push(std::move(full_frame));

        //清除组装缓存
        assembling_pool.erase(header->frame_id);

        //更新最新id,扔掉老东西
        latest_frame_id = header->frame_id;
        std::erase_if(assembling_pool, [this](const auto& item) {
            return is_later_than(latest_frame_id, static_cast<decltype(latest_frame_id)>(item.first+MAX_ASM_POOL_SIZE)); 
        });
    }
}