#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>

#ifdef __cplusplus
}
#endif

struct AudioOutFmt {
	AVChannelLayout channelLayout{};
	AVSampleFormat  sampleFormat = AV_SAMPLE_FMT_NONE;
	int sampleRate = 0;
	int bytesPerFrame = 0;
};

enum class VideoDecoder:int {
	DX12VideoProcessor=0,
	DX12Shader,
	null
};

enum class AudioOuput:int {
	WasApi=0,
	null
};

struct PlayerSetting {
	VideoDecoder vd = VideoDecoder::DX12VideoProcessor;
	AudioOuput ap = AudioOuput::WasApi;
	size_t videoPacketSize = 600;
	size_t videoFrameSize = 3;
	size_t audioPacketSize = 600;
	size_t audioFrameSize = 3;
	int vThreadNum = 1;
	int aThreadNum = 1;
	int audioApiIndex = 0;
	int videoApiIndex = 0;
};