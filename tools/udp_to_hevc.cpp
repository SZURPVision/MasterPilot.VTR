#include "udp.h"

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1
#endif

namespace
{
volatile std::sig_atomic_t g_should_stop = 0;

void handle_signal(int)
{
    g_should_stop = 1;
}

bool parse_port(std::string_view text, uint16_t& port)
{
    int parsed = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end || parsed <= 0 || parsed > 65535)
    {
        return false;
    }
    port = static_cast<uint16_t>(parsed);
    return true;
}

void print_usage(const char* exe)
{
    std::cerr << "Usage: " << exe << " <port> <output.hevc> [--append]\n";
}
}

int main(int argc, char** argv)
{
    if (argc < 3 || argc > 4)
    {
        print_usage(argv[0]);
        return 2;
    }

    uint16_t port = 0;
    if (!parse_port(argv[1], port))
    {
        std::cerr << "Invalid UDP port: " << argv[1] << "\n";
        print_usage(argv[0]);
        return 2;
    }

    const bool append = argc == 4 && std::string_view{argv[3]} == "--append";
    if (argc == 4 && !append)
    {
        std::cerr << "Unknown option: " << argv[3] << "\n";
        print_usage(argv[0]);
        return 2;
    }

    std::ofstream out{
        argv[2],
        std::ios::binary | std::ios::out | (append ? std::ios::app : std::ios::trunc),
    };
    if (!out.is_open())
    {
        std::cerr << "Failed to open output file: " << argv[2] << "\n";
        return 1;
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    VTR::Que output_queue;
    VTR::UDP udp{output_queue};
    if (!udp.start(port))
    {
        return 1;
    }

    uint64_t frames_written = 0;
    uint64_t bytes_written = 0;
    std::atomic_bool writer_failed{false};

    std::thread writer{[&] {
        while (true)
        {
            std::vector<uint8_t> frame;
            output_queue.pop(frame);
            if (frame.empty())
            {
                if (g_should_stop || writer_failed.load())
                {
                    break;
                }
                continue;
            }

            out.write(reinterpret_cast<const char*>(frame.data()), static_cast<std::streamsize>(frame.size()));
            if (!out)
            {
                std::cerr << "Failed while writing output file\n";
                writer_failed.store(true);
                break;
            }
            out.flush();

            ++frames_written;
            bytes_written += frame.size();
            std::cout << "wrote frame " << frames_written << ", " << frame.size()
                      << " bytes, total " << bytes_written << " bytes\n";
        }
    }};

    std::cout << "Listening UDP port " << port << ", writing HEVC stream to " << argv[2] << "\n";
    while (!g_should_stop && !writer_failed.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
    }

    udp.stop();
    g_should_stop = 1;
    output_queue.push({});
    writer.join();

    std::cout << "Stopped. Frames written: " << frames_written
              << ", bytes written: " << bytes_written << "\n";
    return writer_failed.load() ? 1 : 0;
}
