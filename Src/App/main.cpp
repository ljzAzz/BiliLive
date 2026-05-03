#include "stdafx.h"
#include "Platform/Window.h"
#include "Application.h"
#ifdef __WINDOWS__
#include <windows.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")
#endif

//#define LOG_TO_FILE

int main() {
#ifdef DEBUG
	LOG_INIT("logs/log.txt", Loglevel::debug);
#else
	LOG_INIT("logs/log.txt", Loglevel::info);
#endif
#ifdef __WINDOWS__
	timeBeginPeriod(1);
#endif

	LOG_DEBUG("[Boost] Boost version: {}, Boost version number: {}", BOOST_LIB_VERSION, BOOST_VERSION);

	PlaybackController::SetVideoDecoder(VideoDecoder::DX12VideoProcessor);
	PlaybackController::SetAudioPlaer(AudioOuput::WasApi);
	Window::SetWidth(1920);
	Window::SetHeight(1080);
	Window::SetVsync(false);
	ImGuiRenderer::SetSyncInterval(SyncInterval::level_1);
	User::Read();
	auto& app = App::CreateApp();
	app.Run();
	User::Save();
	LOG_SHUTDOWN();
#ifdef __WINDOWS__
	timeEndPeriod(1);
#endif
	return 0;
}