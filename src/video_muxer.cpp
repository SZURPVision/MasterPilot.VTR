#include "video_muxer.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <utility>

namespace VTR
{
namespace
{
constexpr AVRational TimeBase{1, 1000 * 1000};

bool has_start_code_at(const std::vector<uint8_t>& data, std::size_t index, std::size_t& start_code_size)
{
	if (index + 3 <= data.size() &&
		data[index] == 0x00 &&
		data[index + 1] == 0x00)
	{
		if (data[index + 2] == 0x01)
		{
			start_code_size = 3;
			return true;
		}
		if (index + 4 <= data.size() && data[index + 2] == 0x00 && data[index + 3] == 0x01)
		{
			start_code_size = 4;
			return true;
		}
	}

	start_code_size = 0;
	return false;
}

std::size_t find_next_start_code(const std::vector<uint8_t>& data, std::size_t from)
{
	for (std::size_t i = from; i + 3 <= data.size(); ++i)
	{
		std::size_t start_code_size = 0;
		if (has_start_code_at(data, i, start_code_size))
			return i;
	}
	return data.size();
}
}

VideoMuxer::VideoMuxer() = default;

VideoMuxer::~VideoMuxer()
{
	stop();
}

std::string VideoMuxer::get_last_error() const
{
	std::lock_guard<std::mutex> lock(error_mutex);
	return last_error;
}

bool VideoMuxer::start(const std::string& path, int64_t offset_usec)
{
	stop();
	queue.clear();
	set_last_error({});

	output_path = path;
	session_offset_usec = std::max<int64_t>(0, offset_usec);
	stop_pts_usec = session_offset_usec;
	last_duration_usec = 33333;
	started_at = std::chrono::steady_clock::now();

	if (!open_output())
		return false;

	recording.store(true);
	worker = std::thread(&VideoMuxer::worker_loop, this);
	return true;
}

void VideoMuxer::stop()
{
	if (recording.exchange(false))
		stop_pts_usec = current_session_pts_usec();

	if (worker.joinable())
	{
		queue.push(PendingSample{
			.is_terminal = true,
		});
		if (worker.get_id() != std::this_thread::get_id())
			worker.join();
	}

	close_output();
	queue.clear();
}

bool VideoMuxer::is_recording() const
{
	return recording.load();
}

void VideoMuxer::enqueue_access_unit(std::vector<uint8_t>&& data)
{
	if (!recording.load() || data.empty())
		return;
	bool is_keyframe = is_keyframe_access_unit(data);

	queue.push(PendingSample{
		.data = std::move(data),
		.pts_usec = current_session_pts_usec(),
		.is_keyframe = is_keyframe,
	});
}

void VideoMuxer::worker_loop()
{
	PendingSample pending;
	bool has_pending = false;

	while (true)
	{
		PendingSample sample;
		queue.pop(sample);

		if (sample.is_terminal)
		{
			if (has_pending)
			{
				int64_t tail_duration = std::max<int64_t>(1, stop_pts_usec - pending.pts_usec);
				write_sample(pending, tail_duration);
			}
			break;
		}

		if (!has_pending)
		{
			pending = std::move(sample);
			has_pending = true;
			continue;
		}

		int64_t duration_usec = std::max<int64_t>(1, sample.pts_usec - pending.pts_usec);
		last_duration_usec = duration_usec;
		if (!write_sample(pending, duration_usec))
			break;

		pending = std::move(sample);
	}
}

bool VideoMuxer::open_output()
{
	close_output();

	int ret = avformat_alloc_output_context2(&format_ctx, nullptr, "mpegts", output_path.c_str());
	if (ret < 0 || format_ctx == nullptr)
	{
		set_last_error("Failed to allocate MPEG-TS output context.");
		std::cerr << "[VideoMuxer] " << get_last_error() << " path=" << output_path << std::endl;
		close_output();
		return false;
	}

	stream = avformat_new_stream(format_ctx, nullptr);
	if (stream == nullptr)
	{
		set_last_error("Failed to create output stream.");
		std::cerr << "[VideoMuxer] " << get_last_error() << std::endl;
		close_output();
		return false;
	}

	stream->id = 0;
	stream->time_base = TimeBase;
	stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	stream->codecpar->codec_id = AV_CODEC_ID_HEVC;
	stream->codecpar->format = AV_PIX_FMT_NONE;

	if ((format_ctx->oformat->flags & AVFMT_NOFILE) == 0)
	{
		ret = avio_open(&format_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			char errbuf[128];
			av_strerror(ret, errbuf, sizeof(errbuf));
			set_last_error(std::string{"Failed to open output file: "} + errbuf);
			std::cerr << "[VideoMuxer] " << get_last_error() << std::endl;
			close_output();
			return false;
		}
	}

	ret = avformat_write_header(format_ctx, nullptr);
	if (ret < 0)
	{
		char errbuf[128];
		av_strerror(ret, errbuf, sizeof(errbuf));
		set_last_error(std::string{"Failed to write MPEG-TS header: "} + errbuf);
		std::cerr << "[VideoMuxer] " << get_last_error() << std::endl;
		close_output();
		return false;
	}
	header_written = true;

	packet = av_packet_alloc();
	if (packet == nullptr)
	{
		set_last_error("Failed to allocate AVPacket.");
		std::cerr << "[VideoMuxer] " << get_last_error() << std::endl;
		close_output();
		return false;
	}

	return true;
}

void VideoMuxer::close_output()
{
	if (format_ctx != nullptr && header_written)
		av_write_trailer(format_ctx);
	header_written = false;

	if (packet != nullptr)
	{
		av_packet_free(&packet);
		packet = nullptr;
	}

	if (format_ctx != nullptr)
	{
		if ((format_ctx->oformat->flags & AVFMT_NOFILE) == 0 && format_ctx->pb != nullptr)
		{
			avio_closep(&format_ctx->pb);
		}
		avformat_free_context(format_ctx);
		format_ctx = nullptr;
		stream = nullptr;
	}
}

int64_t VideoMuxer::current_session_pts_usec() const
{
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started_at);
	return session_offset_usec + elapsed.count();
}

bool VideoMuxer::write_sample(const PendingSample& sample, int64_t duration_usec)
{
	if (format_ctx == nullptr || stream == nullptr || packet == nullptr)
		return false;
	if (sample.data.empty())
		return true;
	if (sample.data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
	{
		set_last_error("Access unit too large.");
		std::cerr << "[VideoMuxer] " << get_last_error() << " size=" << sample.data.size() << std::endl;
		return false;
	}

	av_packet_unref(packet);
	int ret = av_new_packet(packet, static_cast<int>(sample.data.size()));
	if (ret < 0)
	{
		char errbuf[128];
		av_strerror(ret, errbuf, sizeof(errbuf));
		set_last_error(std::string{"Failed to allocate packet: "} + errbuf);
		std::cerr << "[VideoMuxer] " << get_last_error() << std::endl;
		return false;
	}

	std::memcpy(packet->data, sample.data.data(), sample.data.size());
	packet->stream_index = stream->index;
	const int64_t pts = av_rescale_q(sample.pts_usec, TimeBase, stream->time_base);
	const int64_t duration = std::max<int64_t>(1, av_rescale_q(duration_usec, TimeBase, stream->time_base));
	packet->pts = pts;
	packet->dts = pts;
	packet->duration = duration;
	packet->flags = sample.is_keyframe ? AV_PKT_FLAG_KEY : 0;

	ret = av_interleaved_write_frame(format_ctx, packet);
	av_packet_unref(packet);
	if (ret < 0)
	{
		char errbuf[128];
		av_strerror(ret, errbuf, sizeof(errbuf));
		set_last_error(std::string{"Failed to write frame: "} + errbuf);
		std::cerr << "[VideoMuxer] " << get_last_error() << std::endl;
		return false;
	}

	return true;
}

void VideoMuxer::set_last_error(std::string message)
{
	std::lock_guard<std::mutex> lock(error_mutex);
	last_error = std::move(message);
}

bool VideoMuxer::is_keyframe_access_unit(const std::vector<uint8_t>& data)
{
	std::size_t cursor = 0;
	while (cursor + 5 <= data.size())
	{
		std::size_t start_code_size = 0;
		if (!has_start_code_at(data, cursor, start_code_size))
		{
			++cursor;
			continue;
		}

		std::size_t nal_header_index = cursor + start_code_size;
		if (nal_header_index + 2 > data.size())
			break;

		uint8_t nal_type = static_cast<uint8_t>((data[nal_header_index] >> 1) & 0x3F);
		if (nal_type >= 16 && nal_type <= 21)
			return true;

		cursor = find_next_start_code(data, nal_header_index + 2);
	}

	return false;
}
}
