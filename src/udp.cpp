#include "udp.h"
#include "frame.h"
#include "time_analyzer.hpp"
#include "udp_utils.hpp"
#include <expected>
#include <iostream>
#include <ostream>
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
    std::cout << "UDP started" << std::endl;
    if (running)
        return true;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr{
        .sin_family = AF_INET, // IPv4
        .sin_port = htons(port),
        .sin_addr{INADDR_ANY}, // 所有网卡接口
    };

    int n_ret = bind(sockfd, (const sockaddr *)&servaddr, sizeof(servaddr));
    if (n_ret < 0)
    {
        std::cerr << "[UDP] Failed to bind, ret=" << n_ret << std::endl;
        close(sockfd);
        return false;
    }

    running = true;
    recv_worker = std::thread(&UDP::recv_loop, this);
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
    while (running)
    {
        TimeAnalyzer timer{"UDP::recv_loop"};
        auto n = recvfrom(sockfd,
                          buffer,
                          sizeof(buffer),
                          0,
                          reinterpret_cast<sockaddr *>(&cliaddr),
                          &len);
        if (n < sizeof(UDPHeader))
            continue;
        process_packet(buffer, n);
    }
    close(sockfd);
    sockfd = 0;
}
void UDP::process_packet(uint8_t *raw_buf, ssize_t n)
{
    TimeAnalyzer timer{"UDP::process_packet"};
    auto header = reinterpret_cast<UDPHeader *>(raw_buf);
    std::span<uint8_t> payload = {raw_buf + sizeof(UDPHeader), raw_buf + n};
    auto ret = asm_pool.push_and_assemble(*header, payload);
    if (!ret.empty())
    {
        output_queue.push(ret);
    }
}