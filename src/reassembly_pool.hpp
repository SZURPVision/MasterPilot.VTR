#pragma once
#include "frame.h"
#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <array>
namespace VTR
{

	
//线程不安全,记得锁
class ReassemblyPool
{
	public:
	static constexpr size_t buffer_frame_count = 128; //缓冲128帧
	static constexpr size_t mtu = 1400;
	static constexpr size_t slice_offset = mtu-sizeof(UDPHeader); //每包的偏移量
	static constexpr size_t pre_alloc_size = 720*1280*1.5;
	static constexpr size_t slice_pre_alloc_count = pre_alloc_size / slice_offset;
	static constexpr int vote_switch_threshold = 5;
	
	ReassemblyPool() = default;
	// 组装包. 组装失败会返回{}. 自带基础检查
	const std::span<uint8_t> push_and_assemble(const UDPHeader& header, const std::span<uint8_t> payload)
	{
		if(payload.size_bytes() > slice_offset || header.slice_idx >= slice_pre_alloc_count) return{};
		int16_t serial = header.frame_id / buffer_frame_count;
		int16_t hash = header.frame_id % buffer_frame_count;
		auto& fb = pool[hash];
		return fb.push_and_assemble(serial,
			header.slice_idx,
			header.total_size,
			payload);
	}

	private:
	class FrameSlot 
	{
		private:
		int16_t serial = -1; //序列号, 用于判断是不是同一包
		size_t current_received_byte = 0;
		std::bitset<slice_pre_alloc_count> slice_exist;
		std::vector<uint8_t> data;

		uint16_t vote;
		size_t total_byte = 0;
		size_t alter_total_byte = 0;
		private:
		
		public:
		inline const std::span<uint8_t> push_and_assemble(
			const int16_t frame_serial,
			const uint16_t slice_idx,
			const uint32_t req_total_byte,
			const std::span<uint8_t> slice_payload)
		{
			//新包擦除旧包
			if(serial != frame_serial)
			{
				serial = frame_serial;
				slice_exist.reset();
				total_byte = req_total_byte;
				alter_total_byte = -1;
				vote = 0;
				current_received_byte = 0;
			}

			//去重
			if(slice_exist[slice_idx]) return {};
			auto offset = slice_idx * slice_offset;
			std::ranges::copy(slice_payload,data.begin()+offset);
			slice_exist[slice_idx] = 1;
			current_received_byte+= slice_payload.size_bytes();

			//处理包大小数据损坏
			if(req_total_byte != total_byte)
			{
				if(req_total_byte == alter_total_byte)
				{
					if(++vote >= vote_switch_threshold)
					{
						total_byte = req_total_byte;
						vote = 0;
					}
				}
				else
				{
					alter_total_byte = req_total_byte;
					vote = 1;
				}
			}
			if(current_received_byte == total_byte) return data;
			else return {};
		}
		FrameSlot() : data(pre_alloc_size) {}
	};
	std::array<FrameSlot,buffer_frame_count> pool;
	uint16_t last_frame_id = 0;
};

}