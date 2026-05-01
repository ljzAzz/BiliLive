//wasapi
#include "Platform/WasApi/WasApi.h"

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
#ifdef __cplusplus
}
#endif

#include "MediaTypes.h"

class AudioPlayer{
protected:
	AudioOutFmt m_ffmpegFormat;
	std::atomic<double> m_clock;

public:
	AudioPlayer() :m_clock(NAN) {}
	const AudioOutFmt& GetFFmpegFormat() const { return m_ffmpegFormat; }
	virtual double GetClock() const = 0;
	virtual double GetFirstPts() const { return m_clock.load(std::memory_order_relaxed); }
	virtual uint32_t GetBufferPointer(uint8_t** buffer) = 0 ;
	virtual void Release(int frameNum) = 0;
	virtual void SetClock(double pts) = 0;
	
	virtual void Stop() = 0;
	virtual void Start() = 0;
	virtual void SetVolume(int volume) = 0;
	virtual void SetMute(bool mute) = 0;
	virtual int GetVolume() = 0;
	virtual bool GetMute() = 0;
};

class WasApiAudio: public AudioPlayer {
private:
	IMMDevice* m_pAdevice;
	IAudioClient* m_pAudioClient;
	WAVEFORMATEX* m_pWaveFormatex;
	IAudioRenderClient* m_pArenderClient;
	IAudioClock* m_pAudioClock;
	ISimpleAudioVolume* m_pVolume;
	uint32_t m_bufferFrames;
	std::atomic_uint64_t m_totalFrames;
	bool clockFlag;
private:
	bool BuildFFmpegOutFmtFromWave();
	void Init();
public:
	WasApiAudio();
	~WasApiAudio();
	void Register(HANDLE event);
	virtual uint32_t GetBufferPointer(uint8_t** buffer) override;
	virtual void Start() override { m_pAudioClient->Start(); }
	virtual double GetClock() const override;
	virtual void Release(int frameNum) override;
	virtual void SetClock(double pts) override;
	virtual void Stop() override;
	virtual void SetVolume(int volume) override;
	virtual void SetMute(bool mute) override;
	virtual int GetVolume() override;
	virtual bool GetMute() override;
};