#pragma once
#include <cstdint>

#pragma pack(push, 1)
namespace __frame_internal
{
struct FrameDataBase {
    uint16_t frame_id;
    uint16_t slice_idx;
    uint32_t total_size;
};
}
struct UDPHeader : public __frame_internal::FrameDataBase {};
struct FrameHeader : public __frame_internal::FrameDataBase {};

#pragma pack(pop)

static_assert(sizeof(UDPHeader)==8, "UDPHeader Size Wrong.");