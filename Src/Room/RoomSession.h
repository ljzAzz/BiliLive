#pragma once
#include "Network/HttpClient.h"
#include "Network/WebSocketClient.h"
#include "Media/PlaybackController.h"
#include "UI/RoomViewRenderer.h"

class LiveRoom:public std::enable_shared_from_this<LiveRoom>, public RenderLayer {
private:
	struct VolumeWidget
	{
		int volume;        // 0-100
		int lastNonzero;
		bool  muted = false;
		bool  expanded = false; 

		float keepOpen = 0.0f;      //display time
		bool  dragging = false;
	} m_volumeWidget;
private:
	uint64_t m_roomID;
	uint64_t m_texture;
	RoomInfo m_roomInfo;
	RoomState m_roomState;

	std::atomic_bool m_running;
	std::shared_ptr<BliveClient> m_bliveClient;
	std::shared_ptr<DanmakuClient> m_danmakuClient;
	std::shared_ptr<PlaybackController> m_playController;
	ImGuiWindowFlags m_windowFlags;
	ImGuiDanmaku m_danmuKu;
	std::string m_danmu;
	float m_inputDispearTime;
	bool m_inputRender;
	bool m_uiActiveLastFrame;
	bool m_danmakuEnabled;
	bool m_danmaActive;
	std::atomic_bool m_renewDanmuku;
	std::thread m_renewThread;
	std::atomic_int m_renewCount;
	std::once_flag flag;

	std::once_flag m_stopFlag;
	static inline constexpr int s_danmuMaxSize = 40;

private:
	inline void RenderDanmaku(ImVec2 cs, bool focus);
	inline void RenderPlayerUI(ImVec2 videoMin, ImVec2 videoMax, bool full);
	inline const char* GetVolumeIcon();
	inline void ToggleMute();
	inline void DrawVolumeWidget();
	inline float GetEffectiveGain();
	inline void SetVolume() const { m_playController->SetVolume(m_volumeWidget.volume); }
	inline void SetMute() const { m_playController->SetMute(m_volumeWidget.muted); }
private:
	static int InputCallback(ImGuiInputTextCallbackData* data);
	inline static void TruncateToMaxChars(std::string& str, int maxChars);
	void Render(bool full);
public:
	LiveRoom(uint64_t roomID);
	void Start();
	void Stop();
	virtual void OnAttach() override;
	virtual void OnUpdate() override;
	virtual void OnImGuiRender(bool fullScreen) override;
	virtual void OnDetach() override;
	~LiveRoom();
};