#include "stdafx.h"
#include "AudioDecoder.h"

void AudioDeCoder::TransAFrame(AVFrame* frame, size_t index)
{
	int converted = swr_convert(m_pSwrCtx, &m_frameCtx[index].buffer, m_frameCtx[index].maxFrame, (const uint8_t**)frame->extended_data, frame->nb_samples);
	m_frameCtx[index].frameNum = converted;
	m_frameCtx[index].offset = 0;
	m_frameCtx[index].pts = frame->pts * m_timeBase;
	if (converted < 0) {
		LOG_ERROR("[DeCoder]: Failed to convert audio format.");
	}
}

void AudioDeCoder::PreAllocBuffer(AVFrame* f)
{
	int maxInSamples = m_pCodecCtx->frame_size;
	if (maxInSamples == 0) {
		maxInSamples = 8192;
	}

	int64_t delay = swr_get_delay(m_pSwrCtx, m_pCodecCtx->sample_rate);
	if (delay < 0) delay = 0;

	int maxOutSamples = static_cast<int>(
		av_rescale_rnd(delay + maxInSamples,
			m_outFormat.sampleRate,
			m_pCodecCtx->sample_rate,
			AV_ROUND_UP)
		);

	constexpr int SAFETY_MARGIN = 256; 
	maxOutSamples += SAFETY_MARGIN;

	int maxOutBufferSize = av_samples_get_buffer_size(
		nullptr,
		m_outFormat.channelLayout.nb_channels,
		maxOutSamples,
		m_outFormat.sampleFormat,
		1
	);
	if (maxOutBufferSize < 0) {
		LOG_ERROR("[AudioDecoder] Failed to get output buffer size");
		return; 
	}

	for (auto& ctx : m_frameCtx) {
		ctx.buffer = (uint8_t*)av_malloc(maxOutBufferSize);
		ctx.maxFrame = maxOutSamples;
		ctx.offset = 0;
		if (!ctx.buffer) {
			LOG_ERROR("[AudioDecoder] Failed to allocate buffer");
			return;
		}
	}
}

void AudioDeCoder::InitSwrContext()
{
	int r = swr_alloc_set_opts2(
		&m_pSwrCtx,
		&m_outFormat.channelLayout,
		m_outFormat.sampleFormat,
		m_outFormat.sampleRate,
		&m_pCodecCtx->ch_layout,
		m_pCodecCtx->sample_fmt,
		m_pCodecCtx->sample_rate,
		0, nullptr);
	if (r) {
		LOG_ERROR("[DeCoder]: Failed to set swr options");
		return;
	}
	if (swr_init(m_pSwrCtx)) {
		LOG_ERROR("[DeCoder]: Failed to init swr context");
		return;
	}
}

AudioDeCoder::AudioDeCoder(size_t pqSize, size_t fqSize, const std::atomic<double>& videoDecoderClock)
	:DeCoder(pqSize, fqSize)
	,m_videoDeCoderClock(videoDecoderClock)
	,m_frameCtx(fqSize)
	,m_pSwrCtx(nullptr)
	,m_nowBufferIndex(0)
	,m_decodeFinish(false)
{
	size_t i = 0;
	for (auto& ctx : m_frameCtx) {
		ctx.frame = m_frameQueue[i++];
	}
}

AudioDeCoder::~AudioDeCoder()
{
	for (auto& ctx : m_frameCtx) {
		if (ctx.buffer) {
			av_free(ctx.buffer);
		}
	}
	if (m_pSwrCtx) {
		swr_free(&m_pSwrCtx);
	}
	LOG_INFO("[RoomExit] AudioDecoder deconstruction");

}


void AudioDeCoder::StartDecode()
{
	LOG_DEBUG("[AudioDeCoder] Time Base is {}s", m_timeBase);
	m_decodeThread = std::thread([=,self = shared_from_this()]() {
		std::once_flag flag;
		AVPacket* packet = nullptr;
		size_t num = 0;
		size_t index = 0;

		while (std::isnan(self->m_videoDeCoderClock.load(std::memory_order_relaxed))&& 
			self->m_running.load(std::memory_order_relaxed) == true) {
			continue;
		}
		while (self->m_running.load(std::memory_order_relaxed) == true) {
			if (!self->m_packetQueue.Pop(packet) || packet == nullptr) {
				continue;
			}
			int ret = avcodec_send_packet(self->m_pCodecCtx, packet);
			av_packet_free(&packet);
			self->ShouldStop(ret);
			while (avcodec_receive_frame(self->m_pCodecCtx, self->m_frameCtx[index].frame) >= 0 
				&& self->m_running.load(std::memory_order_relaxed) == true) {
				self->ShouldStop(ret);
				if(ret)
				if (self->m_frameCtx[index].frame->pts * self->m_timeBase < self->m_videoDeCoderClock.load(std::memory_order_relaxed)) {
					continue;
				}
				std::call_once(flag, [&]() {
					LOG_DEBUG("[AudioDeCoder]: First pts is : {}s", self->m_frameCtx[index].frame->pts * self->m_timeBase);
					self->PreAllocBuffer(self->m_frameCtx[index].frame);
					});
				self->TransAFrame(self->m_frameCtx[index].frame, index);
				num++;
				if (num == self->m_frameQueueSize) {
					self->m_ready.store(true, std::memory_order_relaxed);
					self->m_ready.notify_all();
				}
				self->m_frameCtx[index].ready.store(true, std::memory_order_relaxed);
				self->m_frameCtx[index].ready.notify_all();
				index = (index + 1) % self->m_frameQueueSize;
				while (self->m_frameCtx[index].ready.load(std::memory_order_relaxed) == true) {
					self->m_frameCtx[index].ready.wait(true, std::memory_order_relaxed);
					av_frame_unref(self->m_frameCtx[index].frame);
				}
			}
		}
		if (packet != nullptr) {
			av_packet_free(&packet);
		}
		avcodec_send_packet(self->m_pCodecCtx, nullptr);
		AVFrame* frame = av_frame_alloc();
		while (avcodec_receive_frame(self->m_pCodecCtx, frame) >= 0) {
			av_frame_unref(frame);
		}
		av_frame_free(&frame);
		avcodec_flush_buffers(self->m_pCodecCtx);
		self->m_decodeFinish.store(true, std::memory_order_relaxed);
		self->m_decodeFinish.notify_all();
		LOG_INFO("[RoomExit] Audio decoder thread exit");
	});
	m_decodeThread.detach();
}

void AudioDeCoder::Stop()
{
	m_running.store(false, std::memory_order_relaxed);
	for (auto& ctx : m_frameCtx) {
		ctx.ready.store(false, std::memory_order_relaxed);
		ctx.ready.notify_all();
	}
	while (m_decodeFinish.load(std::memory_order_relaxed) == false) {
		m_decodeFinish.wait(false, std::memory_order_relaxed);
	}
	for (size_t i = 0; i < m_frameQueueSize; i++) {
		av_frame_free(&m_frameQueue[i]);
	}
	auto size = m_packetQueue.GetContentSize();
	AVPacket* packet = nullptr;
	for (size_t i = 0; i < size; i++) {
		if (m_packetQueue.Pop(packet) && packet != nullptr) {
			av_packet_free(&packet);
		}
	}
}

//should be managed by playbackcontroller
double AudioDeCoder::FillData(uint8_t*& buffer, uint32_t frameNum)
{
	int framesRemaining = frameNum;
	uint8_t* destPtr = buffer;

	while (framesRemaining > 0) {
		auto& ctx = m_frameCtx[m_nowBufferIndex];

		if (!ctx.ready.load(std::memory_order_acquire)) {
			break;
		}

		int available = ctx.frameNum - ctx.offset;
		if (available <= 0) {
			ctx.ready.store(false, std::memory_order_release);
			ctx.ready.notify_all();
			m_nowBufferIndex = (m_nowBufferIndex + 1) % m_frameQueueSize;
			continue;
		}

		int copyFrames = std::min(available, framesRemaining);
		std::memcpy(destPtr,
			ctx.buffer + ctx.offset * m_outFormat.bytesPerFrame,
			copyFrames * m_outFormat.bytesPerFrame);

		ctx.offset += copyFrames;
		destPtr += copyFrames * m_outFormat.bytesPerFrame;
		framesRemaining -= copyFrames;

		if (ctx.offset >= ctx.frameNum) {
			ctx.ready.store(false, std::memory_order_release);
			ctx.ready.notify_all();

			m_nowBufferIndex = (m_nowBufferIndex + 1) % m_frameQueueSize;
		}
	}

	if (framesRemaining > 0) {
		std::memset(destPtr, 0, framesRemaining * m_outFormat.bytesPerFrame);
	}
	return m_frameCtx[m_nowBufferIndex].pts;
}
