#pragma once
#include "Platform/Directx12/Directx12.h"
#include "DeCoder.h"
#include <d3d12video.h>
#include "UI/ImGuiRenderer.h"


struct Texture {
	uint64_t textureHandle = 0;;
	double pts = 0;
};

class VideoDeCoder :public DeCoder {
protected:
	const class AudioPlayer& m_audioOutput;
public:
	VideoDeCoder(size_t pqSize, size_t fqSize,const class AudioPlayer& audioOutput) :DeCoder(pqSize, fqSize),m_audioOutput(audioOutput) {}
	virtual ~VideoDeCoder() {}
	virtual void StartDecode() override {};
	virtual Texture GetTextureHandle() = 0;
	virtual double GetFps() const = 0;
	virtual void SetFps(AVRational fps) = 0;
};


class DX12DeCoder :public std::enable_shared_from_this<DX12DeCoder>, public VideoDeCoder {
private:
	//-----------------DX12------------------
	struct VFrameCtx{
		AVFrame* frame;
		ID3D12Resource* texture;
		ID3D12CommandAllocator* vcmdAllocator;
		ID3D12CommandAllocator* mcmdAllocator;
		ID3D12CommandAllocator* dcmdAllocator;
		ID3D12VideoProcessCommandList* vcmdList;
		ID3D12GraphicsCommandList* mcmdList;
		ID3D12GraphicsCommandList* dcmdList;
		ID3D12CommandQueue* vcmdQueue;
		ID3D12CommandQueue* dcmdQueue;
		ID3D12Fence* gFence;
		ID3D12Fence* mFence;
		ID3D12Fence* cFence;
		uint64_t gFenceValue;
		uint64_t mFenceValue;
		double pts;
		std::atomic_uint64_t cFenceValue;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
		//barriers
		CD3DX12_RESOURCE_BARRIER fvpre;
		CD3DX12_RESOURCE_BARRIER fvpost;
		CD3DX12_RESOURCE_BARRIER tvpre;
		CD3DX12_RESOURCE_BARRIER tvpost;
		CD3DX12_RESOURCE_BARRIER tdpre;
		CD3DX12_RESOURCE_BARRIER tdreset;

		D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS inArgs;
		D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS outArgs;
		std::once_flag init;
		std::atomic_int reset = 0;
	};

	struct DXGIMeta {
		int width;
		int height;
		DXGI_RATIONAL fps;
		DXGI_COLOR_SPACE_TYPE indxgiColorSpace;
		static constexpr const DXGI_COLOR_SPACE_TYPE outdxgiColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		static constexpr const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		bool hdr;

	} m_dxgiMeta;

	std::vector<VFrameCtx> m_frameCtxs;
	ID3D12VideoDevice* m_pDx12VideoDevice;
	ID3D12VideoProcessor* m_pDx12VideoProcessor;

	std::thread m_decodeThread;
	std::atomic_bool m_decodeFinish;

	size_t m_nowTextureIndex;
	size_t m_lastTextureIndex;
	size_t m_frameCtxSize;
	std::once_flag flag;
private:
	bool CheckSupport();
	void InitDXGIMeta(AVFrame* f);
	void CreateVideoProcessor();
	void CreateOutputTexture();
	void InitDx12Resources();
public:
	DX12DeCoder(size_t pqSize, size_t fqSize, const class AudioPlayer& audioOutput);
	~DX12DeCoder();
	virtual Texture GetTextureHandle() override;
	virtual void StartDecode() override;
	virtual void Stop() override;

	virtual double GetFps() const override { return static_cast<double>(m_dxgiMeta.fps.Numerator)/ static_cast<double>(m_dxgiMeta.fps.Denominator); }
	virtual void SetFps(AVRational fps) override{ m_dxgiMeta.fps = { static_cast<uint32_t>(fps.num), static_cast<uint32_t>(fps.den) }; }
};


#define COLOR_MATRIX_BT601_LIMITED  0
#define COLOR_MATRIX_BT601_FULL     1
#define COLOR_MATRIX_BT709_LIMITED  2
#define COLOR_MATRIX_BT709_FULL     3
#define COLOR_MATRIX_BT2020_LIMITED 4
#define COLOR_MATRIX_COUNT          5

class DX12PixelShaderDeCoder: public std::enable_shared_from_this<DX12PixelShaderDeCoder>, public VideoDeCoder {
private:
	struct Vertex {
		float pos[2];
		float uv[2];
	};

	static inline constexpr Vertex s_vertices[] = {
		{ {-1.0f,  1.0f}, {0.0f, 0.0f} },
		{ { 1.0f,  1.0f}, {1.0f, 0.0f} },
		{ {-1.0f, -1.0f}, {0.0f, 1.0f} }, 
		{ { 1.0f, -1.0f}, {1.0f, 1.0f} } 
	};

	static inline const constexpr D3D12_INPUT_ELEMENT_DESC s_inputDesc[2] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	
	static inline const constexpr D3D12_INPUT_LAYOUT_DESC s_inputLayout = {
		.pInputElementDescs = s_inputDesc,
		.NumElements =_countof(s_inputDesc)
	};
	
	static inline D3D12_VERTEX_BUFFER_VIEW s_vertexBufferView{ .BufferLocation = 0,.SizeInBytes = 0,.StrideInBytes = 0 };
	static inline Microsoft::WRL::ComPtr<ID3D12Resource> s_vertexBuffer = nullptr;
	static inline Microsoft::WRL::ComPtr<ID3D12Resource> s_vertexUploadBuffer = nullptr;

	static inline Microsoft::WRL::ComPtr<ID3D12RootSignature> s_pRootSignature = nullptr;
	static inline Microsoft::WRL::ComPtr<ID3D12PipelineState> s_pPso = nullptr;

private:
	struct VFrameCtx {
		AVFrame* frame;
		ID3D12Resource* texture;
		D3D12_CPU_DESCRIPTOR_HANDLE imguiCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE imguiGpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE rtCpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE yCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE yGpuHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE uvCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE uvGpuHandle;
		ID3D12CommandAllocator* cmdAllocator;
		ID3D12GraphicsCommandList* cmdList;
		CD3DX12_RESOURCE_BARRIER preBarrier;
		CD3DX12_RESOURCE_BARRIER postBarrier;
		ID3D12Fence* fence;
		std::atomic_uint64_t fenValue;
		std::atomic_bool ready = false;
		std::once_flag init;
	};
	std::vector<VFrameCtx> m_frameCtxs;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
	ID3D12CommandQueue* m_pCmdQueue;
	ID3D12DescriptorHeap* m_pSrvHeap;
	ID3D12DescriptorHeap* m_pRtvHeap;

	uint32_t m_matrixIndex;
	size_t m_nowTextureIndex;
	uint64_t m_lastTextureIndex;

	std::thread m_decodeThread;
	std::atomic_bool m_decodeFinish;


	double m_fps = 0;
	std::once_flag flag;
private:
	void CreateTexture(AVFrame* f);
	void CreateSignature();
	void InitDX12Resources();
public:
	DX12PixelShaderDeCoder(size_t pqSize, size_t fqSize, const class AudioPlayer& audioOutput);
	~DX12PixelShaderDeCoder();
	virtual Texture GetTextureHandle() override;
	virtual void StartDecode() override;
	virtual void Stop() override;
	virtual double GetFps() const override { return m_fps; }
	virtual void SetFps(AVRational fps) override { m_fps = static_cast<double>(fps.num)/static_cast<double>(fps.den); }
};