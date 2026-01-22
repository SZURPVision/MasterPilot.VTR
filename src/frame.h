#pragma once
#include <cstdint>
#include <map>
#include <vector>

#pragma pack(push, 1)
struct UDPHeader {
    uint16_t frame_id;
    uint16_t slice_idx;
    uint32_t total_size;
};
#pragma pack(pop)

struct FrameBuffer {
    uint32_t total_size{};
    uint32_t current_received_byte{};
    std::map<uint16_t, std::vector<uint8_t>> slices;
};
