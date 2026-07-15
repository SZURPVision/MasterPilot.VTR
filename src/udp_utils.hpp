#pragma once

#include <concepts>

namespace VTR
{
    /**
     * 判断 a 是否在 b 之后（考虑回绕/逸出）
     * 约束：T 必须是无符号整数类型
     */
    template <std::unsigned_integral T>
    [[nodiscard]] constexpr bool is_later_than(T a, T b) noexcept
    {
        // 将差值强制转换为同宽度的有符号类型
        // 如果 a 领先 b 且未超过半个周期，差值在有符号下为正
        using SignedT = std::make_signed_t<T>;
        return static_cast<SignedT>(a - b) > 0;
    }

	// 暂时移除, 拔掉所有iostream符号
    // // 保存hevc裸流. 用于debug, 后续可以改造成录屏工具
    // inline void dump_frame(const std::span<uint8_t> &frame_data, const std::string& file_path)
    // {
    //     static std::ofstream dump_file(file_path, std::ios::binary | std::ios::out);
    //     if (dump_file.is_open())
    //     {
    //         dump_file.write(reinterpret_cast<const char *>(frame_data.data()), frame_data.size());
    //         dump_file.flush();
    //     }
    // }
}