#include "stdafx.h"
#include "Directx12.h"
#include <roapi.h>

void DX12::DumpDredAfterRemoval(ID3D12Device* device)
{
    const HRESULT reason = device->GetDeviceRemovedReason();
    LOG_DEBUG("[DX12] GetDeviceRemovedReason = {}", static_cast<unsigned>(reason));
    
    // Breadcrumbs + context
    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData1> pDred1;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&pDred1))))
    {
        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 ab = {};
        if (SUCCEEDED(pDred1->GetAutoBreadcrumbsOutput1(&ab)))
        {
            for (auto n = ab.pHeadAutoBreadcrumbNode; n; n = n->pNext)
            {
                const UINT last = n->pLastBreadcrumbValue ? *n->pLastBreadcrumbValue : 0;
                LOG_DEBUG(L"[DX12][DRED] CL={}  Queue={}  Last={}  Count={}",
                    n->pCommandListDebugNameW ? n->pCommandListDebugNameW : L"<unnamed>",
                    n->pCommandQueueDebugNameW ? n->pCommandQueueDebugNameW : L"<unnamed>",
                    last, n->BreadcrumbCount);
    
                for (UINT i = 0; i < n->BreadcrumbContextsCount; ++i)
                {
                    const auto& c = n->pBreadcrumbContexts[i];
                    if (c.BreadcrumbIndex + 8 >= last && c.BreadcrumbIndex <= last + 1)
                    {
                        LOG_DEBUG(L"    ctx[{}] = {}",
                            c.BreadcrumbIndex,
                            c.pContextString ? c.pContextString : L"<null>");
                    }
                }
            }
        }
    }
    
    // Page fault
    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> pDred0;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&pDred0))))
    {
        D3D12_DRED_PAGE_FAULT_OUTPUT pf = {};
        if (SUCCEEDED(pDred0->GetPageFaultAllocationOutput(&pf)))
        {
            LOG_DEBUG(L"[DX12][DRED] PageFaultVA = {}", pf.PageFaultVA);
    
            for (auto a = pf.pHeadExistingAllocationNode; a; a = a->pNext)
                LOG_DEBUG(L"    existing = {}", a->ObjectNameW ? a->ObjectNameW : L"<unnamed>");
    
            for (auto a = pf.pHeadRecentFreedAllocationNode; a; a = a->pNext)
                LOG_DEBUG(L"    freed    = {}", a->ObjectNameW ? a->ObjectNameW : L"<unnamed>");
        }
    }

    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings)))) {
        dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
}

void DX12::CreateDevice()
{
    if (auto ret = Windows::Foundation::Initialize(RO_INIT_MULTITHREADED); ret != S_OK) {
        LOG_WARN("[DX12] Failed to initialize by RO_INIT_MULTITHREADED");
    }
    // [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
    Microsoft::WRL::ComPtr<ID3D12Debug1> pDebug1 = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug1))))
    {
        pDebug1->EnableDebugLayer();
        pDebug1->SetEnableGPUBasedValidation(TRUE);
        Microsoft::WRL::ComPtr<ID3D12Debug5> pDebug5;
        if (SUCCEEDED(pDebug1.As(&pDebug5)))
        {
            pDebug5->SetEnableAutoName(TRUE);
        }
    }

    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> pDred1;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDred1))))
    {
        pDred1->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pDred1->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pDred1->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
#endif
    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&s_pDevice)) != S_OK)
        return;

    // [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
    if (pDebug1 != nullptr)
    {
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue = nullptr;
        s_pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        // Disable breaking on this warning because of a suspected bug in the D3D12 SDK layer, see #9084 for details.
        const int D3D12_MESSAGE_ID_FENCE_ZERO_WAIT_ = 1424; // not in all copies of d3d12sdklayers.h
        D3D12_MESSAGE_ID disabledMessages[] = { (D3D12_MESSAGE_ID)D3D12_MESSAGE_ID_FENCE_ZERO_WAIT_ };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = 1;
        filter.DenyList.pIDList = disabledMessages;
        pInfoQueue->AddStorageFilterEntries(&filter);
    }
    DumpDredAfterRemoval(s_pDevice.Get());
#endif
}

void DX12::InitDirectCmdAlloc(ID3D12CommandAllocator** cmdAlloc)
{
    if (s_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc)) != S_OK) {
        LOG_DEBUG("[DX12] Create Direct CommandAllocator Failed!");
    }
}

void DX12::InitDirectCmdList(ID3D12CommandAllocator* cmdAlloc, ID3D12GraphicsCommandList** cmdlist)
{
    if (s_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(cmdlist)) != S_OK) {
        LOG_DEBUG("[DX12] Create Direct CommandList Failed!");
    }
}

void DX12::InitDirectCommandQueue(ID3D12CommandQueue** cmdQueue)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 1;
    if (s_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(cmdQueue)) != S_OK) {
        LOG_DEBUG("[DX12] Create Direct CommandQueue Failed!");
    }
}

void DX12::InitDescHeap(ID3D12DescriptorHeap** Heap, D3D12_DESCRIPTOR_HEAP_DESC& desc)
{
    if (s_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(Heap)) != S_OK) {
        LOG_DEBUG("[DX12] Create DescriptorHeap Failed!");
    }
}

void DX12::InitFence(ID3D12Fence** fence)
{
    if (s_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence)) != S_OK) {
        LOG_DEBUG("[DX12] Create Fence Failed!");
    }
}

void DX12::InitVideoProcessCmdAlloc(ID3D12CommandAllocator** cmdAlloc)
{
    if (s_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS, IID_PPV_ARGS(cmdAlloc)) != S_OK) {
        LOG_ERROR("[DX12]: Failed to create video CommandAllocator.");
        return;
    }
}

void DX12::InitVideoProcessCmdList(ID3D12CommandAllocator* cmdAlloc, ID3D12VideoProcessCommandList** cmdlist)
{
    if (s_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS, cmdAlloc, nullptr, IID_PPV_ARGS(cmdlist)) != S_OK) {
        LOG_ERROR("[DX12]: Failed to create video CommandList.");
        return;
    }
}

void DX12::InitVideoProcessCommandQueue(ID3D12CommandQueue** cmdQueue)
{
    D3D12_COMMAND_QUEUE_DESC qdesc = {};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS;
    qdesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    qdesc.NodeMask = 0;
    if (s_pDevice->CreateCommandQueue(&qdesc, IID_PPV_ARGS(cmdQueue)) != S_OK) {
        LOG_ERROR("[DX12]: Failed to create video CommandQueue.");
        return;
    }
}

void DX12::InitVideoDevice(ID3D12VideoDevice** device)
{
    if (s_pDevice->QueryInterface(IID_PPV_ARGS(device)) != S_OK) {
        LOG_WARN("[DX12]:  Unsupport video device.");
        return;
    }
}

void DX12::Destroy()
{
    s_pDevice->Release();
    s_pDevice = nullptr;
    Windows::Foundation::Uninitialize();
}
