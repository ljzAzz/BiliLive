#pragma once
#include "Platform/Window.h"
#include "Room/RoomSession.h"
#include "UI/ImGuiRenderer.h"
#include "Media/PlaybackController.h"
#include "UI/MainUI.h"
#include "Event/Event.h"

struct AppSetting {
	WindowSetting& windowSetting;
	RenderSetting& renderSetting;
	PlayerSetting& playerSetting;
	DanmakuSetting& damukuSetting;
};

class App {
private:
	std::atomic_bool m_running;
	Window& m_window;
	//std::unique_ptr<MainUI> m_pUI;
	static inline std::unique_ptr<App> s_pApp = nullptr;
	LockFreeQueue<Event*> m_events;
private:
	AppSetting m_settings;
private:
	App(Window& window);
	void BuildUI();
	bool OnAppCloseEvent(Event* e);
	bool OnRoomOpenEvent(Event* e);
	bool OnRoomCloseEvent(Event* e);
	bool OnRoomRemoveEvent(Event* e);
	void HandleEvents();
public:
	static App& CreateApp();
	static float GetAppRefreshRate() { return s_pApp->m_settings.windowSetting.appRefreshRate; }
	static AppSetting& GetSetting() { return s_pApp->m_settings; }
	~App();
	void Run();

};