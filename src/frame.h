#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct UDPHeader {
    uint16_t frame_id;
    uint16_t slice_idx;
    uint32_t total_size;
};
#pragma pack(pop)
static_assert(sizeof(UDPHeader)==8, "UDPHeader Size Wrong.");