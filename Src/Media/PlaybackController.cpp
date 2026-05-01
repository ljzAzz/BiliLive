#include "stdafx.h"
#include "Platform/WasApi/WasApi.h"
#include "PlaybackController.h"

AVPixelFormat PlaybackController::GetHwFormat(AVCodecContext* ctx, const AVPixelFormat* pix_fmts)
{
	const enum AVPixelFormat* p;
	AVStreamInfo* pSI = (AVStreamInfo*)ctx->opaque;
	for (p = pix_fmts; *p != -1; p++) {
		if (*p == pSI->hw_pix_fmt) {
			return *p;
		}
	}
	return AV_PIX_FMT_D3D12;
	LOG_ERROR("[PlaybackController] Failed to get HW surface format. Using Default format");
}

PlaybackController::PlaybackController()
	:m_running(true)
	,m_vsIndex(0)
	,m_asIndex(0)
	,m_pFormatCtx(nullptr)
	,m_audioOutputFinish(false)
	,m_makePacketFinish(false)
	,m_pVcodecCtx(nullptr)
	,m_pAcodecCtx(nullptr)
	,m_first(false)
{
	m_event = CreateEvent(nullptr, false, false, nullptr);

	switch (s_playerSetting.ap)
	{
		case AudioOuput::WasApi:
			m_pAudioPlayer = std::shared_ptr<AudioPlayer>(new WasApiAudio());
			static_cast<WasApiAudio*>(m_pAudioPlayer.get())->Register(m_event);
			break;
		default:
			break;
	}

	switch (s_playerSetting.vd) {
		case VideoDecoder::DX12VideoProcessor:
			m_pVideoDecoder = std::shared_ptr<VideoDeCoder>(new DX12DeCoder(s_playerSetting.videoPacketSize, s_playerSetting.videoFrameSize, *m_pAudioPlayer));
			break;
		case VideoDecoder::DX12Shader:
			m_pVideoDecoder = std::shared_ptr<VideoDeCoder>(new DX12PixelShaderDeCoder(s_playerSetting.videoPacketSize, s_playerSetting.videoFrameSize, *m_pAudioPlayer));
			break;
		default:
			break;
	}
	m_pAudioDecoder = std::shared_ptr<AudioDeCoder>(new AudioDeCoder(s_playerSetting.audioPacketSize, s_playerSetting.audioFrameSize, m_pVideoDecoder->GetDecodeClock()));
}

PlaybackController::~PlaybackController()
{
	if (m_pVcodecCtx) {
		if(m_pVcodecCtx->hw_device_ctx)
			LOG_DEBUG("[RoomExit] codecCtx  hw_device_ctx refconut is {}", av_buffer_get_ref_count(m_pVcodecCtx->hw_device_ctx));
		if (m_pVcodecCtx->hw_frames_ctx)
			LOG_DEBUG("[RoomExit] codecCtx  hw_frames_ctx refconut is {}", av_buffer_get_ref_count(m_pVcodecCtx->hw_frames_ctx));
	}

	avcodec_free_context(&m_pVcodecCtx);
	avcodec_free_context(&m_pAcodecCtx);

	avformat_free_context(m_pFormatCtx);
	CloseHandle(m_event);

	LOG_INFO("[RoomExit] PlaybackController deconstruction");
}

void PlaybackController::Start()
{
	int ret = 0;
	//find stream info
	try {
		 ret = avformat_find_stream_info(m_pFormatCtx, nullptr);
	}
	catch (const std::exception& e) {
		LOG_INFO("[PlaybackController] Can't find stream info ,maybe room is not aliving");
		return;
		//InfoRoomSession() //todo
	}

	if (ret < 0) {
		LOG_ERROR("[DeCoder] Failed to find stream info");
		exit(1);
	}
	for (unsigned int i = 0; i < m_pFormatCtx->nb_streams; i++) {
		if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			m_vsIndex = i;
		}
		else if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			m_asIndex = i;
		}
	}

	//find video decoder
	const AVCodec* vcodec;
	ret = av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);
	if (ret < 0) {
		LOG_ERROR("[DeCoder] Failed to find video stream");
	}
	else {
		m_vsIndex = ret;
	}
	auto vstream = m_pFormatCtx->streams[m_vsIndex];

	if (vcodec == nullptr) {
		LOG_ERROR("[DeCoder] Video codec not found for ID: {}", (int)vstream->codecpar->codec_id);
		avformat_close_input(&m_pFormatCtx);
		return;
	}
	AVRational fps = av_guess_frame_rate(m_pFormatCtx, vstream, NULL);

	//find audio decoder
	const AVCodec* acodec;
	ret = av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &acodec, 0);
	if (ret < 0) {
		LOG_ERROR("[DeCoder] Failed to find audio stream");
	}
	else {
		m_asIndex = ret;
	}
	auto astream = m_pFormatCtx->streams[m_asIndex];

	if (acodec == nullptr) {
		LOG_ERROR("[DeCoder] Audio codec not found for ID: {}", (int)astream->codecpar->codec_id);
		avformat_close_input(&m_pFormatCtx);
		return;
	}

	//find hardware decoder
	for (int i = 0;; i++) {
		const AVCodecHWConfig* config = avcodec_get_hw_config(vcodec, i);
		if (!config) {
			LOG_ERROR("[Decoder] {} does not support device type {}.", vcodec->name, av_hwdevice_get_type_name(s_avStreamInfo.device_type));
			return;
		}
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
			config->device_type == s_avStreamInfo.device_type) {
			//m_AVStreamInfo.hw_pix_fmt = config->pix_fmt;
			LOG_INFO("[Decoder] hw_pix_fmt {}.", av_get_pix_fmt_name(config->pix_fmt));
			break;
		}
	}

	//allocate video codec context
	auto vcodecCtx = avcodec_alloc_context3(vcodec);
	if (!vcodecCtx) {
		LOG_ERROR("[DeCoder] Could not allocate video codec context");
		avformat_close_input(&m_pFormatCtx);
		return;
	}
	
	if (avcodec_parameters_to_context(vcodecCtx, vstream->codecpar)< 0) {
		avformat_close_input(&m_pFormatCtx);
		avcodec_free_context(&vcodecCtx);
		return;
	}
	vcodecCtx->thread_count = s_playerSetting.vThreadNum;
	vcodecCtx->opaque = static_cast<void*>(&s_avStreamInfo);
	vcodecCtx->get_format = GetHwFormat;

	auto der = av_hwdevice_ctx_alloc(s_avStreamInfo.device_type);

	ret = av_hwdevice_ctx_create_derived(&der, s_avStreamInfo.device_type, s_avStreamInfo.hw_device_ctx, 0);
	av_hwdevice_ctx_init(der);
	vcodecCtx->hw_device_ctx = der;

	//
	//framesCtx->initial_pool_size = 4; //for performance
	//av_opt_set_int(vcodecCtx->priv_data, "buffer_size", 1024 * 1024 * 5, AV_OPT_SEARCH_CHILDREN); //for performance
	//av_opt_set_int(vcodecCtx->priv_data, "flags2", AV_CODEC_FLAG2_FAST, AV_OPT_SEARCH_CHILDREN);
	//av_opt_set_int(vcodecCtx->priv_data, "hw_surface_count", 8, AV_OPT_SEARCH_CHILDREN);
	//vcodecCtx->thread_type = FF_THREAD_FRAME;


	if (int error = avcodec_open2(vcodecCtx, vcodec, nullptr); error < 0) {
		LOG_ERROR("[DeCoder] Could not open input codec (error {})", error);
		avcodec_free_context(&vcodecCtx);
		avformat_close_input(&m_pFormatCtx);
		return;
	}

	//allocate audio codec context
	auto acodecCtx = avcodec_alloc_context3(acodec);
	if (!acodecCtx) {
		LOG_ERROR("[DeCoder] Could not allocate audio codec context");
		avformat_close_input(&m_pFormatCtx);
		return;
	}
	if (avcodec_parameters_to_context(acodecCtx, astream->codecpar) < 0) {
		avformat_close_input(&m_pFormatCtx);
		avcodec_free_context(&vcodecCtx);
		avcodec_free_context(&acodecCtx);
		return;
	}
	acodecCtx->thread_count = s_playerSetting.aThreadNum;

	av_opt_set_int(acodecCtx->priv_data, "buffer_size", 1024 * 1024 * 5, AV_OPT_SEARCH_CHILDREN); //for performance
	if (avcodec_open2(acodecCtx, acodec, nullptr) < 0) {
		LOG_ERROR("[DeCoder] Could not open audio codec");
		avcodec_free_context(&vcodecCtx);
		avcodec_free_context(&acodecCtx);
		avformat_close_input(&m_pFormatCtx);
		return;
	}
	m_pVideoDecoder->SetCodec(vcodec);
	m_pVideoDecoder->SetCodecCtx(vcodecCtx);
	m_pVideoDecoder->SetTimeBase(vstream->time_base);
	m_pVideoDecoder->SetFps(fps);
	m_pAudioDecoder->SetCodec(acodec);
	m_pAudioDecoder->SetCodecCtx(acodecCtx);
	m_pAudioDecoder->SetTimeBase(astream->time_base);

	auto& ffmpegFormat = m_pAudioPlayer->GetFFmpegFormat();
	m_pAudioDecoder->SetOutFormt(ffmpegFormat);
	m_pVideoDecoder->StartDecode();
	m_pAudioDecoder->StartDecode();
	
	m_tickTime = std::chrono::milliseconds(static_cast<int>((static_cast<float>(fps.den) / static_cast<float>(fps.num)) * 1000));
	m_nTextureTime = std::chrono::steady_clock::now();

	m_audioOutputThread = std::thread([self = shared_from_this()]() {
		while (!self->m_pAudioDecoder->Ready()&& self->m_running.load(std::memory_order_relaxed)==true) {
			continue;
		}
		self->m_pAudioPlayer->Start();
		while (self->m_running.load(std::memory_order_relaxed)==true) {
			WaitForSingleObject(self->m_event, INFINITE);
			uint8_t* buffer;
			auto frameNum = self->m_pAudioPlayer->GetBufferPointer(&buffer);
			if (buffer == nullptr) {
				continue;
			}
			auto pts = self->m_pAudioDecoder->FillData(buffer, frameNum);
			self->m_pAudioPlayer->Release(frameNum);
			self->m_pAudioPlayer->SetClock(pts);
		}
		self->m_audioOutputFinish.store(true, std::memory_order_relaxed);
		self->m_audioOutputFinish.notify_all();
		LOG_INFO("[RoomExit] Audio output thread exit");
	});

	m_makePacketThread = std::thread([self = shared_from_this()]() {
		AVPacket* gpacket = nullptr;
		while (self->m_running.load(std::memory_order_relaxed)==true) {
			AVPacket* packet = av_packet_alloc();
			gpacket = packet;
			int ret = av_read_frame(self->m_pFormatCtx, packet);
			if (ret < 0 && self->m_running.load(std::memory_order_relaxed) == true) {
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					av_packet_free(&packet);
					continue;
				}
				else {
					LOG_ERROR("[PlaybackController] av_read_frame error: {}", ret);
					std::this_thread::sleep_for(std::chrono::seconds(1));
					av_packet_free(&packet);
					continue;
				}
			}

			if (packet->stream_index == self->m_vsIndex) {
				self->m_pVideoDecoder->PushPacket(packet);
				gpacket = nullptr;
			}
			else if (packet->stream_index == self->m_asIndex) {
				self->m_pAudioDecoder->PushPacket(packet);
				gpacket = nullptr;
			}
		}
		if (gpacket) {
			av_packet_free(&gpacket);
		}
		avformat_close_input(&self->m_pFormatCtx);
		self->m_makePacketFinish.store(true, std::memory_order_relaxed);
		self->m_makePacketFinish.notify_all();
		LOG_INFO("[RoomExit] Make packet thread exit");
	});
	m_pVcodecCtx = vcodecCtx;
	m_pAcodecCtx = acodecCtx;
	m_audioOutputThread.detach();
	m_makePacketThread.detach();
}

bool PlaybackController::Connect(const BliveUrl& url)
{
	AVDictionary* opts = nullptr;
	//av_dict_set(&opts, "fflags", "nobuffer", 0);
	//av_dict_set(&opts, "flags", "low_delay", 0);
	av_dict_set(&opts, "probesize", "50000000", 0);
	av_dict_set(&opts, "analyzeduration", "10000000", 0);
	av_dict_set(&opts, "buffer_size", "512000", 0);
	av_dict_set(&opts, "user_agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0", 0);
	av_dict_set(&opts, "reconnect", "1", 0);
	av_dict_set(&opts, "reconnect_streamed", "1", 0);
	av_dict_set(&opts, "reconnect_on_network_error", "1", 0);
	av_dict_set(&opts, "reconnect_delay_max", "2", 0);
	av_dict_set(&opts, "rw_timeout", "5000000", 0);
	av_dict_set(&opts, "multiple_requests", "1", 0);
	auto ret = avformat_open_input(&m_pFormatCtx, url.url.c_str(), nullptr, &opts);
	av_dict_free(&opts);
	if (ret >= 0) {
		LOG_INFO("[PlaybackController] Connect to {}", url.url);
		return true;
	}
	else {
		avformat_close_input(&m_pFormatCtx);
		avformat_free_context(m_pFormatCtx);
		return false;
	}

}

void PlaybackController::Stop()
{
	m_running.store(false, std::memory_order_relaxed);
	m_pAudioPlayer->Stop();
	m_pVideoDecoder->Stop();
	m_pAudioDecoder->Stop();
	while (m_makePacketFinish.load(std::memory_order_relaxed) == false) {
		m_makePacketFinish.wait(false, std::memory_order_relaxed);
	}
	while (m_audioOutputFinish.load(std::memory_order_relaxed) == false) {
		m_audioOutputFinish.wait(false, std::memory_order_relaxed);
	}

}

uint64_t PlaybackController::GetTexture()
{
	auto audioTime = m_pAudioPlayer->GetClock();
	static constexpr auto HOLD = std::chrono::milliseconds(1);
	if (std::isnan(audioTime)) {
		return 0;
	}
	auto now = std::chrono::steady_clock::now();
	if (m_first == true && now < m_nTextureTime - HOLD) {
		return m_texture.textureHandle;
	}
	if (m_first == true && now < m_nTextureTime) {
		auto sleepTime = static_cast<int>(static_cast<std::chrono::duration<double>>(m_nTextureTime - now).count() * 1000000) - 500;
		if (sleepTime > 0) {
			std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
		}
	}
	if ((m_first == false && m_texture.pts == 0) || (m_first && m_texture.pts != 0)) {
		m_texture = m_pVideoDecoder->GetTextureHandle();
	}
	auto diff = m_texture.pts - audioTime;
	if (diff < -0.04) {
		if (m_first == false) {
			return 0;
		}
		while (diff < -0.04) {
			m_texture = m_pVideoDecoder->GetTextureHandle();
			diff = m_texture.pts - audioTime;
		}
	}
	else if (diff > 0.02) {
		if (m_texture.pts == 0) {
			return 0;
		}
		int sleepTime = static_cast<int>((diff - 0.02) * 1000000) - 500;
		if (sleepTime > 0) {
			std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
		}
	}
	m_nTextureTime = now + m_tickTime;

	m_first = true;
	return m_texture.textureHandle;
}

void PlaybackController::Init()
{
	avformat_network_init();
	AVBufferRef* devRef = av_hwdevice_ctx_alloc(s_avStreamInfo.device_type);
	if (!devRef) {
		LOG_ERROR("[PlaybackController] Failed to alloc hwdevice ctx");
		return;
	}
	//int ret = av_hwdevice_ctx_create(&devRef, s_avStreamInfo.device_type, nullptr, nullptr, 0);
	//if (ret < 0) {
	//	LOG_ERROR("[PlaybackController] Failed to create hwdevice ctx");
	//}
	auto* hwdev = reinterpret_cast<AVHWDeviceContext*>(devRef->data);
	auto* d3d12 = reinterpret_cast<AVD3D12VADeviceContext*>(hwdev->hwctx);

	Microsoft::WRL::ComPtr<ID3D12Device> dev = DX12::GetDevice();
	d3d12->device = dev.Get();

	int ret = av_hwdevice_ctx_init(devRef);
	if (ret < 0) {
		av_buffer_unref(&devRef);
		LOG_ERROR("[PlaybackController] Failed to init hwdevice ctx");
		return;
	}
	s_avStreamInfo.hw_device_ctx = devRef;
	LOG_DEBUG("[PlaybackController] hw_device_ctx refconut is {}", av_buffer_get_ref_count(s_avStreamInfo.hw_device_ctx));


	switch (s_playerSetting.ap)
	{
		case AudioOuput::WasApi:
			WasApi::Init();
			break;
		default:
			break;
	}
}

void PlaybackController::Destroy()
{
	avformat_network_deinit();
	switch (s_playerSetting.ap)
	{
	case AudioOuput::WasApi:
		WasApi::ShutDown();
		break;
	default:
		break;
	}
}
