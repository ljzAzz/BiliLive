#include "stdafx.h"
#include "DeCoder.h"

DeCoder::DeCoder(size_t pqSize, size_t fqSize)
	:m_packetQueueSize(pqSize)
	,m_frameQueueSize(fqSize)
	,m_running(true)
	,m_packetQueue(pqSize)
	,m_frameQueue(fqSize)
	,m_decodeClock(NAN)
	,m_pCodec(nullptr)
	,m_pCodecCtx(nullptr)
	,m_ready(false)
	,m_timeBase(NAN)
{
	for (size_t i = 0; i < fqSize; i++) {
		m_frameQueue[i] = av_frame_alloc();
	}
}

DeCoder::~DeCoder()
{
	LOG_INFO("[RoomExit] DeCoder deconstruction");
}

bool DeCoder::PushPacket(AVPacket* packet)
{
	return m_packetQueue.Push(packet);
}

void DeCoder::ShouldStop(int ret)
{
	if (ret == AVERROR_EOF) {
		LOG_INFO("[DeCoder] Live stream is over.");
		m_callback(nullptr);
	}
	else if (ret == AVERROR_EXIT) {
		LOG_INFO("[DeCoder] User actively logs out.");
		m_callback(nullptr);
	}
	else if (ret == AVERROR(EIO) || ret == AVERROR(ETIMEDOUT)) {
		LOG_INFO("[DeCoder] Network interruption or timeout.");
		m_callback(nullptr);
	}

}
