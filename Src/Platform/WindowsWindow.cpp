#include "stdafx.h"
#include "WindowsWindow.h"
#include <winuser.h>
#include <shellscalingapi.h>
#include <UI/ImGuiRenderer.h>
#include <Event/Event.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);                // Use ImGui::GetCurrentContext()

using PFN_GetDpiForMonitor =  HRESULT(WINAPI*)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

void WindowsWinodw::EnableDpiAwareness()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

//Get monitor size to init window position
std::pair<int, int> WindowsWinodw::GetMonitorSize() const
{
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    return { screenWidth, screenHeight };
}

//Get dpi sacle to init window size
//Followed ImGui's approach
float WindowsWinodw::GetMonitorDpiScale() const
{
    UINT xdpi = 96, ydpi = 96;
    static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll"); // Reference counted per-process
    static PFN_GetDpiForMonitor GetDpiForMonitorFn = nullptr;
    if (GetDpiForMonitorFn == nullptr && shcore_dll != nullptr)
    {
        GetDpiForMonitorFn = (PFN_GetDpiForMonitor)::GetProcAddress(shcore_dll, "GetDpiForMonitor");

    }
    if (GetDpiForMonitorFn != nullptr)
    {
        GetDpiForMonitorFn((HMONITOR)::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
        assert(xdpi == ydpi); 
    }
    else {
        LOG_WARN("[Window] Failed to get monitor dpi scale");
    }
    return xdpi / 96.0f;

}

WindowsWinodw::WindowsWinodw()
    : Window()
{
    EnableDpiAwareness();

    m_wc = { sizeof(m_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"BLIVE", nullptr};
    ::RegisterClassExW(&m_wc);

    auto monitorDpi = GetMonitorDpiScale();
    LOG_INFO("[Window] Monitor dpi scale is {}", monitorDpi);

    //auto fwidth = width * monitorDpi;
    //auto fheight = height * monitorDpi;

    auto [initPosX, initPosY] = GetMonitorSize();
    //initPosX = static_cast<int>((initPosX * monitorDpi - fwidth) / 2.0);
    //initPosY = static_cast<int>((initPosY * monitorDpi - fheight) / 2.0);
    initPosX = static_cast<int>((initPosX  - s_setting.width) / 2.0);
    initPosY = static_cast<int>((initPosY - s_setting.height) / 2.0);

    auto hwnd = ::CreateWindowW(m_wc.lpszClassName, L"BLive", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_VSCROLL, initPosX, initPosY, static_cast<int>(s_setting.width), static_cast<int>(s_setting.height), nullptr, nullptr, m_wc.hInstance, nullptr);

    if (hwnd) {
        EventDispatcher::RegisterEventHandle<EventType::WindowMaximize>(std::bind(&WindowsWinodw::MaximizeEvent, this, std::placeholders::_1));
        EventDispatcher::RegisterEventHandle<EventType::WindowRestore>(std::bind(&WindowsWinodw::RestoreEvent, this, std::placeholders::_1));
        EventDispatcher::RegisterEventHandle<EventType::WindowAdjust>(std::bind(&WindowsWinodw::Resize, this, std::placeholders::_1));

        m_scaleFactor = static_cast<float>(GetDpiForWindow(hwnd) / 96.0);
        LOG_INFO("[Window] Dpi scale is {}", m_scaleFactor);
        m_windowHandle = reinterpret_cast<std::uintptr_t>(hwnd);

        HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (hMonitor) {
            MONITORINFOEX monitorInfo;
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            if (GetMonitorInfo(hMonitor, &monitorInfo))
            {
                DEVMODE devMode;
                devMode.dmSize = sizeof(DEVMODE);
                if (EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode))
                {
                    s_setting.refreshRate = static_cast<float>(devMode.dmDisplayFrequency);
                    LOG_INFO("[Window] Get monitor refresh rate :{}", s_setting.refreshRate);
                }
            }
        }
    }
    else {
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        LOG_ERROR("[Window] Failed to create window");
    }
 
}

WindowsWinodw::~WindowsWinodw()
{
}

bool WindowsWinodw::Resize(Event* e)
{
    auto ae = static_cast<WindowAdjustEvent*>(e);
    auto width = ae->GetWidth();
    auto height = ae->GetHeight();
    auto [initPosX, initPosY] = GetMonitorSize();
    initPosX = static_cast<int>((initPosX - width) / 2.0);
    initPosY = static_cast<int>((initPosY - height) / 2.0);
    SetWindowPos(reinterpret_cast<HWND>(m_windowHandle), NULL, initPosX, initPosY, width, height, SWP_NOZORDER);
    return true;
}

void WindowsWinodw::Show()
{
    ::ShowWindow(reinterpret_cast<HWND>(m_windowHandle), SW_SHOWDEFAULT);
}

void WindowsWinodw::Destroy()
{
    ::DestroyWindow(reinterpret_cast<HWND>(m_windowHandle));
    ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
}

void WindowsWinodw::PollEvents()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool WindowsWinodw::MaximizeEvent(Event* e)
{
    ShowWindow(reinterpret_cast<HWND>(m_windowHandle), SW_MAXIMIZE);
    LOG_INFO("[Window] Maximize event handled");
    return true;
}

bool WindowsWinodw::RestoreEvent(Event* e)
{
    ShowWindow(reinterpret_cast<HWND>(m_windowHandle), SW_RESTORE);
    LOG_INFO("[Window] Restore event handled");
    return true;
}

bool WindowsWinodw::IsFullScreen()
{
    return IsZoomed(reinterpret_cast<HWND>(m_windowHandle));
}


LRESULT WINAPI WindowsWinodw::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    //it should not be to do like this, but just do it temporarily
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            Event* e = new WindowResizeEvent((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
            EventDispatcher::Dispatch(e);
        }
        return 0;
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        Event* e = new ApplicationCloseEvent();
        EventDispatcher::Dispatch(e);
        return 0;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
