#include "stdafx.h"
#include "AudioOutput.h"
#include "VideoDecoder.h"
#include <Render/Dx12RenderBackend.h>
#include <comdef.h>

#pragma comment(lib,"comsuppwd.lib")

bool DX12DeCoder::CheckSupport()
{
	D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT support = {};
	support.NodeIndex = 0;

	support.InputSample.Width = m_dxgiMeta.width;
	support.InputSample.Height = m_dxgiMeta.height;
	support.InputSample.Format.Format = DXGI_FORMAT_NV12;
	support.InputSample.Format.ColorSpace = m_dxgiMeta.indxgiColorSpace;

	support.InputFieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
	support.InputStereoFormat = D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE;
	support.InputFrameRate = m_dxgiMeta.fps;

	support.OutputFormat.Format = m_dxgiMeta.format;
	support.OutputFormat.ColorSpace = m_dxgiMeta.outdxgiColorSpace;
	support.OutputStereoFormat = D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE;
	support.OutputFrameRate = m_dxgiMeta.fps;

	if (m_pDx12VideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_SUPPORT, &support, sizeof(support)) != S_OK) {
		LOG_ERROR("[DX12]: Failed to check video process support.");
		return false;
	}

	return (support.SupportFlags & D3D12_VIDEO_PROCESS_SUPPORT_FLAG_SUPPORTED) != 0;
}

void DX12DeCoder::InitDXGIMeta(AVFrame* f)
{
	m_dxgiMeta.width = f->width;
	m_dxgiMeta.height = f->height;

	bool full = f->color_range == AVCOL_RANGE_JPEG;
	bool limited = f->color_range == AVCOL_RANGE_MPEG;
	bool chromaIsLeft = f->chroma_location == AVCHROMA_LOC_LEFT || f->chroma_location == AVCHROMA_LOC_UNSPECIFIED;
	bool chromaIsTopLeft = f->chroma_location == AVCHROMA_LOC_TOPLEFT;
	switch (f->colorspace) {
	case AVCOL_SPC_BT709:
	{
		// SDR 709
		if (f->color_trc == AVCOL_TRC_BT709 || f->color_trc == AVCOL_TRC_IEC61966_2_1 || f->color_trc == AVCOL_TRC_UNSPECIFIED)
		{
			if (!chromaIsLeft) {
				LOG_ERROR("[FFmpeg]: BT.709 NV12 requires LEFT siting in DXGI");
			}
			m_dxgiMeta.indxgiColorSpace = limited ? DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 : DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709;
			m_dxgiMeta.hdr = false;
			return;
		}
		else {
			LOG_ERROR("[FFmpeg]: BT.709 colorspace with unsupported transfer");
			return;
		}
	}
	case AVCOL_SPC_SMPTE170M:
	case AVCOL_SPC_BT470BG:
	{
		// SDR 601
		if (f->color_trc == AVCOL_TRC_BT709 || f->color_trc == AVCOL_TRC_GAMMA22 || f->color_trc == AVCOL_TRC_SMPTE170M || f->color_trc == AVCOL_TRC_UNSPECIFIED)
		{
			if (!chromaIsLeft) {
				LOG_ERROR("[FFmpeg]: BT.601 NV12 requires LEFT siting in DXGI");
				return;
			}
			m_dxgiMeta.indxgiColorSpace = limited ? DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601 : DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601;
			m_dxgiMeta.hdr = false;
			return;
		}
		else {
			LOG_ERROR("[FFmpeg]: BT.601 colorspace with unsupported transfer");
			return;
		}
	}
	case AVCOL_SPC_BT2020_NCL:
	{
		// HDR10 / PQ
		if (f->color_trc == AVCOL_TRC_SMPTE2084)
		{
			if (full) {
				LOG_ERROR("[FFmpeg]: DXGI has no FULL-range PQ YCbCr P2020 mapping here");
				return;
			}
			if (chromaIsTopLeft)
			{
				m_dxgiMeta.indxgiColorSpace = DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020;
			}
			else if (chromaIsLeft) {
				m_dxgiMeta.indxgiColorSpace = DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020;
			}
			else {
				LOG_ERROR("[FFmpeg]: Unsupported chroma siting for PQ BT.2020");
				return;
			}
			m_dxgiMeta.hdr = true;
			return;
		}

		// HLG
		if (f->color_trc == AVCOL_TRC_ARIB_STD_B67)
		{
			// TOPLEFT P2020
			if (!chromaIsTopLeft) {
				LOG_ERROR("[FFmpeg]: DXGI HLG mapping requires TOPLEFT siting");
				return;
			}
			m_dxgiMeta.indxgiColorSpace = limited ? DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020 : DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020;
			m_dxgiMeta.hdr = true;
			return;
		}

		// SDR / WCG BT.2020
		if (f->color_trc == AVCOL_TRC_BT709 || f->color_trc == AVCOL_TRC_BT2020_10 || f->color_trc == AVCOL_TRC_BT2020_12 || f->color_trc == AVCOL_TRC_UNSPECIFIED)
		{
			if (full)
			{
				if (!chromaIsLeft) {
					LOG_ERROR("[FFmpeg]: FULL-range BT.2020 NV12 requires LEFT siting");
					return;
				}
				m_dxgiMeta.indxgiColorSpace = DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020;
			}
			else
			{
				if (chromaIsTopLeft) {
					m_dxgiMeta.indxgiColorSpace = DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020;
				}
				else if (chromaIsLeft) {
					m_dxgiMeta.indxgiColorSpace = DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020;
				}
				else {
					LOG_ERROR("[FFmpeg]: Unsupported chroma siting for SDR BT.2020");
					return;
				}
			}

			m_dxgiMeta.hdr = false;
			return;
		}
		LOG_ERROR("[FFmpeg]: BT.2020 colorspace with unsupported transfer");
		return;
	}
	case AVCOL_SPC_UNSPECIFIED:
	{
		LOG_ERROR("[FFmpeg]: Colorspace is unspecified");
		return;
	}
	default:
	{
		LOG_ERROR("[FFmpeg]: Colorspace not representable by DXGI_COLOR_SPACE_TYPE");
		return;
	}
	}
}

void DX12DeCoder::CreateVideoProcessor()

{
	if (!CheckSupport()) {
		LOG_WARN("[DX12]: Unsupoort video process.");
		//release resource todo

		return;
	}
	auto w = static_cast<uint32_t>(m_dxgiMeta.width);
	auto h = static_cast<uint32_t>(m_dxgiMeta.height);

	D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC inDesc = {};
	inDesc.Format = DXGI_FORMAT_NV12;
	inDesc.ColorSpace = m_dxgiMeta.indxgiColorSpace;
	inDesc.SourceAspectRatio = { w, h };
	inDesc.DestinationAspectRatio = { w,h };
	inDesc.FrameRate = m_dxgiMeta.fps;
	inDesc.SourceSizeRange = { w, h, w, h };
	inDesc.DestinationSizeRange = { w, h, w, h };
	inDesc.EnableOrientation = FALSE;
	inDesc.FilterFlags = D3D12_VIDEO_PROCESS_FILTER_FLAG_NONE;
	inDesc.StereoFormat = D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE;
	inDesc.FieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
	inDesc.DeinterlaceMode = D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE;
	inDesc.EnableAlphaBlending = FALSE;
	inDesc.LumaKey.Enable = FALSE;
	inDesc.NumPastFrames = 0;
	inDesc.NumFutureFrames = 0;
	inDesc.EnableAutoProcessing = FALSE;

	D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC outDesc = {};
	outDesc.Format = m_dxgiMeta.format;
	outDesc.ColorSpace = m_dxgiMeta.outdxgiColorSpace;
	outDesc.AlphaFillMode = D3D12_VIDEO_PROCESS_ALPHA_FILL_MODE_OPAQUE;
	outDesc.AlphaFillModeSourceStreamIndex = 0;
	outDesc.BackgroundColor[0] = 0.0f;
	outDesc.BackgroundColor[1] = 0.0f;
	outDesc.BackgroundColor[2] = 0.0f;
	outDesc.BackgroundColor[3] = 1.0f;
	outDesc.FrameRate = m_dxgiMeta.fps;
	outDesc.EnableStereo = FALSE;


	if (m_pDx12VideoDevice->CreateVideoProcessor(0, &outDesc, 1, &inDesc, IID_PPV_ARGS(&m_pDx12VideoProcessor)) != S_OK) {
		LOG_ERROR("[DX12]: Failed to create video processor.");
		return;
	}

}

void DX12DeCoder::CreateOutputTexture()
{
	D3D12_RESOURCE_DESC texDesc{};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.MipLevels = 1;
	texDesc.DepthOrArraySize = 1;
	texDesc.Width = m_dxgiMeta.width;
	texDesc.Height = m_dxgiMeta.height;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES pro{};
	ZeroMemory(&pro, sizeof(pro));
	pro.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_SHADER_RESOURCE_VIEW_DESC sdesc{};
	ZeroMemory(&sdesc, sizeof(sdesc));
	sdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sdesc.Texture2D.MipLevels = 1;
	sdesc.Texture2D.MostDetailedMip = 0;
	sdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	auto device = DX12::GetDevice();
	std::for_each(m_frameCtxs.begin(), m_frameCtxs.end(), [&](VFrameCtx& ctx) {
		device->CreateCommittedResource(&pro, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&ctx.texture));
		device->CreateShaderResourceView(ctx.texture, &sdesc, ctx.cpuHandle);
		ctx.tvpre = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE);
		ctx.tvpost = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE, D3D12_RESOURCE_STATE_COMMON);
		ctx.tdpre = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		ctx.tdreset = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
		});
}

DX12DeCoder::DX12DeCoder(size_t pqSize, size_t fqSize,const AudioPlayer& audioOutput)
	:VideoDeCoder(pqSize, fqSize,audioOutput)
	,m_frameCtxs(fqSize)
	,m_frameCtxSize(fqSize)
	,m_nowTextureIndex(0)
	,m_lastTextureIndex(-1)
	,m_decodeFinish(false)
	,m_pDx12VideoProcessor(nullptr)
	,m_pDx12VideoDevice(nullptr)

{
	InitDx12Resources();

}

DX12DeCoder::~DX12DeCoder()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc{};
	ZeroMemory(&Desc, sizeof(Desc));
	Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.Texture2D.MipLevels = 1;
	Desc.Texture2D.MostDetailedMip = 0;
	Desc.Texture2D.PlaneSlice = 0;
	Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	auto& renderBackend = ImGuiRenderer::GetRenderBackend();
	auto dx12RenderBackend = static_cast<Dx12RenderBackend*>(&renderBackend);
	std::for_each(m_frameCtxs.begin(), m_frameCtxs.end(), [&](VFrameCtx& ctx) {
		if (ctx.cpuHandle.ptr != 0){
			DX12::GetDevice()->CreateShaderResourceView(nullptr, &Desc, ctx.cpuHandle);
			dx12RenderBackend->FreeDescriptorHandle(ctx.cpuHandle, ctx.gpuHandle);
		}
		if(ctx.cFence)
			ctx.cFence->Release();
		if (ctx.mFence)
			ctx.mFence->Release();
		if (ctx.gFence)
			ctx.gFence->Release();
		if (ctx.dcmdAllocator)
			ctx.dcmdAllocator->Release();
		if (ctx.mcmdAllocator)
			ctx.mcmdAllocator->Release();
		if (ctx.vcmdAllocator)
			ctx.vcmdAllocator->Release();
		if (ctx.dcmdList)
			ctx.dcmdList->Release();
		if (ctx.vcmdList)
			ctx.vcmdList->Release();
		if (ctx.mcmdList)
			ctx.mcmdList->Release();
		if (ctx.dcmdQueue)
			ctx.dcmdQueue->Release();
		if (ctx.vcmdQueue)
			ctx.vcmdQueue->Release();
		if (ctx.texture)
			ctx.texture->Release();
		});
	if (m_pDx12VideoProcessor)
		m_pDx12VideoProcessor->Release();
	if (m_pDx12VideoDevice)
		m_pDx12VideoDevice->Release();

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

	LOG_INFO("[RoomExit] VideoDecoder deconstruction");
}

void DX12DeCoder::InitDx12Resources()
{
	auto& renderBackend = ImGuiRenderer::GetRenderBackend();
	auto dx12RenderBackend = static_cast<Dx12RenderBackend*>(&renderBackend);
	if (DX12::GetDevice()->QueryInterface(IID_PPV_ARGS(&m_pDx12VideoDevice)) != S_OK) {
		LOG_WARN("[DX12DeCoder]:  Unsupport video device");
		return;
	}
	int i = 0;
	std::for_each(m_frameCtxs.begin(), m_frameCtxs.end(), [&](VFrameCtx& ctx) {
		dx12RenderBackend->AllocDescriptorHandle(ctx.cpuHandle, ctx.gpuHandle);
		DX12::InitFence(&ctx.cFence);
		DX12::InitFence(&ctx.mFence);
		DX12::InitFence(&ctx.gFence);
		DX12::InitDirectCmdAlloc(&ctx.dcmdAllocator);
		DX12::InitDirectCmdAlloc(&ctx.mcmdAllocator);
		DX12::InitDirectCmdList(ctx.dcmdAllocator, &ctx.dcmdList);
		DX12::InitDirectCmdList(ctx.mcmdAllocator, &ctx.mcmdList);
		DX12::InitDirectCommandQueue(&ctx.dcmdQueue);
		DX12::InitVideoProcessCmdAlloc(&ctx.vcmdAllocator);
		DX12::InitVideoProcessCmdList(ctx.vcmdAllocator, &ctx.vcmdList);
		DX12::InitVideoProcessCommandQueue(&ctx.vcmdQueue);
		ctx.cFenceValue = 0;
		ctx.gFenceValue = 0;
		ctx.mFenceValue = 0;
		ctx.reset = 0;
		ctx.frame = m_frameQueue[i];
		i++;
		});
}

Texture DX12DeCoder::GetTextureHandle()
{
	if (m_frameCtxs[m_nowTextureIndex].cFence->GetCompletedValue() == 0) {
		LOG_DEBUG("[DX12DeCoder] Return fence value 0, the index is {}", m_nowTextureIndex);
		return Texture{};
	}

	auto& ctx = m_frameCtxs[m_nowTextureIndex];
	if (ctx.cFence->GetCompletedValue() >= ctx.cFenceValue.load(std::memory_order_acquire) && ctx.reset.load(std::memory_order_relaxed) == 1) {
		if (m_lastTextureIndex != -1) {
			m_frameCtxs[m_lastTextureIndex].reset.store(2, std::memory_order_relaxed);
			m_frameCtxs[m_lastTextureIndex].reset.notify_all();
		}
		m_lastTextureIndex = m_nowTextureIndex;
		m_nowTextureIndex = (m_nowTextureIndex + 1) % m_frameCtxSize;
	}
	if (m_frameCtxs[m_lastTextureIndex].frame->pts <= 0) {
		LOG_DEBUG("[DX12DeCoder] Pts less or eq 0, the index is {}", m_lastTextureIndex);
	}
	return Texture{ .textureHandle = m_frameCtxs[m_lastTextureIndex].gpuHandle.ptr,.pts = m_frameCtxs[m_lastTextureIndex].frame->pts * m_timeBase };

}

void DX12DeCoder::StartDecode()
{
	LOG_DEBUG("[VideoDeCoder] Time Base is {}s", m_timeBase);
	m_decodeThread = std::thread([self = shared_from_this()]() {
		AVPacket* packet = nullptr;
		size_t index = 0;

		std::once_flag flag;
		bool pktFlag = false;
		while (self->m_running.load(std::memory_order_relaxed) == true) {
			if (packet == nullptr) {
				if (self->m_packetQueue.Pop(packet) == false) {
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
					continue;
				}
			}

			if (pktFlag == false && (packet->flags & AV_PKT_FLAG_KEY) != AV_PKT_FLAG_KEY) {
				av_packet_free(&packet);
				packet = nullptr;
				continue;
			}
			else if (pktFlag == false && (packet->flags & AV_PKT_FLAG_KEY) == AV_PKT_FLAG_KEY) {
				pktFlag = true;
			}
			int ret = avcodec_send_packet(self->m_pCodecCtx, packet);
			if (ret == AVERROR(EAGAIN)) {

			}
			else {
				av_packet_free(&packet);
				packet = nullptr;
			}
			self->ShouldStop(ret);

			while (avcodec_receive_frame(self->m_pCodecCtx, self->m_frameCtxs[index].frame) ==0 &&self->m_running.load(std::memory_order_relaxed) == true) {
				std::call_once(flag, [&]() {
					self->InitDXGIMeta(self->m_frameCtxs[index].frame);
					self->CreateVideoProcessor();
					self->CreateOutputTexture();
					self->m_decodeClock.store(self->m_frameCtxs[index].frame->pts * self->m_timeBase, std::memory_order_relaxed);
					LOG_DEBUG("[DX12DeCoder]: First pts is : {}s", self->m_frameCtxs[index].frame->pts * self->m_timeBase);
					});
				double audioOuputTime = NAN;
				audioOuputTime = self->m_audioOutput.GetClock();
				if (!std::isnan(audioOuputTime)) {
					if (self->m_frameCtxs[index].frame->best_effort_timestamp * self->m_timeBase < audioOuputTime) {
						continue;
					}
				}

				auto& ctx = self->m_frameCtxs[index];
				auto& syncCtx = ((AVD3D12VAFrame*)ctx.frame->data[0])->sync_ctx;
				ctx.fvpre = CD3DX12_RESOURCE_BARRIER::Transition(((AVD3D12VAFrame*)ctx.frame->data[0])->texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ);
				ctx.fvpost = CD3DX12_RESOURCE_BARRIER::Transition(((AVD3D12VAFrame*)ctx.frame->data[0])->texture, D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ, D3D12_RESOURCE_STATE_COMMON);
				ctx.inArgs.InputStream[0].pTexture2D = ((AVD3D12VAFrame*)ctx.frame->data[0])->texture;
				std::call_once(self->m_frameCtxs[index].init, [&]() {
					ctx.tvpost = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE, D3D12_RESOURCE_STATE_COMMON);
					ctx.tvpre = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE);
					ctx.tdpre = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					ctx.tdreset = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

					ctx.inArgs = D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS{};
					ctx.inArgs.InputStream[0].pTexture2D = ((AVD3D12VAFrame*)ctx.frame->data[0])->texture;
					ctx.inArgs.InputStream[0].Subresource = 0;
					ctx.inArgs.InputStream[1].pTexture2D = nullptr;
					ctx.inArgs.InputStream[1].Subresource = 0;
					ctx.inArgs.Transform.SourceRectangle = {
						0, 0,
						static_cast<LONG>(self->m_dxgiMeta.width),
						static_cast<LONG>(self->m_dxgiMeta.height)
					};
					ctx.inArgs.Transform.DestinationRectangle = {
						0, 0,
						static_cast<LONG>(self->m_dxgiMeta.width),
						static_cast<LONG>(self->m_dxgiMeta.height)
					};
					ctx.inArgs.Transform.Orientation = D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT;

					ctx.inArgs.Flags = D3D12_VIDEO_PROCESS_INPUT_STREAM_FLAG_NONE;
					//ctx.inArgs.Flags = D3D12_VIDEO_PROCESS_INPUT_STREAM_FLAG_FRAME_DISCONTINUITY;
					ctx.inArgs.RateInfo.OutputIndex = 0;
					ctx.inArgs.RateInfo.InputFrameOrField = 0;
					ctx.inArgs.AlphaBlending.Enable = FALSE;
					ctx.inArgs.AlphaBlending.Alpha = 1.0f;

					ctx.outArgs = D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS{};
					ctx.outArgs.OutputStream[0].pTexture2D = ctx.texture;
					ctx.outArgs.OutputStream[0].Subresource = 0;
					ctx.outArgs.OutputStream[1].pTexture2D = nullptr;
					ctx.outArgs.OutputStream[1].Subresource = 0;
					ctx.outArgs.TargetRectangle = {
						0, 0,
						static_cast<LONG>(self->m_dxgiMeta.width),
						static_cast<LONG>(self->m_dxgiMeta.height)
					};
					self->m_frameCtxs[index].mcmdList->Close();
					});
				ctx.vcmdList->ResourceBarrier(1, &ctx.fvpre);
				ctx.vcmdList->ResourceBarrier(1, &ctx.tvpre);
				ctx.vcmdList->ProcessFrames(
					self->m_pDx12VideoProcessor,
					&ctx.outArgs,
					1,
					&ctx.inArgs);
				ctx.vcmdList->ResourceBarrier(1, &ctx.tvpost);
				ctx.vcmdList->ResourceBarrier(1, &ctx.fvpost);
				ctx.vcmdList->Close();
				ctx.vcmdQueue->Wait(syncCtx.fence, syncCtx.fence_value);
				ctx.vcmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&ctx.vcmdList);
				ctx.gFenceValue += 1;
				ctx.vcmdQueue->Signal(ctx.gFence, ctx.gFenceValue);
				ctx.dcmdList->ResourceBarrier(1, &ctx.tdpre);
				ctx.dcmdList->Close();
				ctx.dcmdQueue->Wait(ctx.gFence, ctx.gFenceValue);
				ctx.dcmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&ctx.dcmdList);
				ctx.cFenceValue.fetch_add(1, std::memory_order_relaxed);
				auto cv = ctx.cFenceValue.load(std::memory_order_relaxed);
				ctx.dcmdQueue->Signal(ctx.cFence, cv);
				ctx.reset.store(1, std::memory_order_relaxed);
				index = (index + 1) % self->m_frameQueueSize;
				while (self->m_frameCtxs[index].reset.load(std::memory_order_relaxed) == 1) {
					if (self->m_running.load(std::memory_order_relaxed) == false) {
						break;
					}
					self->m_frameCtxs[index].reset.wait(1, std::memory_order_relaxed);
				}
				if (self->m_frameCtxs[index].reset.load(std::memory_order_relaxed) == 2&& self->m_running.load(std::memory_order_relaxed) == true) {
					av_frame_unref(self->m_frameCtxs[index].frame);
					self->m_frameCtxs[index].dcmdAllocator->Reset();
					self->m_frameCtxs[index].mcmdAllocator->Reset();
					self->m_frameCtxs[index].vcmdAllocator->Reset();
					self->m_frameCtxs[index].dcmdList->Reset(self->m_frameCtxs[index].dcmdAllocator, nullptr);
					self->m_frameCtxs[index].mcmdList->Reset(self->m_frameCtxs[index].mcmdAllocator, nullptr);
					self->m_frameCtxs[index].vcmdList->Reset(self->m_frameCtxs[index].vcmdAllocator);
					self->m_frameCtxs[index].mcmdList->ResourceBarrier(1, &self->m_frameCtxs[index].tdreset);
					self->m_frameCtxs[index].mcmdList->Close();
					self->m_frameCtxs[index].mFenceValue += 1;
					self->m_frameCtxs[index].dcmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&self->m_frameCtxs[index].mcmdList);
					self->m_frameCtxs[index].dcmdQueue->Signal(self->m_frameCtxs[index].mFence, self->m_frameCtxs[index].mFenceValue);
					self->m_frameCtxs[index].vcmdQueue->Wait(self->m_frameCtxs[index].mFence, self->m_frameCtxs[index].mFenceValue);
					self->m_frameCtxs[index].reset.store(0, std::memory_order_relaxed);
				}

			}
		}
		HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
		for (auto& ctx : self->m_frameCtxs) {
			if (ctx.gFence->GetCompletedValue() < ctx.gFenceValue) {
				ctx.gFence->SetEventOnCompletion(ctx.gFenceValue, event);
				WaitForSingleObject(event, INFINITE);
			}
		}
		CloseHandle(event);

		if (packet != nullptr) {
			av_packet_free(&packet);
		}
		AVFrame* frame = av_frame_alloc();

		avcodec_send_packet(self->m_pCodecCtx, nullptr);
		while (avcodec_receive_frame(self->m_pCodecCtx, frame) >= 0) {
			av_frame_unref(frame);
			LOG_INFO("[RoomExit] Handle rest frame......");
		}
		av_frame_free(&frame);

		avcodec_flush_buffers(self->m_pCodecCtx);
		self->m_decodeFinish.store(true, std::memory_order_relaxed);
		self->m_decodeFinish.notify_all();
		LOG_INFO("[RoomExit] Video decoder thread exit");
	});
	m_decodeThread.detach();
}

void DX12DeCoder::Stop()
{
	m_running.store(false, std::memory_order_relaxed); 
	for (auto& ctx : m_frameCtxs) {
		ctx.reset.store(2, std::memory_order_relaxed);
		ctx.reset.notify_all();
	}
	while (m_decodeFinish.load(std::memory_order_relaxed) == false) {
		m_decodeFinish.wait(false, std::memory_order_relaxed);
	}

}

void DX12PixelShaderDeCoder::CreateTexture(AVFrame* f)
{
	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	resourceDesc.Height = f->height;
	resourceDesc.Width = f->width;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;

	D3D12_HEAP_PROPERTIES pro;
	ZeroMemory(&pro, sizeof(pro));
	pro.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = resourceDesc.Format;
	optimizedClearValue.Color[0] = 0.0f;
	optimizedClearValue.Color[1] = 0.0f;
	optimizedClearValue.Color[2] = 0.0f;
	optimizedClearValue.Color[3] = 1.0f;

	auto rtvDescriptorSize = DX12::GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto srvDescriptorSize = DX12::GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	auto rtvC = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvC = m_pSrvHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvG = m_pSrvHeap->GetGPUDescriptorHandleForHeapStart();

	int i = 0;
	auto& renderBackend = ImGuiRenderer::GetRenderBackend();
	auto dx12RenderBackend = static_cast<Dx12RenderBackend*>(&renderBackend);



	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = static_cast<float>(f->width);
	m_viewport.Height = static_cast<float>(f->height);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissorRect = { 0, 0, (long)f->width, (long)f->height };



	for (auto& ctx : m_frameCtxs) {
		dx12RenderBackend->AllocDescriptorHandle(ctx.imguiCpuHandle, ctx.imguiGpuHandle);
		ctx.rtCpuHandle.ptr = rtvC.ptr + i * rtvDescriptorSize;
		ctx.yCpuHandle.ptr = srvC.ptr + i * 2 * srvDescriptorSize;
		ctx.yGpuHandle.ptr = srvG.ptr + i * 2 * srvDescriptorSize;
		ctx.uvCpuHandle.ptr = ctx.yCpuHandle.ptr + srvDescriptorSize;
		ctx.uvGpuHandle.ptr = ctx.yGpuHandle.ptr + srvDescriptorSize;
		DX12::GetDevice()->CreateCommittedResource(&pro, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &optimizedClearValue, IID_PPV_ARGS(&ctx.texture));
		DX12::GetDevice()->CreateRenderTargetView(ctx.texture, nullptr, ctx.rtCpuHandle);
		ctx.preBarrier = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		ctx.postBarrier = CD3DX12_RESOURCE_BARRIER::Transition(ctx.texture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		ctx.cmdList->SetPipelineState(s_pPso.Get());
		ctx.cmdList->SetGraphicsRootSignature(s_pRootSignature.Get());
		i++;
	}
}

void DX12PixelShaderDeCoder::CreateSignature()
{
	auto pro = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const UINT VerticesSize = _countof(s_vertices) * sizeof(Vertex);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VerticesSize);
	DX12::GetDevice()->CreateCommittedResource(&pro, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&s_vertexBuffer));

	auto uploadPro = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	DX12::GetDevice()->CreateCommittedResource(&uploadPro, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&s_vertexUploadBuffer));

	Microsoft::WRL::ComPtr<ID3DBlob> errBlob, vertexBlob, pixelBolb, serializedRootSig;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	D3D12_ROOT_PARAMETER rootParameters[2];
	D3D12_DESCRIPTOR_RANGE range;

	range.BaseShaderRegister = 0;
	range.NumDescriptors = 2;
	range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	range.RegisterSpace = 0;
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &range;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[1].Constants.Num32BitValues = 1;
	rootParameters[1].Constants.RegisterSpace = 0;
	rootParameters[1].Constants.ShaderRegister = 0;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	auto sampler = CD3DX12_STATIC_SAMPLER_DESC(0);
	rootSigDesc.NumParameters = 2;
	rootSigDesc.pParameters = rootParameters;
	rootSigDesc.pStaticSamplers = &sampler;
	rootSigDesc.NumStaticSamplers = 1;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	auto vertextPath = GetExePath();
	vertextPath.concat("\\Resources\\Shaders\\VertexShader.hlsl");
	auto vertexShader = vertextPath.generic_wstring();
	auto hr = D3DCompileFromFile(vertexShader.c_str(), nullptr, nullptr, "main", "vs_5_1", 0, 0, &vertexBlob, &errBlob);
	if (FAILED(hr)) {
		LOG_ERROR("[DX12PixelShaderDeCoder] Failed to compile vertex shader : {}", static_cast<char*>(errBlob->GetBufferPointer()));
		exit(1);
	}
	errBlob.Reset();

	auto pixelPath = GetExePath();
	pixelPath.concat("\\Resources\\Shaders\\PixelShader.hlsl");
	auto pixelShader = pixelPath.generic_wstring();
	hr = D3DCompileFromFile(pixelShader.c_str(), nullptr, nullptr, "main", "ps_5_1", 0, 0, &pixelBolb, &errBlob);
	if (FAILED(hr)) {
		LOG_ERROR("[DX12PixelShaderDeCoder] Failed to compile pixel shader : {}", static_cast<char*>(errBlob->GetBufferPointer()));
		exit(1);
	}
	errBlob.Reset();
	hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errBlob);
	if (FAILED(hr)) {
		if (errBlob) {
			LOG_ERROR("[DX12PixelShaderDeCoder] Root signature error: {}",
				static_cast<const char*>(errBlob->GetBufferPointer()));
		}
		else {
			LOG_ERROR("[DX12PixelShaderDeCoder] Failed to serialize root signature. HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
		}
		exit(1);
	}
	errBlob.Reset();
	DX12::GetDevice()->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&s_pRootSignature));

	psoDesc.pRootSignature = s_pRootSignature.Get();
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	psoDesc.VS = { vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize() };
	psoDesc.PS = { pixelBolb->GetBufferPointer(), pixelBolb->GetBufferSize() };
	psoDesc.NodeMask = 0;
	psoDesc.InputLayout = s_inputLayout;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	DX12::GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&s_pPso));
}

void DX12PixelShaderDeCoder::InitDX12Resources()
{
	if (s_pPso == nullptr || s_pRootSignature == nullptr) {
		CreateSignature();
	}

	int i = 0;
	for (auto& ctx : m_frameCtxs) {
		DX12::InitDirectCmdAlloc(&ctx.cmdAllocator);
		DX12::InitDirectCmdList(ctx.cmdAllocator, &ctx.cmdList);
		DX12::InitFence(&ctx.fence);
		ctx.frame = m_frameQueue[i++];
		ctx.fenValue = 0;
	}
	D3D12_DESCRIPTOR_HEAP_DESC srvDesc;
	srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvDesc.NodeMask = 0;
	srvDesc.NumDescriptors = static_cast<UINT>(2 * m_frameQueueSize);
	srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc;
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDesc.NodeMask = 0;
	rtvDesc.NumDescriptors = static_cast<UINT>(m_frameQueueSize);
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	DX12::InitDescHeap(&m_pSrvHeap, srvDesc);
	DX12::InitDescHeap(&m_pRtvHeap, rtvDesc);
	DX12::InitDirectCommandQueue(&m_pCmdQueue);

}

DX12PixelShaderDeCoder::DX12PixelShaderDeCoder(size_t pqSize, size_t fqSize,const class AudioPlayer& audioOutput)
	:VideoDeCoder(pqSize, fqSize, audioOutput)
	,m_matrixIndex(0)
	,m_frameCtxs(fqSize)
	,m_nowTextureIndex(0)
	,m_lastTextureIndex(-1)
	,m_decodeFinish(false)
{
	InitDX12Resources();
}

DX12PixelShaderDeCoder::~DX12PixelShaderDeCoder()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc{};
	ZeroMemory(&Desc, sizeof(Desc));
	Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.Texture2D.MipLevels = 1;
	Desc.Texture2D.MostDetailedMip = 0;
	Desc.Texture2D.PlaneSlice = 0;
	Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	auto& renderBackend = ImGuiRenderer::GetRenderBackend();

	auto dx12RenderBackend = static_cast<Dx12RenderBackend*>(&renderBackend);
	if (m_pRtvHeap != nullptr) {
		m_pRtvHeap->Release();
	}
	if (m_pSrvHeap != nullptr) {
		m_pSrvHeap->Release();
	}
	if (m_pCmdQueue != nullptr) {
		m_pCmdQueue->Release();
	}
	for (auto& ctx : m_frameCtxs) {
		if (ctx.imguiCpuHandle.ptr != 0) {
			//This null binding is crucial, as even if you have already released the descriptor, 
			// ImGUI may continue to use it; otherwise, it may compromise the driver state
			DX12::GetDevice()->CreateShaderResourceView(nullptr, &Desc, ctx.imguiCpuHandle);
			dx12RenderBackend->FreeDescriptorHandle(ctx.imguiCpuHandle, ctx.imguiGpuHandle);
		}
		if (ctx.fence != nullptr) {
			ctx.fence->Release();
		}
		if (ctx.cmdAllocator != nullptr) {
			ctx.cmdAllocator->Release();
		}
		if (ctx.cmdList != nullptr) {
			ctx.cmdList->Release();
		}
		if (ctx.texture != nullptr) {
			ctx.texture->Release();
		}
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
	LOG_INFO("[RoomExit] VideoDecoder deconstruction");

}

Texture DX12PixelShaderDeCoder::GetTextureHandle()
{
	auto r = m_frameCtxs[m_nowTextureIndex].ready.load(std::memory_order_relaxed);
	auto v = m_frameCtxs[m_nowTextureIndex].fenValue.load(std::memory_order_relaxed);
	if (v == 0) {
		return Texture{};
	}
	if (m_frameCtxs[m_nowTextureIndex].fence->GetCompletedValue() < v || r == false) {
		if (m_lastTextureIndex != -1) {
			return Texture{ .textureHandle = m_frameCtxs[m_lastTextureIndex].imguiGpuHandle.ptr, .pts = m_frameCtxs[m_lastTextureIndex].frame->pts * m_timeBase };
		}
		return Texture{};
	}
	else if (m_frameCtxs[m_nowTextureIndex].fence->GetCompletedValue() >= v && r == true) {
		if (m_lastTextureIndex != -1) {
			m_frameCtxs[m_lastTextureIndex].ready.store(false, std::memory_order_relaxed);
			m_frameCtxs[m_lastTextureIndex].ready.notify_all();
		}
		m_lastTextureIndex = m_nowTextureIndex;
		m_nowTextureIndex = (m_nowTextureIndex + 1) % m_frameQueueSize;
		std::call_once(m_frameCtxs[m_lastTextureIndex].init, [&]() {
			D3D12_SHADER_RESOURCE_VIEW_DESC Desc{};
			ZeroMemory(&Desc, sizeof(Desc));
			Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			Desc.Texture2D.MipLevels = 1;
			Desc.Texture2D.MostDetailedMip = 0;
			Desc.Texture2D.PlaneSlice = 0;
			Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			DX12::GetDevice()->CreateShaderResourceView(m_frameCtxs[m_lastTextureIndex].texture, &Desc, m_frameCtxs[m_lastTextureIndex].imguiCpuHandle);
			});
		return Texture{ .textureHandle = m_frameCtxs[m_lastTextureIndex].imguiGpuHandle.ptr, .pts = m_frameCtxs[m_lastTextureIndex].frame->pts * m_timeBase };

	}

	if (m_lastTextureIndex == -1) {
		return Texture{};
	}
	return Texture{};
}

void DX12PixelShaderDeCoder::StartDecode()
{
	m_decodeThread = std::thread([self = shared_from_this()]() {
		AVPacket* packet = nullptr;
		size_t index = 0;

		std::once_flag flag;
		bool pktFlag = false;
		auto timeBase = av_q2d(self->m_pCodecCtx->time_base);

		D3D12_SHADER_RESOURCE_VIEW_DESC yDesc{};
		ZeroMemory(&yDesc, sizeof(yDesc));
		yDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		yDesc.Format = DXGI_FORMAT_R8_UNORM;
		yDesc.Texture2D.MipLevels = 1;
		yDesc.Texture2D.MostDetailedMip = 0;
		yDesc.Texture2D.PlaneSlice = 0;
		yDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		D3D12_SHADER_RESOURCE_VIEW_DESC uvDesc{};
		ZeroMemory(&uvDesc, sizeof(uvDesc));
		uvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		uvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
		uvDesc.Texture2D.MipLevels = 1;
		uvDesc.Texture2D.MostDetailedMip = 0;
		uvDesc.Texture2D.PlaneSlice = 1;
		uvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = s_vertices;
		subResourceData.RowPitch = _countof(s_vertices) * sizeof(Vertex);
		subResourceData.SlicePitch = subResourceData.RowPitch;

		CD3DX12_RESOURCE_BARRIER vertexBarrierPre;
		CD3DX12_RESOURCE_BARRIER vertexBarrierPost;
		if (s_vertexBufferView.BufferLocation == 0) {
			vertexBarrierPre = CD3DX12_RESOURCE_BARRIER::Transition(s_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			vertexBarrierPost = CD3DX12_RESOURCE_BARRIER::Transition(s_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}

		while (self->m_running.load(std::memory_order_relaxed) == true) {
			if (packet == nullptr) {
				if (self->m_packetQueue.Pop(packet) == false) {
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
					continue;
				}
			}
			if (pktFlag==false && (packet->flags& AV_PKT_FLAG_KEY) != AV_PKT_FLAG_KEY) {
				av_packet_free(&packet);
				packet = nullptr;
				continue;
			}
			else if (pktFlag == false && (packet->flags & AV_PKT_FLAG_KEY) == AV_PKT_FLAG_KEY) {
				pktFlag = true;
			}
			int ret = avcodec_send_packet(self->m_pCodecCtx, packet);
			if (ret < 0) {
				char str[128];
				auto s = av_make_error_string(str, 128, ret);
				LOG_ERROR("[VidoeDeCoder] Failed to send packet :{}", s);
			}
			if (ret == AVERROR(EAGAIN)) {

			}
			else {
				av_packet_free(&packet);
				packet = nullptr;
			}
			self->ShouldStop(ret);
			while (avcodec_receive_frame(self->m_pCodecCtx, self->m_frameCtxs[index].frame) == 0
				&& self->m_running.load(std::memory_order_relaxed) == true) {

				auto& ctx = self->m_frameCtxs[index];
				std::call_once(flag, [&]() {
					if (s_vertexBufferView.BufferLocation == 0) {
						ctx.cmdList->ResourceBarrier(1, &vertexBarrierPre);
						UpdateSubresources<1>(ctx.cmdList, s_vertexBuffer.Get(), s_vertexUploadBuffer.Get(), 0, 0, 1, &subResourceData);
						ctx.cmdList->ResourceBarrier(1, &vertexBarrierPost);
						s_vertexBufferView.BufferLocation = s_vertexBuffer->GetGPUVirtualAddress();
						s_vertexBufferView.SizeInBytes = _countof(s_vertices) * sizeof(Vertex);
						s_vertexBufferView.StrideInBytes = sizeof(Vertex);
					}
					self->CreateTexture(ctx.frame);
					self->m_decodeClock.store(self->m_frameCtxs[index].frame->pts * self->m_timeBase, std::memory_order_relaxed);
					});
				
				auto& syncCtx = ((AVD3D12VAFrame*)ctx.frame->data[0])->sync_ctx;
				if (syncCtx.fence->GetCompletedValue() < syncCtx.fence_value) {
					syncCtx.fence->SetEventOnCompletion(syncCtx.fence_value,syncCtx.event);
					WaitForSingleObject(syncCtx.event, INFINITE);
				}

				DX12::GetDevice()->CreateShaderResourceView(((AVD3D12VAFrame*)ctx.frame->data[0])->texture, &yDesc, ctx.yCpuHandle);
				DX12::GetDevice()->CreateShaderResourceView(((AVD3D12VAFrame*)ctx.frame->data[0])->texture, &uvDesc, ctx.uvCpuHandle);
				
				ctx.cmdList->RSSetViewports(1, &self->m_viewport);
				ctx.cmdList->RSSetScissorRects(1, &self->m_scissorRect);
				ctx.cmdList->OMSetRenderTargets(1, &ctx.rtCpuHandle, FALSE, nullptr);
				ctx.cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);
				ctx.cmdList->IASetVertexBuffers(0, 1, &s_vertexBufferView);
				ctx.cmdList->SetDescriptorHeaps(1, &self->m_pSrvHeap);
				ctx.cmdList->SetGraphicsRootDescriptorTable(0, ctx.yGpuHandle);
				ctx.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				ctx.cmdList->DrawInstanced(4, 1, 0, 0);
				ctx.cmdList->ResourceBarrier(1, &ctx.postBarrier);
				ctx.cmdList->Close();

	
				self->m_pCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&ctx.cmdList);
				ctx.fenValue.fetch_add(1, std::memory_order_relaxed);

				self->m_pCmdQueue->Signal(ctx.fence, ctx.fenValue.load(std::memory_order_relaxed));
				ctx.ready.store(true, std::memory_order_relaxed);
				index = (index + 1) % self->m_frameQueueSize;
				while (self->m_frameCtxs[index].ready.load(std::memory_order_relaxed) == true) {
					self->m_frameCtxs[index].ready.wait(true, std::memory_order_relaxed);
				}
				if (self->m_frameCtxs[index].ready.load(std::memory_order_relaxed) == false && self->m_frameCtxs[index].fenValue.load(std::memory_order_relaxed) != 0
					&&self->m_running.load(std::memory_order_relaxed) == true) {
					if (self->m_frameCtxs[index].fence->GetCompletedValue() < self->m_frameCtxs[index].fenValue.load(std::memory_order_relaxed)) {
						HANDLE e = CreateEvent(nullptr, false, false, nullptr);
						self->m_frameCtxs[index].fence->SetEventOnCompletion(self->m_frameCtxs[index].fenValue.load(std::memory_order_relaxed), e);
						WaitForSingleObject(e, INFINITE);
						CloseHandle(e);
					}
					av_frame_unref(self->m_frameCtxs[index].frame);
					self->m_frameCtxs[index].cmdAllocator->Reset();
					self->m_frameCtxs[index].cmdList->Reset(self->m_frameCtxs[index].cmdAllocator, self->s_pPso.Get());
					self->m_frameCtxs[index].cmdList->SetGraphicsRootSignature(self->s_pRootSignature.Get());
					self->m_frameCtxs[index].cmdList->ResourceBarrier(1, &self->m_frameCtxs[index].preBarrier);
				}
			}
		}
		for (auto& ctx : self->m_frameCtxs) {
			if (ctx.fence->GetCompletedValue() < ctx.fenValue.load(std::memory_order_relaxed)) {
				HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
				ctx.fence->SetEventOnCompletion(ctx.fenValue.load(std::memory_order_relaxed), event);
				WaitForSingleObject(event, INFINITE);
				CloseHandle(event);
			}
		}
		if (packet != nullptr) {
			av_packet_free(&packet);
		}

		AVFrame* frame = av_frame_alloc();
		while (avcodec_receive_frame(self->m_pCodecCtx, frame) >= 0) {
			av_frame_unref(frame);
			LOG_INFO("[RoomExit] Handle rest frame......");
		}

		avcodec_send_packet(self->m_pCodecCtx, nullptr);
		while (avcodec_receive_frame(self->m_pCodecCtx, frame) >= 0) {
			av_frame_unref(frame);
			LOG_INFO("[RoomExit] Handle rest frame......");
		}
		av_frame_free(&frame);
		avcodec_flush_buffers(self->m_pCodecCtx);
		self->m_decodeFinish.store(true, std::memory_order_relaxed);
		self->m_decodeFinish.notify_all();
		LOG_INFO("[RoomExit] Video decoder thread exit");
	});
	m_decodeThread.detach();
}

void DX12PixelShaderDeCoder::Stop()
{
	m_running.store(false, std::memory_order_relaxed);
	for (auto& ctx : m_frameCtxs) {
		ctx.ready.store(false, std::memory_order_relaxed);
		ctx.ready.notify_all();
	}
	while (m_decodeFinish.load(std::memory_order_relaxed) == false) {
		m_decodeFinish.wait(false, std::memory_order_relaxed);
	}
	if (m_decodeThread.joinable()) {
		LOG_DEBUG("[DX12PixelShaderDeCoder] Thread still joinable,which is never should be hanppend");
		m_decodeThread.join();
	}

}


