#pragma once
#include "DeCoder.h"
#include "MediaTypes.h"

class AudioDeCoder:public std::enable_shared_from_this<AudioDeCoder>,public DeCoder{
private:

	AudioOutFmt m_outFormat;
	struct AFrameCtx {
		AVFrame* frame;
		uint8_t* buffer = nullptr;
		uint32_t frameNum = 0;
		uint32_t maxFrame = 0;
		uint32_t offset = 0;
		double pts;
		std::atomic_bool ready = false;
	};

	std::vector<AFrameCtx> m_frameCtx;

	std::thread m_decodeThread;
	std::atomic_bool m_decodeFinish;

	//---------FFmpeg------------

	SwrContext* m_pSwrCtx;
	//---------FFmpeg------------

	size_t m_nowBufferIndex;
	const std::atomic<double>& m_videoDeCoderClock;

private:
	void TransAFrame(AVFrame* f, size_t index);
	void PreAllocBuffer(AVFrame* f);
	void InitSwrContext();
public:
	AudioDeCoder(size_t pqSize, size_t fqSize, const std::atomic<double>& videoDecoderClock);
	~AudioDeCoder();
	void SetOutFormt(const AudioOutFmt& of) { m_outFormat = of; InitSwrContext(); }
	virtual void StartDecode() override;
	virtual void Stop() override;

	double FillData(uint8_t*& buffer, uint32_t frameNum);
};