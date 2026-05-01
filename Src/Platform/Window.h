#pragma once
#include <Event/Event.h>

//enum class EventType {
//	None = 0,
//	WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
//	AppTick, AppUpdate, AppRender,
//	KeyPressed, KeyReleased, KeyTyped,
//	MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled,
//};

using WindowHandle = std::uintptr_t;

struct WindowSetting {
	int width = 1920;
	int height = 1080;
	float refreshRate = 60.f;
	float appRefreshRate = 120.f;
	bool vsync = false;
};

class Window {
private:
	static inline std::unique_ptr<Window> s_pWindowInstance = nullptr;

protected:
	WindowHandle m_windowHandle;
	static inline WindowSetting s_setting;

protected:
	Window();
public:
	virtual ~Window() = default;
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	static void CreateWindowInstance();
	static Window& GetWindowInstance() { return *s_pWindowInstance; }
	virtual bool Resize(Event* e) = 0;
	virtual void Show() = 0;
	virtual void Destroy() = 0;
	virtual void PollEvents() = 0;
	virtual bool IsFullScreen() = 0;
	virtual bool MaximizeEvent(Event* e) = 0;
	virtual bool RestoreEvent(Event* e) = 0;
	auto GetRefreshRate() const { return s_setting.refreshRate; }
	WindowHandle& GetWindowHandle() { return m_windowHandle; }
	static int GetWidth() { return s_setting.width; }
	static int GetHeight() { return s_setting.height; }
	static bool GetVsync() { return s_setting.vsync; }
	static WindowSetting& GetSetting() { return s_setting; }
	static void SetWidth(int width) { s_setting.width = width; }
	static void SetHeight(int height) { s_setting.height = height; }
	static void SetVsync(bool v) { s_setting.vsync = v; }

};