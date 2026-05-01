#include "stdafx.h"
#include "ImGuiRenderer.h"
#include "Common/IconsFontAwesome6.h"

void ImGuiRenderer::InitRenderer(WindowHandle& whandle)
{
    //must set api before call this function
    assert(s_renderSetting.renderBackendApi != RenderBackendApi::null);
    InitImGui();
    static std::once_flag flag;
    std::call_once(flag, [&]() {
#ifdef __WINDOWS__
        auto renderBackend = RenderBackend::CreateRenderBackend(s_renderSetting.renderBackendApi, static_cast<void*>(&whandle));
        s_renderBackend = std::unique_ptr<RenderBackend>(renderBackend);
#endif
        renderBackend->SetSyncInterval(s_renderSetting.level);
        });

}

void ImGuiRenderer::InitImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    auto curPath = GetExePath();
    auto iniPath = curPath.concat("ImGui.ini").generic_string();
    io.IniFilename = iniPath.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    io.ConfigInputTrickleEventQueue = true;         

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 6.0f;
    style.ScaleAllSizes(1.f);
    style.FontScaleDpi = 1.f;
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 5.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    //set fonts to support chinese
    auto font = GetExePath();
    auto font1 = font.concat("/Resources/Fonts/NotoSansSC-Regular.ttf").generic_string();
    io.Fonts->AddFontFromFileTTF(
        font1.c_str(),
        18.0f,
        nullptr,
        io.Fonts->GetGlyphRangesChineseFull()
    );


    //merge IconsFontAwesome6 to support some icons
    static ImFontConfig iconsConfig;
    iconsConfig.MergeMode = true;
    iconsConfig.PixelSnapH = true;

    static const ImWchar iconsRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    font = GetExePath();
    auto font2 = font.concat("/Resources/Fonts/fa-solid-900.ttf").generic_string();
    io.Fonts->AddFontFromFileTTF(font2.c_str(), 16.0f, &iconsConfig, iconsRanges);
}

void ImGuiRenderer::SetRenderBackendApi(RenderBackendApi api)
{
	s_renderSetting.renderBackendApi= api;
}

void ImGuiRenderer::SetDpiScale(float scale)
{    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
}


void ImGuiRenderer::BeginRender()
{
    return s_renderBackend->BeginRender();
}

void ImGuiRenderer::EndRender()
{
    return s_renderBackend->EndRender();
}

void ImGuiRenderer::SwapBuffers(bool vsync)
{
    return s_renderBackend->SwapBuffers(vsync);
}

bool ImGuiRenderer::IsOccluded()
{
    return s_renderBackend->IsOccluded();
}

void ImGuiRenderer::ShutDown()
{
    s_renderBackend->ShutDown();
    ImGui::DestroyContext();
 
}


