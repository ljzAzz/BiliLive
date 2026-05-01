#include "stdafx.h"
#include "AudioOutput.h"

bool WasApiAudio::BuildFFmpegOutFmtFromWave()
{
	m_ffmpegFormat.sampleRate = m_pWaveFormatex->nSamplesPerSec;
	m_ffmpegFormat.bytesPerFrame = m_pWaveFormatex->nBlockAlign;

	// 以下处理非 EXTENSIBLE 的标准 PCM / Float
	if (m_pWaveFormatex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(m_pWaveFormatex);

		// 声道布局：优先用 dwChannelMask
		if (ext->dwChannelMask) {
			if (av_channel_layout_from_mask(&m_ffmpegFormat.channelLayout, ext->dwChannelMask) < 0)
				return false;
		}
		else {
			av_channel_layout_default(&m_ffmpegFormat.channelLayout, m_pWaveFormatex->nChannels);
		}

		// 采样格式：按 SubFormat / bits 判断
		if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
			if (m_pWaveFormatex->wBitsPerSample == 32) m_ffmpegFormat.sampleFormat = AV_SAMPLE_FMT_FLT;   // packed float
			else return false;
		}
		else if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
			switch (m_pWaveFormatex->wBitsPerSample) {
			case 16: m_ffmpegFormat.sampleFormat = AV_SAMPLE_FMT_S16; break;
			case 32: m_ffmpegFormat.sampleFormat = AV_SAMPLE_FMT_S32; break;
			default:
				// 24-bit PCM 最麻烦。共享模式下建议直接用 GetMixFormat 返回的 float。
				return false;
			}
		}
		else {
			return false;
		}
		return true;
	}


	av_channel_layout_default(&m_ffmpegFormat.channelLayout, m_pWaveFormatex->nChannels);

	if (m_pWaveFormatex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT && m_pWaveFormatex->wBitsPerSample == 32) {
		m_ffmpegFormat.sampleFormat = AV_SAMPLE_FMT_FLT;
		return true;
	}
	if (m_pWaveFormatex->wFormatTag == WAVE_FORMAT_PCM) {
		switch (m_pWaveFormatex->wBitsPerSample) {
		case 16: m_ffmpegFormat.sampleFormat = AV_SAMPLE_FMT_S16; return true;
		case 32: m_ffmpegFormat.sampleFormat = AV_SAMPLE_FMT_S32; return true;
		default: return false;
		}
	}
	return false;
}

void WasApiAudio::Init()
{
	const auto CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	IMMDeviceEnumerator* pEnumeRator;
	auto ret = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumeRator));
	if (ret != S_OK) {
		LOG_ERROR("[AudioPlayer] Failed to create device enumerator");
	}
	pEnumeRator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pAdevice);
	pEnumeRator->Release();

	LPWSTR strid;
	m_pAdevice->GetId(&strid);
	//LOG_DEBUG("[AudioPlayer] Use default device, the id is {}", strid);

	const auto IID_IAudioClient = __uuidof(IAudioClient);
	m_pAdevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&m_pAudioClient);


	m_pAudioClient->GetMixFormat(&m_pWaveFormatex);

	WAVEFORMATEX* closest = nullptr;
	HRESULT hr = m_pAudioClient->IsFormatSupported(
		AUDCLNT_SHAREMODE_SHARED,
		m_pWaveFormatex,
		&closest);

	if (hr == S_OK) {
		LOG_INFO("[AudioPlayer] Format supported exactly");
	}
	else if (hr == S_FALSE) {
		LOG_INFO("[AudioPlayer] Closest match returned");
		CoTaskMemFree(closest);
		closest = nullptr;
	}
	else {
		LOG_ERROR("[AudioPlayer] Format not supported, hr=0x{:08X}", (unsigned)hr);
	}

	if (closest) {
		CoTaskMemFree(closest);
	}

	//for minimal latency
	REFERENCE_TIME hnsRequestedDuration = 0;

	hr = m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
		hnsRequestedDuration, 0, m_pWaveFormatex, NULL);
	if (hr != S_OK) {
		LOG_ERROR("[AudioPlayer] Failed to initialize audio client");
	}

	m_pAudioClient->GetBufferSize(&m_bufferFrames);

	const GUID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
	m_pAudioClient->GetService(IID_IAudioRenderClient, (void**)&m_pArenderClient);
	hr = m_pAudioClient->GetService(IID_PPV_ARGS(&m_pAudioClock));
	if (FAILED(hr)) {
		LOG_ERROR("[AudioPlayer] Failed to get AudioClock.");
	}
	const GUID IID_ISimpleAudioVolume = __uuidof(ISimpleAudioVolume);
	hr = m_pAudioClient->GetService(IID_ISimpleAudioVolume, (void**)&m_pVolume);
	if (FAILED(hr)) {
		LOG_ERROR("[AudioPlayer] Failed to get AudioVolume.");
	}
	CoTaskMemFree(strid);
	BuildFFmpegOutFmtFromWave();
}

WasApiAudio::WasApiAudio()
	:m_totalFrames(0)
	,clockFlag(false)
{
	Init();
}

WasApiAudio::~WasApiAudio()
{
	m_pAdevice->Release();
	m_pAudioClient->Release();
	m_pArenderClient->Release();
	m_pAudioClock->Release();
	m_pVolume->Release();
	CoTaskMemFree(m_pWaveFormatex);
	LOG_INFO("[RoomExit] AudioPlayer deconstruction");

}
void WasApiAudio::SetClock(double pts)
{
	if (!clockFlag) {
		m_clock.store(pts, std::memory_order_relaxed);
		clockFlag = true;
	}
}

void WasApiAudio::Stop() {
	m_pAudioClient->Stop();
}

void WasApiAudio::SetVolume(int volume)
{
	m_pVolume->SetMasterVolume(static_cast<float>(volume) / 100.f, NULL);
}

void WasApiAudio::SetMute(bool mute)
{
	mute==true?m_pVolume->SetMute(TRUE,NULL): m_pVolume->SetMute(FALSE, NULL);
}

int WasApiAudio::GetVolume()
{
	float volume;
	m_pVolume->GetMasterVolume(&volume);
	return static_cast<int>(volume * 100);
}

bool WasApiAudio::GetMute()
{
	BOOL mute = false;
	m_pVolume->GetMute(&mute);
	return mute==TRUE;
}


void WasApiAudio::Register(HANDLE event)
{
	m_pAudioClient->SetEventHandle(event);
}

uint32_t WasApiAudio::GetBufferPointer(uint8_t** buffer)
{
	UINT32 padding;
	m_pAudioClient->GetCurrentPadding(&padding); 
	auto frames = m_bufferFrames - padding; 
	m_pArenderClient->GetBuffer(frames, buffer);
	return frames;
}

double WasApiAudio::GetClock() const
{
	if (std::isnan(m_clock.load(std::memory_order_relaxed))) {
		return NAN;
	}

	uint32_t p;
	m_pAudioClient->GetCurrentPadding(&p);
	return m_clock.load(std::memory_order_relaxed) + (double)(m_totalFrames.load(std::memory_order_relaxed) - p) / (double)m_pWaveFormatex->nSamplesPerSec;
}

void WasApiAudio::Release(int frameNum)
{
	m_pArenderClient->ReleaseBuffer(frameNum, 0);
	m_totalFrames.fetch_add(frameNum,std::memory_order_relaxed);
}
