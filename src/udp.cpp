#include "udp.h"
#include "frame.h"
#include "time_analyzer.hpp"
#include <cstdint>
#include <cerrno>
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
	stop_recording();
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

	const int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		std::cerr << "[UDP] Failed to set socket non-blocking" << std::endl;
		close(fd);
		return false;
	}

	sockfd = fd;
	running = true;
	recv_worker = std::thread(&UDP::recv_loop, this);
	std::cout << "[UDP] UDP started" << std::endl;
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

bool UDP::start_recording(const std::string& path, int64_t session_offset_usec)
{
	return muxer.start(path, session_offset_usec);
}

void UDP::stop_recording()
{
	muxer.stop();
}

bool UDP::is_recording() const
{
	return muxer.is_recording();
}

std::string UDP::get_last_recording_error() const
{
	return muxer.get_last_error();
}

void UDP::recv_loop()
{
	uint8_t buffer[65536];
	sockaddr_in cliaddr{};
	while (running)
	{
		const int fd = sockfd.load();
		if (fd < 0)
			break;

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
		if (n < 0)
		{
			if (!running)
				break;
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				continue;
			break;
		}
		if (n < static_cast<ssize_t>(sizeof(UDPHeader)))
			continue;
		process_packet(buffer, n);
	}
	running = false;
}
void UDP::process_packet(uint8_t *raw_buf, ssize_t n)
{
	TimeAnalyzer timer{"UDP::process_packet"};
	auto udp_header = reinterpret_cast<UDPHeader *>(raw_buf);
	std::span<uint8_t> payload = {raw_buf + sizeof(UDPHeader), raw_buf + n};
	//转换大小端
	FrameHeader frame_header =
	{
		{
			.frame_id = ntohs(udp_header->frame_id),
			.slice_idx = ntohs(udp_header->slice_idx),
			.total_size = ntohl(udp_header->total_size)
		}
	};
	auto ret = asm_pool.push_and_assemble(frame_header, payload);

	if (!ret.empty())
	{
		if (muxer.is_recording())
			muxer.enqueue_access_unit(std::vector<uint8_t>{ret.begin(), ret.end()});
		output_queue.push(std::vector<uint8_t>{ret.begin(), ret.end()});
	}
}
