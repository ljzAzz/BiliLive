#include "stdafx.h"
#include "RoomViewRenderer.h"
#include <User/UserData.h>

ImGuiDanmaku::ImGuiDanmaku()
	:m_displayW(0)
	,m_displayH(0)
	,m_pending(1000)
	,m_addTimeGap(0.f)
{
	m_font = ImGui::GetFont();
}

ImGuiDanmaku::~ImGuiDanmaku()
{
}

void ImGuiDanmaku::Push(std::string text, bool sbm, ImU32 color)
{
	m_pending.Push({ std::move(text), sbm, color&0xffffff00|s_danmakuSetting.alpha.load(std::memory_order_relaxed)});
}

void ImGuiDanmaku::Update(ImDrawList* dl, float dt, ImVec2 displaySize, ImVec2 pmin)
{
	auto io = ImGui::GetIO();
	auto fontSize = s_danmakuSetting.fontSize.load(std::memory_order_relaxed);
	m_addTimeGap -= io.DeltaTime;
	bool hovered = false;
	int plane = -1;
	auto display_w = displaySize.x;
	auto display_h = displaySize.y;
	if (display_w <= 1.0f || display_h <= 1.0f)
		return;

	m_displayW = display_w;
	m_displayH = display_h;

	m_lanePitch = fontSize + m_laneGap;
	m_laneCount = std::max(1, (int)std::floor(m_displayH *
		s_danmakuSetting.danmaArea.load(std::memory_order_relaxed) / m_lanePitch));

	for (auto& d : m_active)
	{
		if (plane != -1) {
			if (d.lane == plane) {
				d.speed = 0.0f;
				continue;
			}
		}
		if (d.speed == 0.0f) {
			d.speed = (m_displayW + d.width) / m_durationSeconds;
		}
		if (User::IsLogin()&& !hovered && m_addTimeGap<=0.f) {
			static constexpr auto text = "+1";
			static const auto s  = CalcTextSize(text);
			static const auto size = ImVec2(s.x * 0.6f,s.y * 0.6f);
			static const auto r = size.x > size.y ? size.x * 0.5f : size.y * 0.5f;

			auto leftX = pmin.x + d.x;
			auto rightX = leftX + d.width;
			auto bottomY = pmin.y + d.y;
			auto topY = pmin.y + d.y + d.height;

			if (io.MousePos.x > leftX && io.MousePos.x < rightX
				&& io.MousePos.y > bottomY && io.MousePos.y < topY) {
				static constexpr auto c = IM_COL32(255, 255, 255, 255);;
				dl->AddCircle(ImVec2(rightX + r + 0.5f, (bottomY + topY) * 0.5f), r, c);
				dl->AddText(m_font, fontSize * 0.6f, 
					ImVec2(rightX + r + 0.5f - size.x * 0.5f, (bottomY + topY) * 0.5f - size.y * 0.5f), 
					c, 
					text);
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				hovered = true;
				d.speed = 0;
				plane = d.lane;
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					m_addTimeGap = 4.f;
					plane = -1;
					m_addOneFunction(d.text);
				}
			}
		}

		d.x -= d.speed * dt;
	}

	m_active.erase(
		std::remove_if(m_active.begin(), m_active.end(),
			[](const DanmakuActive& d)
			{
				return (d.x + d.width) < 0.0f;
			}),
		m_active.end());

	EmitPending();
}

void ImGuiDanmaku::Draw(ImDrawList* dl, const ImVec2& pmin, const ImVec2& pmax)
{
	if (!dl) {
		return;
	}

	dl->PushClipRect(pmin, pmax, true);

	for (const auto& d : m_active)
	{
		ImVec2 pos(pmin.x + d.x, pmin.y + d.y);
		auto fontSize = s_danmakuSetting.fontSize.load(std::memory_order_relaxed);
		// fort shadow
		dl->AddText(m_font, fontSize,
			ImVec2(pos.x + 1.0f, pos.y + 1.0f),
			IM_COL32(0, 0, 0, s_danmakuSetting.alpha.load(std::memory_order_relaxed)),
			d.text.c_str());

		dl->AddText(m_font, fontSize,
			pos,
			d.color,
			d.text.c_str());

		if (d.sendByMe) {
			auto p1 =  ImVec2(pos.x - 1.5f, pos.y - 1.5f);
			auto p2 =  ImVec2(pos.x + d.width + 1.5f, pos.y - 1.5f);
			auto p3 =  ImVec2(pos.x + d.width + 1.5f, pos.y + d.height + 1.5f);
			auto p4 =  ImVec2(pos.x - 1.5f, pos.y + d.height + 1.5f);
			static constexpr auto c = IM_COL32(255, 255, 255, 255);;
			dl->AddQuad(p1, p2, p3, p4, c);
		}
	}

	dl->PopClipRect();
}

void ImGuiDanmaku::SetDanmaArea(float area)
{
	s_danmakuSetting.danmaArea = area;
}

void ImGuiDanmaku::Init()
{

}

void ImGuiDanmaku::EmitPending()
{
	// Try to send as many frames as possible, but maintain the order
	while (!m_pending.Empty())
	{
		DanmakuPending msg;
		if (!m_pending.Pop(msg)) {
			LOG_ERROR("[ImGuiDanmaku] Failed to emit danmu pending");
		}

		auto textSize= CalcTextSize(msg.text.c_str());

		// Try to make the total duration of all bullet comments close to each other
		// The longer the bullet comment, the faster its speed
		float speed = (m_displayW + textSize.x) / m_durationSeconds;

		int lane = PickLane(textSize.x, speed);
		if (lane < 0)
			break; // Currently, no track is available. Try again in the next frame

		DanmakuActive d;
		d.text = msg.text;
		d.color = msg.color;
		d.lane = lane;
		d.width = textSize.x;
		d.height = textSize.y;
		d.speed = speed;
		d.x = m_displayW;                  // Spawn from the right boundary
		d.y = lane * m_lanePitch;
		d.sendByMe = msg.sendByMe;

		m_active.push_back(std::move(d));
	}
}

int ImGuiDanmaku::PickLane(float new_width, float new_speed) const
{
	if (m_laneCount <= 0)
		return -1;

	// Use a rotating vernier to avoid constantly jamming the top few tracks
	for (int i = 0; i < m_laneCount; ++i)
	{
		int lane = (m_laneCursor + i) % m_laneCount;
		if (CanSpawnInLane(lane, new_width, new_speed))
		{
			// mutable is inconvenient, 
			// simply returning the previous value and letting the outside update it is also fine here
			const_cast<ImGuiDanmaku*>(this)->m_laneCursor = (lane + 1) % m_laneCount;
			return lane;
		}
	}

	return -1;
}

bool ImGuiDanmaku::CanSpawnInLane(int lane, float new_width, float new_speed) const
{
	const DanmakuActive* tail = FindRightMostInLane(lane);
	if (!tail)
		return true;

	if (tail->speed == 0.0f) {
		return false;
	}
	// The previous item's current right edge
	float rp = tail->x + tail->width;

	// Is there currently sufficient birth spacing
	float g0 = m_displayW - rp - m_minGap;
	if (g0 < 0.0f)
		return false;

	// If the new one is not faster than the previous one, there will be no rear-end collision
	if (new_speed <= tail->speed)
		return true;

	// The new one is faster. Let's see if it can catch up before the previous one leaves
	float dv = new_speed - tail->speed;
	float t_catch = g0 / dv;
	float t_prev_exit = rp / tail->speed; // The time from the previous right edge to x=0

	return t_catch >= t_prev_exit;
}

const DanmakuActive* ImGuiDanmaku::FindRightMostInLane(int lane) const
{
	const DanmakuActive* best = nullptr;
	float best_r = -FLT_MAX;

	for (const auto& d : m_active)
	{
		if (d.lane != lane)
			continue;

		float r = d.x + d.width;
		if (d.speed == 0.f) {
			best = &d;
			return best;
		}
		if (r > best_r)
		{
			best_r = r;
			best = &d;
		}
	}
	return best;
}

inline ImVec2 ImGuiDanmaku::CalcTextSize(const char* text) const
{
	ImVec2 sz = m_font->CalcTextSizeA(s_danmakuSetting.fontSize, FLT_MAX, 0.0f, text);
	return sz;
}

