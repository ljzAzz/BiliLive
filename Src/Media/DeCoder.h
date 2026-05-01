#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavdevice/avdevice.h>
#include <libavutil/hwcontext.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext_d3d12va.h>
#include <libavutil/buffer.h>
#ifdef __cplusplus
}
#endif

#include "Common/Common.h"


class DeCoder {
protected:
	size_t m_packetQueueSize;
	size_t m_frameQueueSize;
	std::atomic_bool m_running;
	std::atomic_bool m_ready;
	std::atomic<double> m_decodeClock;

	LockFreeQueue<AVPacket*> m_packetQueue;
	std::vector<AVFrame*> m_frameQueue;

	const AVCodec* m_pCodec;
	AVCodecContext* m_pCodecCtx;
	double m_timeBase;

	std::function<void(void* opaque)> m_callback;
public:
	DeCoder(size_t pqSize, size_t fqSize);
	virtual ~DeCoder();
	bool PushPacket(AVPacket* packet);
	virtual void StartDecode() = 0;
	virtual void Stop() = 0;
	virtual void StopNotify(std::function<void(void* opaque)> callback) { m_callback = callback;}
	virtual void ShouldStop(int ret);
	const std::atomic<double>& GetDecodeClock() const { return m_decodeClock; }
	bool Ready() const { return m_ready.load(std::memory_order_relaxed); }
	void SetCodec(const AVCodec* codec) { m_pCodec = codec; }
	void SetCodecCtx(AVCodecContext* codecCtx) { m_pCodecCtx = codecCtx; }
	void SetTimeBase(AVRational tb) { m_timeBase = av_q2d(tb); }
};