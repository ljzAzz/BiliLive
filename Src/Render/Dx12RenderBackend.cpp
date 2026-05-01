#include "stdafx.h"
#include "Dx12RenderBackend.h"

void Dx12RenderBackend::ExampleDescriptorHeapAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
{
    IM_ASSERT(Heap == nullptr && FreeIndices.empty());
    Heap = heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    HeapType = desc.Type;
    HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
    HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
    HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
    FreeIndices.reserve((int)desc.NumDescriptors);
    for (int n = desc.NumDescriptors; n > 0; n--)
        FreeIndices.push_back(n - 1);
}
void Dx12RenderBackend::ExampleDescriptorHeapAllocator::Destroy()
{
    Heap = nullptr;
    FreeIndices.clear();
}
void Dx12RenderBackend::ExampleDescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
{
    std::unique_lock<std::mutex>(Mutex);
    IM_ASSERT(FreeIndices.Size > 0);
    int idx = FreeIndices.back();
    FreeIndices.pop_back();
    out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
    out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
}
void Dx12RenderBackend::ExampleDescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
{
    std::unique_lock<std::mutex>(Mutex);
    int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
    int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
    IM_ASSERT(cpu_idx == gpu_idx);
    FreeIndices.push_back(cpu_idx);
}



Dx12RenderBackend::Dx12RenderBackend(void* initData)
    :m_clearColor(ImVec4(0.45f, 0.55f, 0.60f, 1.00f))
    ,m_d3dDevice(DX12::GetDevice())
{
    EventDispatcher::RegisterEventHandle<EventType::WindowResize>(std::bind(&Dx12RenderBackend::ReSize, this, std::placeholders::_1));
    m_windowHandle = reinterpret_cast<HWND>(*static_cast<std::uintptr_t*>(initData));
    // Initialize Direct3D
    if (!CreateDeviceD3D(m_windowHandle))
    {
        CleanupDeviceD3D();
        LOG_ERROR("[RenderBackend] Failed to create dx12 device.");
        return;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_windowHandle);

    m_initInfo = {};
    m_initInfo.Device = m_d3dDevice.Get();
    m_initInfo.CommandQueue = m_pd3dCommandQueue;
    m_initInfo.NumFramesInFlight = RENDER_NUM_FRAMES_IN_FLIGHT;
    m_initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
    m_initInfo.UserData = static_cast<void*>(this);
    m_initInfo.SrvDescriptorHeap = m_pd3dSrvDescHeap;
    m_initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { auto ptr = static_cast<Dx12RenderBackend*>(info->UserData); return ptr->m_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
    m_initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { auto ptr = static_cast<Dx12RenderBackend*>(info->UserData); return ptr->m_pd3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&m_initInfo);

    // Before 1.91.6: our signature was using a single descriptor. From 1.92, specifying SrvDescriptorAllocFn/SrvDescriptorFreeFn will be required to benefit from new features.
    //ImGui_ImplDX12_Init(m_d3dDevice, RENDER_NUM_FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, m_pd3dSrvDescHeap, m_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), m_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Our state

}

Dx12RenderBackend::~Dx12RenderBackend()
{
}

// Helper functions
bool Dx12RenderBackend::CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could handle fullscreen mode differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = RENDER_NUM_BACK_BUFFERS;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = RENDER_NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        if (m_d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pd3dRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < RENDER_NUM_BACK_BUFFERS; i++)
        {
            m_mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = RENDER_SRV_HEAP_SIZE;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (m_d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pd3dSrvDescHeap)) != S_OK)
            return false;
        m_pd3dSrvDescHeapAlloc.Create(m_d3dDevice.Get(), m_pd3dSrvDescHeap);
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (m_d3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pd3dCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < RENDER_NUM_FRAMES_IN_FLIGHT; i++)
        if (m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frameContext[0].CommandAllocator, nullptr, IID_PPV_ARGS(&m_pd3dCommandList)) != S_OK ||
        m_pd3dCommandList->Close() != S_OK)
        return false;

    if (m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)) != S_OK)
        return false;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
        return false;

    {
        IDXGIFactory5* dxgiFactory = nullptr;
        IDXGISwapChain1* swapChain1 = nullptr;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
            return false;

        BOOL allow_tearing = FALSE;
        dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
        m_swapChainTearingSupport = (allow_tearing == TRUE);
        if (m_swapChainTearingSupport)
            sd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        if (dxgiFactory->CreateSwapChainForHwnd(m_pd3dCommandQueue, hWnd, &sd, nullptr, nullptr, &swapChain1) != S_OK)
            return false;
        if (swapChain1->QueryInterface(IID_PPV_ARGS(&m_pSwapChain)) != S_OK)
            return false;
        if (m_swapChainTearingSupport)
            dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

        swapChain1->Release();
        dxgiFactory->Release();
        //m_pSwapChain->SetMaximumFrameLatency(RENDER_NUM_BACK_BUFFERS);
        m_pSwapChain->SetMaximumFrameLatency(1);
        m_hSwapChainWaitableObject = m_pSwapChain->GetFrameLatencyWaitableObject();
    }

    CreateRenderTarget();
    return true;
}

void Dx12RenderBackend::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (m_pSwapChain) { m_pSwapChain->SetFullscreenState(false, nullptr); m_pSwapChain->Release(); m_pSwapChain = nullptr; }
    if (m_hSwapChainWaitableObject != nullptr) { CloseHandle(m_hSwapChainWaitableObject); }
    for (UINT i = 0; i < RENDER_NUM_FRAMES_IN_FLIGHT; i++)
        if (m_frameContext[i].CommandAllocator) { m_frameContext[i].CommandAllocator->Release(); m_frameContext[i].CommandAllocator = nullptr; }
    if (m_pd3dCommandQueue) { m_pd3dCommandQueue->Release(); m_pd3dCommandQueue = nullptr; }
    if (m_pd3dCommandList) { m_pd3dCommandList->Release(); m_pd3dCommandList = nullptr; }
    if (m_pd3dRtvDescHeap) { m_pd3dRtvDescHeap->Release(); m_pd3dRtvDescHeap = nullptr; }
    if (m_pd3dSrvDescHeap) { m_pd3dSrvDescHeap->Release(); m_pd3dSrvDescHeap = nullptr; }
    if (m_fence) { m_fence->Release(); m_fence = nullptr; }
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* pDebug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
#endif
}

void Dx12RenderBackend::CreateRenderTarget()
{
    for (UINT i = 0; i < RENDER_NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        m_d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, m_mainRenderTargetDescriptor[i]);
        m_mainRenderTargetResource[i] = pBackBuffer;
    }
}

void Dx12RenderBackend::CleanupRenderTarget()
{
    WaitForPendingOperations();

    for (UINT i = 0; i < RENDER_NUM_BACK_BUFFERS; i++)
        if (m_mainRenderTargetResource[i]) { m_mainRenderTargetResource[i]->Release(); m_mainRenderTargetResource[i] = nullptr; }
}

bool Dx12RenderBackend::ReSize(Event* e)
{
    auto re = static_cast<WindowResizeEvent*>(e);
    auto w = re->GetWidth();
    auto h = re->GetHeight();
    if (m_d3dDevice != nullptr)
    {
        CleanupRenderTarget();
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        m_pSwapChain->GetDesc1(&desc);
        HRESULT result = m_pSwapChain->ResizeBuffers(0, w, h, desc.Format, desc.Flags);
        IM_ASSERT(SUCCEEDED(result) && "Failed to resize swapchain.");
        CreateRenderTarget();
        return true;
    }
    return false;
}

void Dx12RenderBackend::WaitForPendingOperations()
{
    m_pd3dCommandQueue->Signal(m_fence, ++m_fenceLastSignaledValue);

    m_fence->SetEventOnCompletion(m_fenceLastSignaledValue, m_fenceEvent);
    ::WaitForSingleObject(m_fenceEvent, INFINITE);
}

Dx12RenderBackend::FrameContext* Dx12RenderBackend::WaitForNextFrameContext()
{
    FrameContext* frame_context = &m_frameContext[m_frameIndex % RENDER_NUM_FRAMES_IN_FLIGHT];
    if (m_fence->GetCompletedValue() < frame_context->FenceValue)
    {
        m_fence->SetEventOnCompletion(frame_context->FenceValue, m_fenceEvent);
        HANDLE waitableObjects[] = { m_hSwapChainWaitableObject, m_fenceEvent };
        ::WaitForMultipleObjects(2, waitableObjects, TRUE, INFINITE);
    }
    else
        ::WaitForSingleObject(m_hSwapChainWaitableObject, INFINITE);

    return frame_context;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
//LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
//{
//    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
//        return true;
//
//    switch (msg)
//    {
//    case WM_SIZE:
//        if (m_d3dDevice != nullptr && wParam != SIZE_MINIMIZED)
//        {
//            CleanupRenderTarget();
//            DXGI_SWAP_CHAIN_DESC1 desc = {};
//            m_pSwapChain->GetDesc1(&desc);
//            HRESULT result = m_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), desc.Format, desc.Flags);
//            IM_ASSERT(SUCCEEDED(result) && "Failed to resize swapchain.");
//            CreateRenderTarget();
//        }
//        return 0;
//    case WM_SYSCOMMAND:
//        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
//            return 0;
//        break;
//    case WM_DESTROY:
//        ::PostQuitMessage(0);
//        return 0;
//    }
//    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
//}

void Dx12RenderBackend::BeginRender()
{
    m_swapChainOccluded = false;
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Dx12RenderBackend::EndRender()
{
    ImGui::Render();
}

void Dx12RenderBackend::SwapBuffers(bool vsync)
{
    FrameContext* frameCtx = WaitForNextFrameContext();
    UINT backBufferIdx = m_pSwapChain->GetCurrentBackBufferIndex();
    frameCtx->CommandAllocator->Reset();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_mainRenderTargetResource[backBufferIdx];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_pd3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
    m_pd3dCommandList->ResourceBarrier(1, &barrier);

    // Render Dear ImGui graphics
    const float clear_color_with_alpha[4] = { m_clearColor.x * m_clearColor.w, m_clearColor.y * m_clearColor.w, m_clearColor.z * m_clearColor.w, m_clearColor.w };
    m_pd3dCommandList->ClearRenderTargetView(m_mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, nullptr);
    m_pd3dCommandList->OMSetRenderTargets(1, &m_mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);
    m_pd3dCommandList->SetDescriptorHeaps(1, static_cast<ID3D12DescriptorHeap*const*>(& m_pd3dSrvDescHeap));
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pd3dCommandList);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_pd3dCommandList->ResourceBarrier(1, &barrier);
    m_pd3dCommandList->Close();

    m_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_pd3dCommandList);

    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    m_pd3dCommandQueue->Signal(m_fence, ++m_fenceLastSignaledValue);
    frameCtx->FenceValue = m_fenceLastSignaledValue;

    HRESULT hr = vsync? m_pSwapChain->Present(static_cast<int>(m_syncInterval),0):m_pSwapChain->Present(0, m_swapChainTearingSupport ? DXGI_PRESENT_ALLOW_TEARING : 0); // Present without vsync
    m_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    m_frameIndex++;
}

void Dx12RenderBackend::ShutDown()
{
    WaitForPendingOperations();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    CleanupDeviceD3D();
    DX12::Destroy();

}

void Dx12RenderBackend::AllocDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu)
{
    m_pd3dSrvDescHeapAlloc.Alloc(&cpu, &gpu);
}

void Dx12RenderBackend::FreeDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu)
{
    m_pd3dSrvDescHeapAlloc.Free(cpu, gpu);

}
