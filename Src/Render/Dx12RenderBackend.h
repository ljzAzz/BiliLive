#pragma
#include "Platform/Directx12/Directx12.h"

//---------ImGui------------
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>
//---------ImGui------------

#include <tchar.h>

#include "RenderBackend.h"
#include "Event/Event.h"


//ImGui Dx12 example

class Dx12RenderBackend :public RenderBackend {
private:
	static constexpr int RENDER_NUM_FRAMES_IN_FLIGHT = 2;
	static constexpr int RENDER_NUM_BACK_BUFFERS = 2;
	static constexpr int RENDER_SRV_HEAP_SIZE = 2056;

	struct ExampleDescriptorHeapAllocator
	{
		std::mutex Mutex;
		ID3D12DescriptorHeap* Heap = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
		D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
		UINT                        HeapHandleIncrement;
		ImVector<int>               FreeIndices;

		void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap);
		void Destroy();
		void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle);
		void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle);
	};

	struct FrameContext
	{
		ID3D12CommandAllocator* CommandAllocator;
		UINT64                      FenceValue;
	};

	FrameContext                 m_frameContext[RENDER_NUM_FRAMES_IN_FLIGHT] = {};
	UINT                         m_frameIndex = 0;

	ImGui_ImplDX12_InitInfo m_initInfo;

	Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
	ID3D12DescriptorHeap* m_pd3dRtvDescHeap = nullptr;
	ID3D12DescriptorHeap* m_pd3dSrvDescHeap = nullptr;
	ExampleDescriptorHeapAllocator m_pd3dSrvDescHeapAlloc;
	ID3D12CommandQueue* m_pd3dCommandQueue = nullptr;
	ID3D12GraphicsCommandList* m_pd3dCommandList = nullptr;
	ID3D12Fence* m_fence = nullptr;
	HANDLE                       m_fenceEvent = nullptr;
	UINT64                       m_fenceLastSignaledValue = 0;
	IDXGISwapChain3* m_pSwapChain = nullptr;
	bool                         m_swapChainTearingSupport = false;
	bool                         m_swapChainOccluded = false;
	HANDLE                       m_hSwapChainWaitableObject = nullptr;
	ID3D12Resource* m_mainRenderTargetResource[RENDER_NUM_BACK_BUFFERS] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE  m_mainRenderTargetDescriptor[RENDER_NUM_BACK_BUFFERS] = {};

	HWND m_windowHandle;
	ImVec4 m_clearColor;
private:
	FrameContext* WaitForNextFrameContext();
	void WaitForPendingOperations();
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();
	bool ReSize(Event* e);
public:
	Dx12RenderBackend(void* initData);
	~Dx12RenderBackend();
	Dx12RenderBackend(const Dx12RenderBackend&) = delete;
	Dx12RenderBackend& operator=(const Dx12RenderBackend&) = delete;
	virtual void BeginRender() override;
	virtual void EndRender() override;
	virtual void SwapBuffers(bool vsync) override;
	virtual void ShutDown() override;
	virtual bool IsOccluded() const override  {
		return (m_swapChainOccluded && m_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) || ::IsIconic(m_windowHandle);
	};
	void AllocDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu);
	void FreeDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu);

};