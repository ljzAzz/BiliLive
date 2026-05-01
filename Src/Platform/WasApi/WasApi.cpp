#include "stdafx.h"
#include "WasApi.h"

void WasApi::Init()
{
	//auto ret = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	//if (ret != S_OK) {
	//	LOG_ERROR("[AudioPlayerManager]: Failed to initialize COM library");
	//}
}

void WasApi::ShutDown()
{
	//CoUninitialize(); // final
}
