#pragma once
//wasapi
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <mmdeviceapi.h>


class WasApi {
private:

public:
	static void Init();
	static void ShutDown();
};