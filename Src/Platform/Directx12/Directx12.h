#pragma once
//---------directx12----------
#include <direct.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <d3d12.h>
#include <dxgi1_5.h>
#include <d3d12video.h>
//---------directx12----------

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"runtimeobject.lib")


class DX12 {
private:
	static inline Microsoft::WRL::ComPtr<ID3D12Device> s_pDevice = nullptr;;
private:
public:
	static void DumpDredAfterRemoval(ID3D12Device* device);
	static void CreateDevice();
	static Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() { return s_pDevice; }
	static void InitDirectCmdAlloc(ID3D12CommandAllocator** cmdAlloc);
	static void InitDirectCmdList(ID3D12CommandAllocator* cmdAlloc, ID3D12GraphicsCommandList** cmdlist);
	static void InitDirectCommandQueue(ID3D12CommandQueue** cmdQueue);
	static void InitDescHeap(ID3D12DescriptorHeap** Heap, D3D12_DESCRIPTOR_HEAP_DESC& desc);
	static void InitFence(ID3D12Fence** fence);
	static void InitVideoProcessCmdAlloc(ID3D12CommandAllocator** cmdAlloc);
	static void InitVideoProcessCmdList(ID3D12CommandAllocator* cmdAlloc, ID3D12VideoProcessCommandList** cmdlist);
	static void InitVideoProcessCommandQueue(ID3D12CommandQueue** cmdQueue);
	static void InitVideoDevice(ID3D12VideoDevice** device);
	static void Destroy();
};