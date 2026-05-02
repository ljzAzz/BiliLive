#include "stdafx.h"
#include "Application.h"
#include "Room/RoomSession.h"

App::App(Window& window)
	:m_window(window)
	,m_settings
	{	.windowSetting=Window::GetSetting(), 
		.renderSetting= ImGuiRenderer::GetSetting(),
		.playerSetting=PlaybackController::GetSetting(),
		.damukuSetting=ImGuiDanmaku::GetSetting()
	}
	,m_events(100)
{
	ImGuiRenderer::InitRenderer(m_window.GetWindowHandle());
	PlaybackController::Init();
	ImGuiDanmaku::Init();
	BuildUI();
	EventDispatcher::RegisterEventHandle<EventType::ApplicationClose>(std::bind(&App::OnAppCloseEvent,this,std::placeholders::_1));
	EventDispatcher::RegisterEventHandle<EventType::RoomOpen>(std::bind(&App::OnRoomOpenEvent,this,std::placeholders::_1));
	EventDispatcher::RegisterEventHandle<EventType::RoomClose>(std::bind(&App::OnRoomCloseEvent,this,std::placeholders::_1));
	EventDispatcher::RegisterEventHandle<EventType::RoomRemove>(std::bind(&App::OnRoomRemoveEvent,this,std::placeholders::_1));
}

void App::BuildUI()
{
	MainUI::Init();
}

bool App::OnAppCloseEvent(Event* e)
{
	Event* event = new AllRoomCloseEvent();
	EventDispatcher::Dispatch(event);
	m_running = false;
	return true;
}

bool App::OnRoomOpenEvent(Event* e)
{
	auto roomID = static_cast<RoomOpenEvent*>(e)->GetRoomID();
	auto room = std::make_shared<LiveRoom>(roomID);
	room->Start();
	MainUI::AddRenderLayer(room);
	return true;
}

bool App::OnRoomCloseEvent(Event* e)
{
	auto ctr = static_cast<RoomCloseEvent*>(e);
	auto uuid = ctr->GetUUID();
	std::jthread([=] {
		auto room = MainUI::GetRenderLayer(uuid);
		auto ptr = dynamic_cast<LiveRoom*>(room.get());
		assert(ptr != nullptr && "LiveRoom is null pointer");
		ptr->Stop();
		Event* e = new RoomRemoveEvent(uuid);
		EventDispatcher::Dispatch(e);
		}).detach();
	return true;
}

bool App::OnRoomRemoveEvent(Event* e)
{
	auto uuid = static_cast<RoomRemoveEvent*>(e)->GetUUID();
	MainUI::RemoveLayer(uuid);
	return true;
}

void App::HandleEvents()
{
	EventDispatcher::Dispatch();
}

App& App::CreateApp()
{
	static std::once_flag flag;
	std::call_once(flag, [=]() {
		Window::CreateWindowInstance();
		auto& window = Window::GetWindowInstance();
		window.Show();
		auto ptr = new App(Window::GetWindowInstance());
		s_pApp = std::unique_ptr<App>(ptr);
		s_pApp->m_running = true;
		});

	if (s_pApp != nullptr) {
		return *s_pApp;
	}
	else {
		LOG_ERROR("[App] Failed to create App.");
		exit(1);
	}
}

App::~App()
{
	ImGuiRenderer::ShutDown();
	PlaybackController::Destroy();
	m_window.Destroy();
}

void App::Run()
{
	m_window.Show();
	auto refreshRate = s_pApp->m_settings.windowSetting.appRefreshRate >
		s_pApp->m_settings.windowSetting.refreshRate ?
		s_pApp->m_settings.windowSetting.refreshRate : s_pApp->m_settings.windowSetting.appRefreshRate;

	auto target = std::chrono::duration_cast<std::chrono::steady_clock::duration>
		(std::chrono::duration<double>(1.0 / refreshRate));
	std::chrono::steady_clock::time_point now;
	std::chrono::steady_clock::time_point nextTick = std::chrono::steady_clock::time_point::min();
	bool tick = false;
	while (m_running) {
		m_window.PollEvents();
		if (ImGuiRenderer::IsOccluded()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}
		s_pApp->HandleEvents(); //maybe events will close app
		if (!m_running) {		//so here check running state
			break;
		}
		now = std::chrono::steady_clock::now();
		if (tick) {
			nextTick = now + target;
			tick = false;
		}
		if (nextTick != std::chrono::steady_clock::time_point::min() && now < nextTick) {
			auto diff = std::chrono::duration<double>(nextTick - now).count();
			int sleepTime = static_cast<int>(diff * 1000000 - 500);
			if (sleepTime > 0) {
				std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
			}
		}
		MainUI::Update();
		ImGuiRenderer::BeginRender();
		MainUI::RenderUI(m_window.IsFullScreen());
		ImGuiRenderer::EndRender();
		ImGuiRenderer::SwapBuffers(m_window.GetVsync());
		tick = true;
	}
}
