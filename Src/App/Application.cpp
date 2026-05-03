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
	LOG_INFO("[APP] Limited refreshRate :{}", refreshRate);

	bool tick = false;
	bool open = true;
	using Clock = std::chrono::steady_clock;
	auto frameDuration = std::chrono::duration_cast<Clock::duration>(
		std::chrono::duration<double>(1.0 / refreshRate));

	auto nextFrame = Clock::now();

	while (m_running)
	{
		s_pApp->HandleEvents();
		if (!m_running) break;

		m_window.PollEvents();
		if (ImGuiRenderer::IsOccluded()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			nextFrame = Clock::now() + frameDuration; 
			continue;
		}

		auto now = Clock::now();
		if (now < nextFrame) {
			auto waitTime = nextFrame - now;
			auto sleepTime = waitTime - std::chrono::microseconds(500);
			if (sleepTime > std::chrono::microseconds(0))
				std::this_thread::sleep_for(sleepTime);
		}

		while (Clock::now() < nextFrame) {
			std::this_thread::yield();
		}

		MainUI::Update();
		ImGuiRenderer::BeginRender();
		MainUI::RenderUI(m_window.IsFullScreen());
		auto io = ImGui::GetIO();
		if (open) {
			ImGui::ShowDemoWindow(&open);
		}
		ImGui::Begin("FrameRate");
		ImGui::SameLine();

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::End();
		ImGuiRenderer::EndRender();
		ImGuiRenderer::SwapBuffers(m_window.GetVsync());

		nextFrame += frameDuration;

		if (nextFrame < Clock::now()) {
			nextFrame = Clock::now() + frameDuration;
		}
	}
}
