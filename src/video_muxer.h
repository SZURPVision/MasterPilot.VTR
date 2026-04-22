#pragma once

#include "safe_queue.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

namespace VTR
{
class VideoMuxer
{
public:
	VideoMuxer();
	~VideoMuxer();

	bool start(const std::string& path, int64_t session_offset_usec);
	void stop();
	bool is_recording() const;
	void enqueue_access_unit(std::vector<uint8_t>&& data);
	std::string get_last_error() const;

private:
	struct PendingSample
	{
		std::vector<uint8_t> data;
		int64_t pts_usec = 0;
		bool is_keyframe = false;
		bool is_terminal = false;
	};

	std::atomic<bool> recording{false};
	SafeQueue<PendingSample> queue;
	std::thread worker;
	std::string output_path;
	int64_t session_offset_usec = 0;
	int64_t stop_pts_usec = 0;
	int64_t last_duration_usec = 33333;
	std::chrono::steady_clock::time_point started_at{};

	AVFormatContext* format_ctx = nullptr;
	AVStream* stream = nullptr;
	AVPacket* packet = nullptr;
	bool header_written = false;
	mutable std::mutex error_mutex;
	std::string last_error;

	void worker_loop();
	bool open_output();
	void close_output();
	int64_t current_session_pts_usec() const;
	bool write_sample(const PendingSample& sample, int64_t duration_usec);
	void set_last_error(std::string message);
	static bool is_keyframe_access_unit(const std::vector<uint8_t>& data);
};
}
