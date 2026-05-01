#pragma once

#ifdef __WINDOWS__
using EventHandle = HANDLE;
#endif // _WINDOWS

enum class RenderBackendApi:int {
	Vulkan = 0,
	Dx12 = 1,
	OpenGL = 2,
	null,
};

enum class SyncInterval:int{
	level_1 = 0,
	level_2,
	level_3
};

class RenderBackend {
protected:
	RenderBackend() :m_syncInterval(SyncInterval::level_1) {}
	SyncInterval m_syncInterval;
public:
	virtual ~RenderBackend()=default;
	static RenderBackend* CreateRenderBackend(RenderBackendApi api,void* initData);
	virtual void BeginRender() = 0;
	virtual void EndRender() = 0;
	virtual void SwapBuffers(bool vsync) = 0;
	virtual void ShutDown() = 0;
	virtual bool IsOccluded() const = 0;
	virtual void SetSyncInterval(SyncInterval level) { m_syncInterval = level; }
};