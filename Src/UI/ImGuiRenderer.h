#pragma once
//--------------ImGui-------------
#include <imgui.h>
#include <imgui_internal.h>
#include <imstb_rectpack.h>
#include <imstb_textedit.h>
#include <imstb_truetype.h>
#include <imconfig.h>
#include <misc/cpp/imgui_stdlib.h>
#include <tchar.h>
//--------------ImGui-------------

#include <Render/RenderBackend.h>
#include <Platform/Window.h>

struct RenderSetting {
	RenderBackendApi renderBackendApi = RenderBackendApi::Dx12;
	SyncInterval level = SyncInterval::level_1;
	int backendApiIndex = 1;
	int vsyncLevelIndex = 0;
};

class ImGuiRenderer {
private:
	static inline std::unique_ptr<RenderBackend> s_renderBackend = nullptr;

	static inline RenderSetting s_renderSetting;
private:
	static void InitImGui();	
public:
	static void SetRenderBackendApi(RenderBackendApi api);
	static void SetDpiScale(float scale);
	static ImGuiStyle& GetStyle() { return ImGui::GetStyle(); }
	static void InitRenderer(WindowHandle& whandle);
	static void BeginRender();
	static void EndRender();
	static void SwapBuffers(bool vsync = false);
	static bool IsOccluded();
	static void ShutDown();
	static RenderBackend& GetRenderBackend() { return *s_renderBackend; }
	static RenderSetting& GetSetting() { return s_renderSetting; }
	static void SetSyncInterval(SyncInterval level) {
		if (s_renderBackend) {
			s_renderBackend->SetSyncInterval(level); 
		}
		s_renderSetting.level = level;
	}
};

