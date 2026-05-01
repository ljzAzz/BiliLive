#pragma once
#include "DeCoder.h"
#include "VideoDecoder.h"
#include "AudioDecoder.h"
#include "AudioOutput.h"
#include <Network/NetWorkTypes.h>
#include "UI/RenderLayer.h"

class PlaybackController:public std::enable_shared_from_this<PlaybackController> {
private:
	std::atomic_bool m_running;

	//for DX12
	struct AVStreamInfo {
		AVBufferRef* src_device_ref = nullptr;
		AVD3D12VADeviceContext* d3d12_hwctx = nullptr;
		AVPixelFormat hw_pix_fmt = AV_PIX_FMT_D3D12; ;
		AVHWDeviceType device_type = AV_HWDEVICE_TYPE_D3D12VA;
		AVBufferRef* hw_device_ctx = nullptr;
	};
	static inline AVStreamInfo s_avStreamInfo;

private:
	//for DX12
	static AVPixelFormat GetHwFormat(AVCodecContext* ctx, const AVPixelFormat* pix_fmts);
protected:

	static inline PlayerSetting s_playerSetting;
	std::shared_ptr<VideoDeCoder> m_pVideoDecoder;
	std::shared_ptr<AudioDeCoder> m_pAudioDecoder;
	std::shared_ptr<AudioPlayer> m_pAudioPlayer;
	AVFormatContext* m_pFormatCtx;
	AVCodecContext* m_pVcodecCtx;
	AVCodecContext* m_pAcodecCtx;

	HANDLE m_event;
	std::thread m_audioOutputThread;
	std::thread m_makePacketThread;
	std::atomic_bool m_audioOutputFinish;
	std::atomic_bool m_makePacketFinish;

	std::chrono::steady_clock::time_point m_nTextureTime;
	std::chrono::milliseconds m_tickTime;
	Texture m_texture;
	bool m_first;
private:
	int m_vsIndex;
	int m_asIndex;

public:
	PlaybackController();
	~PlaybackController();
	void Start();
	bool Connect(const BliveUrl& url);
	void Stop();
	void SetVolume(int v) { m_pAudioPlayer->SetVolume(v); }
	void SetMute(bool m) { m_pAudioPlayer->SetMute(m); }
	int GetVolume() { return m_pAudioPlayer->GetVolume(); }
	bool GetMute() { return m_pAudioPlayer->GetMute(); }
	void SetStopCallback(std::function<void(void*)> cb) {
		m_pVideoDecoder->StopNotify(cb);
		m_pAudioDecoder->StopNotify(cb);
	}
	uint64_t GetTexture();
	double GetFps() const { return m_pVideoDecoder->GetFps(); }
	DX12DeCoder& GetDeCoder() { return *static_cast<DX12DeCoder*>(m_pVideoDecoder.get()); }
	static PlayerSetting& GetSetting() { return s_playerSetting; }
	static void SetVideoDecoder(VideoDecoder v) { s_playerSetting.vd = v; }
	static void SetAudioPlaer(AudioOuput a) { s_playerSetting.ap = a; }
	static void Init();
	static void Destroy();
};