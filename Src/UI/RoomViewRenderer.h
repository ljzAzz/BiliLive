#pragma once
#include "RenderLayer.h"
#include <Media/VideoDecoder.h>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/policies.hpp>
struct DanmakuPending
{
	std::string text;
	bool sendByMe = false;
	ImU32 color = IM_COL32(255, 255, 255, 255);
};

struct DanmakuActive
{
	std::string text;
	ImU32 color = IM_COL32(255, 255, 255, 255);

	int lane = 0;

	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	float speed = 0.0f;

	bool sendByMe = false;

};

struct DanmakuSetting {
	static constexpr const float arr[] = {
		1.f,
		0.75f,
		0.5f,
		0.25f,
	};
	std::atomic_int areaIndex = 2;
	std::atomic<float> danmaArea = arr[areaIndex];
	std::atomic<float> fontSize = 48.0f;
	std::atomic_uint32_t alpha = 255;
};

class ImGuiDanmaku
{
private:
	static inline DanmakuSetting s_danmakuSetting;
private:
	ImFont* m_font = nullptr;
	float m_displayW;
	float m_displayH;
	float m_laneGap = 8.0f;           // lane gap
	float m_lanePitch = 32.0f;        // font_size + lane_gap
	float m_minGap = 24.0f;           // Minimum safe distance on the same lane
	float m_durationSeconds = 8.0f;   // The duration of a bullet comment crossing the screen
	int m_laneCount = 0;
	int m_laneCursor = 0;

	std::function<void(const std::string&)> m_addOneFunction;
	float m_addTimeGap;
	LockFreeQueue<DanmakuPending> m_pending;
	std::vector<DanmakuActive> m_active;

private:
	void EmitPending();
	int PickLane(float new_width, float new_speed) const;
	bool CanSpawnInLane(int lane, float new_width, float new_speed) const;
	const DanmakuActive* FindRightMostInLane(int lane) const;
	inline ImVec2 CalcTextSize(const char* text) const;

public:
	ImGuiDanmaku();
	~ImGuiDanmaku();
	void Push(std::string text, bool sbm = false, ImU32 color = IM_COL32(255, 255, 255, 255));
	void Update(ImDrawList* dl,float dt, ImVec2 displaySize, ImVec2 pmin);
	void Draw(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax);
	void SetDanmaArea(float area);
	void SetAddOneFunc(std::function<void(const std::string&)> adf) { m_addOneFunction = adf; }
	static auto& GetSetting() { return s_danmakuSetting; }
	static void Init();

};
