#include "udp.h"
#include "frame.h"
#include "time_analyzer.hpp"
#include "udp_utils.hpp"
#include <cstdint>
#include <format>
#include <iostream>
#include <ostream>
#include <poll.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <vector>

using namespace VTR;

UDP::~UDP()
{
    stop();
}

bool UDP::start(int port)
{
    if (running)
        return true;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        std::cerr << "[UDP] Failed to create socket" << std::endl;
        return false;
    }

    sockaddr_in servaddr{
        .sin_family = AF_INET, // IPv4
        .sin_port = htons(port),
        .sin_addr{INADDR_ANY}, // 所有网卡接口
    };

    int n_ret = bind(fd, (const sockaddr *)&servaddr, sizeof(servaddr));
    if (n_ret < 0)
    {
        std::cerr << "[UDP] Failed to bind, ret=" << n_ret << std::endl;
        close(fd);
        return false;
    }

    sockfd = fd;
    running = true;
    recv_worker = std::thread(&UDP::recv_loop, this);
    std::cout << "UDP started" << std::endl;
    return true;
}
void UDP::stop()
{
    running = false;
    const int fd = sockfd.exchange(-1);
    if (fd != -1)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    if (recv_worker.joinable() && recv_worker.get_id() != std::this_thread::get_id())
    {
        recv_worker.join();
    }
}
void UDP::recv_loop()
{
    const int fd = sockfd.load();
    if (fd < 0)
        return;

    uint8_t buffer[65536];
    sockaddr_in cliaddr{};
    while (running)
    {
        TimeAnalyzer timer{"UDP::recv_loop"};
        pollfd poll_fd{
            .fd = fd,
            .events = POLLIN,
            .revents = 0,
        };

        const int poll_ret = poll(&poll_fd, 1, 100);
        if (!running)
            break;
        if (poll_ret <= 0)
            continue;
        if (poll_fd.revents & (POLLERR | POLLHUP | POLLNVAL))
            break;
        if (!(poll_fd.revents & POLLIN))
            continue;

        socklen_t len = sizeof(cliaddr);
        auto n = recvfrom(fd,
                          buffer,
                          sizeof(buffer),
                          0,
                          reinterpret_cast<sockaddr *>(&cliaddr),
                          &len);
        if (n < sizeof(UDPHeader))
            continue;
        process_packet(buffer, n);
    }
    running = false;
}
void UDP::process_packet(uint8_t *raw_buf, ssize_t n)
{
    TimeAnalyzer timer{"UDP::process_packet"};
    auto header = reinterpret_cast<UDPHeader *>(raw_buf);
    std::span<uint8_t> payload = {raw_buf + sizeof(UDPHeader), raw_buf + n};
    auto ret = asm_pool.push_and_assemble(*header, payload);

#if DEBUG_ENABLED
    // std::span<uint8_t> raw_buffer_span = {raw_buf, raw_buf + n};
    // auto file_name = std::format("dump/{}-{}-{}.hex",header->frame_id,header->slice_idx,header->total_size);
    // dump_frame( raw_buffer_span, file_name);
    // std::cout<<"dumped frame "<<file_name<<std::endl;
    std::cout << std::format("{},{},{},{}",header->frame_id,header->slice_idx,header->total_size,n - sizeof(UDPHeader)) << std::endl;
#endif

    if (!ret.empty())
    {
        output_queue.push(std::vector<uint8_t>{ret.begin(), ret.end()});
#if DEBUG_ENABLED
        std::cout<<"----- valid frame -----"<<std::endl;
        dump_frame(ret, "debug_dump.hevc");
#endif
    }
}
