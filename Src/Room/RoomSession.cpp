#include "stdafx.h"
#include "RoomSession.h"
#include "UI/MainUI.h"
#include "Event/Event.h"
#include <Common/IconsFontAwesome6.h>

void LiveRoom::RenderDanmaku(ImVec2 cs, bool focus)
{
	ImVec2 pmin = ImGui::GetItemRectMin();
	ImVec2 pmax = ImGui::GetItemRectMax();
	float dt = ImGui::GetIO().DeltaTime;
	auto dl = ImGui::GetWindowDrawList();
	m_danmuKu.Update(dl, dt, cs, pmin);
	if (focus) {
		m_danmuKu.Draw(dl, pmin, pmax);
	}
}

inline void LiveRoom::RenderPlayerUI(ImVec2 videoMin, ImVec2 videoMax, bool full)
{
	constexpr float kMargin = 12.0f;
	const float frameH = ImGui::GetFrameHeight();
	const float barH = frameH + 14.0f;

	ImVec2 barMin(videoMin.x + kMargin, videoMax.y - barH - kMargin);
	ImVec2 barMax(videoMax.x - kMargin, videoMax.y - kMargin);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(barMin, barMax, IM_COL32(20, 20, 20, 180), 8.0f);

	// screen bottom
	ImGui::SetCursorScreenPos(ImVec2(barMin.x + 8.0f, barMin.y + 6.0f));

	bool send = false;

	ImGui::BeginGroup();
	if (ImGui::Button("刷新")) {
		//RefreshRoom(); //todo
	}
	m_uiActiveLastFrame |= ImGui::IsItemHovered() || ImGui::IsItemActive();

	ImGui::SameLine();
	DrawVolumeWidget();
	ImGui::SameLine();

	ImGui::Checkbox("弹幕", &m_danmakuEnabled);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
		m_danmakuEnabled = !m_danmakuEnabled;
	}
	m_uiActiveLastFrame |= ImGui::IsItemHovered() || ImGui::IsItemActive();

	ImGui::SameLine();

	float inputW = ImGui::GetContentRegionAvail().x * 0.9f;

	ImGui::BeginDisabled(!m_danmakuEnabled);

	//if (full) {
		ImGui::SetNextItemWidth(inputW);
		if (m_danmaActive) {
			ImGui::SetKeyboardFocusHere();
		}

		send = ImGui::InputTextWithHint("##danmakuInput", "发个友善的弹幕...", &m_danmu,
			ImGuiInputTextFlags_EnterReturnsTrue| ImGuiInputTextFlags_CallbackAlways,InputCallback);
		m_uiActiveLastFrame |= ImGui::IsItemHovered() || ImGui::IsItemActive();
		m_danmaActive = ImGui::IsItemActive();

		ImGui::SameLine();
		ImGui::Button("发送");
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			send = true;
		}
		m_uiActiveLastFrame |= ImGui::IsItemHovered() || ImGui::IsItemActive();
	//}
	//else {
	//	auto cursorX = ImGui::GetCursorPosX() + inputW + ImGui::GetItemRectSize().x + 5.0f;
	//	ImGui::SetCursorPosX(cursorX);
	//}

	ImGui::EndDisabled();
	//full?ImGui::SameLine(): void();
	ImGui::SameLine();

	auto btn = full ? ICON_FA_MINIMIZE : ICON_FA_MAXIMIZE;
	ImGui::Button(btn);
	if(ImGui::IsItemClicked(ImGuiMouseButton_Left)){
		Event* e = nullptr;
		if (full) {
			e = new WindowRestoreEvent();
		}
		else {
			e = new WindowMaximizeEvent();
		}
		EventDispatcher::Dispatch(e);
	}

	m_uiActiveLastFrame |= ImGui::IsItemHovered() || ImGui::IsItemActive();
	if (send && m_danmakuEnabled && !m_danmu.empty()) {
		m_bliveClient->SendDanmu(m_danmu);
		m_danmu.clear();
	}

	ImGui::EndGroup();

	return;
}

inline const char* LiveRoom::GetVolumeIcon()
{
	if (m_volumeWidget.muted || m_volumeWidget.volume <= 0.0f)
		return ICON_FA_VOLUME_OFF;
	return (m_volumeWidget.volume < 50.0f) ? ICON_FA_VOLUME_LOW : ICON_FA_VOLUME_HIGH;
}

inline void LiveRoom::ToggleMute()
{
	if (!m_volumeWidget.muted && m_volumeWidget.volume > 0.0f)
	{
		m_volumeWidget.lastNonzero = m_volumeWidget.volume;
		m_volumeWidget.muted = true;
	}
	else
	{
		if (m_volumeWidget.volume <= 0.0f) {
			m_volumeWidget.volume = (m_volumeWidget.lastNonzero > 0.0f) ? m_volumeWidget.lastNonzero : 50;
		}
		m_volumeWidget.muted = false;
	}
}

inline void LiveRoom::DrawVolumeWidget()
{
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	const float height = ImGui::GetFrameHeight();
	const ImVec2 btnSize(28.0f, height);
	const ImVec2 sliderSize(16.0f, 96.0f);
	const float gap = 6.0f;
	const float holdTime = 0.16f;
	const float rounding = 6.0f;

	//ImGui::InvisibleButton("##vol_btn", btnSize);
	//ImVec2 bmin = ImGui::GetItemRectMin();
	//ImVec2 bmax = ImGui::GetItemRectMax();

	//const ImU32 colBtn = ImGui::GetColorU32(btnHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
	const ImU32 colBorder = ImGui::GetColorU32(ImGuiCol_Border);
	const ImU32 colText = ImGui::GetColorU32(ImGuiCol_Text);

	//dl->AddRectFilled(bmin, bmax, colBtn, rounding);
	//dl->AddRect(bmin, bmax, colBorder, rounding);

	const char* icon = (m_volumeWidget.muted || m_volumeWidget.volume <= 0.0f) ?
		ICON_FA_VOLUME_OFF : (m_volumeWidget.volume < 50.0f ? ICON_FA_VOLUME_LOW : ICON_FA_VOLUME_HIGH);
	//ImVec2 ts = ImGui::CalcTextSize(icon);
	//dl->AddText(
	//	ImVec2((bmin.x + bmax.x - ts.x) * 0.5f, (bmin.y + bmax.y - ts.y) * 0.5f),
	//	colText,
	//	icon
	//);
	ImGui::Button(icon, btnSize);
	bool btnHovered = ImGui::IsItemHovered();
	bool btnClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
	ImVec2 bmin = ImGui::GetItemRectMin();
	ImVec2 bmax = ImGui::GetItemRectMax();
	m_uiActiveLastFrame |= btnHovered || btnClicked;

	if (btnClicked)
	{
		ToggleMute();
		m_volumeWidget.keepOpen = holdTime;
		SetMute();
	}

	if (btnHovered)
	{
		m_volumeWidget.keepOpen = holdTime;
	}
	else {
		m_volumeWidget.keepOpen = std::max(0.0f, m_volumeWidget.keepOpen - io.DeltaTime);
	}
	auto volumeStr = std::to_string(m_volumeWidget.volume) + "%";
	auto vStrSize = ImGui::CalcTextSize(volumeStr.c_str());

	ImVec2 smin(bmin.x + (btnSize.x - sliderSize.x) * 0.5f, bmin.y - gap - sliderSize.y - vStrSize.y);
	ImVec2 smax(smin.x + sliderSize.x, smin.y + sliderSize.y);

	bool showSlider = (m_volumeWidget.keepOpen > 0.0f) || m_volumeWidget.dragging;
	bool sliderHovered = false;

	if (showSlider)
	{
		sliderHovered = ImGui::IsMouseHoveringRect(smin, smax, true);
		if (sliderHovered) {
			m_volumeWidget.keepOpen = holdTime;
		}

		bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
		bool mouseClickedSlider = sliderHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		bool mouseSlideSlider = btnHovered && io.MouseWheel != 0;

		m_uiActiveLastFrame |= mouseClickedSlider || mouseSlideSlider;

		if (mouseClickedSlider || (m_volumeWidget.dragging && mouseDown))
		{
			m_volumeWidget.dragging = true;

			float t = (smax.y - io.MousePos.y) / (smax.y - smin.y);
			t = std::clamp(t, 0.0f, 1.0f);

			float newVolume = t * 100.0f;
			if (newVolume != m_volumeWidget.volume)
			{
				m_volumeWidget.volume = static_cast<int>(newVolume);

				if (m_volumeWidget.volume > 0.0f)
				{
					m_volumeWidget.lastNonzero = m_volumeWidget.volume;
					m_volumeWidget.muted = false;
				}
				else
				{
					m_volumeWidget.muted = true;
				}

				SetVolume();
			}
		}
		else if(mouseSlideSlider){
			float delta = io.MouseWheel;
			delta = delta > 0 ?  0.05f : -0.05f;
			float t = m_volumeWidget.volume * 0.01f + delta;
			t = std::clamp(t, 0.0f, 1.0f) * 100.0f;

			if (t != m_volumeWidget.volume)
			{
				m_volumeWidget.volume = static_cast<int>(t);

				if (m_volumeWidget.volume > 0.0f)
				{
					m_volumeWidget.lastNonzero = m_volumeWidget.volume;
					m_volumeWidget.muted = false;
				}
				else
				{
					m_volumeWidget.muted = true;
				}
				SetVolume();

			}
		}

		if (m_volumeWidget.dragging && !mouseDown)
		{
			m_volumeWidget.dragging = false;
		}

		// draw slider
		const ImU32 colBg = ImGui::GetColorU32(ImVec4(0.10f, 0.10f, 0.10f, 0.85f));
		const ImU32 colFill = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
		const ImU32 colKnob = ImGui::GetColorU32(ImGuiCol_Text);

		dl->AddRectFilled(smin, smax, colBg, rounding);
		dl->AddRect(smin, smax, colBorder, rounding);

		float shown = (m_volumeWidget.muted ? 0.0f : m_volumeWidget.volume) / 100.0f;
		float fy = smax.y - shown * (smax.y - smin.y);

		dl->AddText(
			ImVec2(bmin.x + (btnSize.x - vStrSize.x) * 0.5f, bmin.y - gap - vStrSize.y),
			colText,
			volumeStr.c_str()
		);
		dl->AddRectFilled(ImVec2(smin.x, fy), smax, colFill, rounding);
		dl->AddCircleFilled(ImVec2((smin.x + smax.x) * 0.5f, fy), 5.0f, colKnob);
	}

	return;
}

inline float LiveRoom::GetEffectiveGain()
{
	return m_volumeWidget.muted ? 0.0f : (m_volumeWidget.volume / 100.0f);
}

int LiveRoom::InputCallback(ImGuiInputTextCallbackData* data)
{
	static constexpr const auto MAX_CHARS = 40;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
		if (ImGui::GetIO().KeyCtrl) {
			return 0;
		}
		if (data->Buf) {
			std::string str(data->Buf);
			TruncateToMaxChars(str, 40);

			if (str.size() < (size_t)data->BufTextLen) {
				strncpy(data->Buf, str.c_str(), data->BufSize);
				data->BufTextLen = (int)str.size();
				data->BufDirty = true;
			}
		}
		else {
			return 0;
		}

	}
	return 0;
}

inline void LiveRoom::TruncateToMaxChars(std::string& str, int maxChars)
{
	int cnt = 0;
	size_t i = 0;
	size_t lastCharEnd = 0;
	while (i < str.size() && cnt < maxChars) {
		unsigned char c = str[i];

		if (c < 0x80) {
			i += 1;
			cnt++;
		}
		else if ((c & 0xE0) == 0xC0) {
			i += 2;
			cnt++;
		}
		else if ((c & 0xF0) == 0xE0) {
			i += 3;
			cnt++;
		}
		else if ((c & 0xF8) == 0xF0) {
			i += 4;
			cnt++;
		}
		else {
			i += 1;
			cnt++;
		}
		if (cnt <= maxChars) {
			lastCharEnd = i;
		}
	}
	if (lastCharEnd < str.size()) {
		str.resize(lastCharEnd);
	}
}

void LiveRoom::Render(bool full) {
	if (!m_roomInfo.liveStatus) {
		ImGui::Begin("主播当前未开播哦", &m_open, m_windowFlags);
		ImVec2 videoSize = ImGui::GetContentRegionAvail();
		ImVec2 videoMin = ImGui::GetCursorScreenPos();
		auto dl = ImGui::GetWindowDrawList();
		auto font = ImGui::GetFont();

		float baseSize = ImGui::GetFontSize(); 
		static constexpr const auto text = "主播当前未开播哦";
		const auto size = font->CalcTextSizeA(baseSize, FLT_MAX, 0.0f, text);
		auto oldScale = font->Scale;
		auto scale = (videoSize.x * 0.6f) / size.x;
		auto pos = ImVec2(videoMin.x + videoSize.x * 0.2f, videoMin.y + (videoSize.y - size.y * scale) * 0.5f);
		dl->AddText(font, baseSize * scale, pos, IM_COL32(255, 255, 255, 255), text);
		ImGui::End();
		return;
	}
	else if (m_layerName.empty()) {
		return;
	}
	m_inputRender = false;
	auto focus = ImGui::Begin(m_layerName.c_str(), &m_open, m_windowFlags);
	auto id = boost::lexical_cast<std::string>(uuid);
	ImGui::PushID(id.c_str());
	focus |= !ImGui::IsWindowCollapsed();
	ImVec2 videoMin = ImGui::GetCursorScreenPos();
	ImVec2 videoSize = ImGui::GetContentRegionAvail();
	ImVec2 videoMax(videoMin.x + videoSize.x, videoMin.y + videoSize.y);

	auto io = ImGui::GetIO();
	if (m_inputDispearTime > 0) {
		m_inputDispearTime -= io.DeltaTime;
	}
	bool mouseInVideo = ImGui::IsWindowHovered();
	bool mouseMoved = (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f);

	if (mouseInVideo && mouseMoved || m_uiActiveLastFrame) {
		if (m_inputDispearTime <= 0) {
			m_inputDispearTime = 2;
		}
		m_uiActiveLastFrame = false;
	}

	if (!focus) {
		RenderDanmaku(videoSize, focus);
		ImGui::End();
		return;
	}
	ImGui::Image((ImTextureID)(m_texture), videoSize);
	RenderDanmaku(videoSize, focus);

	m_inputRender = (mouseInVideo && m_inputDispearTime > 0);

	if (m_inputRender) {

		RenderPlayerUI(videoMin, videoMax,full);

	}
	else {
		m_uiActiveLastFrame = false;
	}
	ImGui::PopID();
	ImGui::End();

}

LiveRoom::LiveRoom(uint64_t roomID)
	:m_running(true)
	,m_roomID(roomID)
	,m_texture(-1)
	,m_inputRender(false)
	,m_uiActiveLastFrame(false)
	,m_danmakuEnabled(true)
	,m_inputDispearTime(0)
	,m_danmaActive(false)
	,m_renewDanmuku(false)
	,m_renewCount(0)
	,RenderLayer()
{
	m_bliveClient = std::make_shared<BliveClient>(roomID);
	m_playController = std::make_shared<PlaybackController>();
	m_danmu.reserve(256);
	m_windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}

void LiveRoom::Start()
{
	m_bliveClient->SetRoomInfo(m_roomInfo);
	if (m_roomInfo.liveStatus == false) {
		m_running.store(false, std::memory_order_relaxed);
		SetLayerName("主播当前未开播哦");
		return;
	}
	m_danmuKu.SetAddOneFunc([self = shared_from_this()](const std::string& str) {
		self->m_bliveClient->SendDanmu(str);
		});

	auto urls = m_bliveClient->GetLiveUrls();
	auto danmuKuUrls = m_bliveClient->GetDanmakuUrls();
	auto token = m_bliveClient->GetToken();
	SetLayerName(m_roomInfo.roomTitle);
	m_danmakuClient = std::make_shared<DanmakuClient>(m_roomInfo.realRoomID, danmuKuUrls[0], token, m_danmuKu, m_roomState);
	m_renewThread = std::thread([this]() {
		auto danmuKuUrls = m_bliveClient->GetDanmakuUrls();
		int max = danmuKuUrls.size();
		m_renewCount = 1;
		if (m_renewCount >= max) {
			return;
		}
		while (m_running.load(std::memory_order_relaxed) == true) {
			while (m_renewDanmuku.load(std::memory_order_relaxed) == false) {
				m_renewDanmuku.wait(false, std::memory_order_relaxed);
			}
			m_danmakuClient->SetUrls(danmuKuUrls[m_renewCount]);

			m_renewDanmuku.store(false, std::memory_order_relaxed);
			m_danmakuClient->Run();
			m_renewCount++;

			if (m_renewCount >= max) {
				m_danmakuClient->Stop();
				LOG_ERROR("[LiveRoom] Failed to connect to danmaku servers,total try {} times", max);
				return;
			}
		}
		});
	auto DanmakuRetry = [self = shared_from_this()]() {
		self->m_renewDanmuku.store(true, std::memory_order_relaxed);
		self->m_renewDanmuku.notify_all();
		};
	m_danmakuClient->SetRetryCallBack(DanmakuRetry);

	m_renewThread.detach();


	m_danmakuClient->Run();
	auto url = urls.cbegin();
	while (!m_playController->Connect(*url)) {
		url++;
		if (url == urls.end()) {
			LOG_ERROR("[PlaybackController] Failed connect to live server");
			break;
		}
	}

	auto stopCb = [self = shared_from_this()](void*) {
		std::call_once(self->m_stopFlag, [=]() {
			//Event* e = new RoomCloseEvent(self->uuid);
			//EventDispatcher::Dispatch(e);
			});
		};
	m_playController->SetStopCallback(stopCb);
	m_playController->Start();
	m_bliveClient->PreDanmuThread();
	
	m_volumeWidget.volume = m_playController->GetVolume();
	LOG_DEBUG("[LiveRoom] Video fps is {}", m_playController->GetFps());
}

void LiveRoom::Stop()
{
	m_running.store(false, std::memory_order_relaxed);
	m_playController->Stop();
	m_bliveClient->Stop();
	m_danmakuClient->Stop();
}

void LiveRoom::OnAttach()
{
}

void LiveRoom::OnUpdate()
{
	if (m_running.load(std::memory_order_relaxed) == false) {
		return;
	}
	m_texture = m_playController->GetTexture();
}

void LiveRoom::OnImGuiRender(bool fullScreen)
{
	if ((m_texture == 0 || m_texture == -1) && m_running.load(std::memory_order_relaxed) == true) {
		return;
	}
	if (m_open) {
		Render(fullScreen);
	}
	else {
		m_running.store(false, std::memory_order_relaxed);
		Event* e = new RoomCloseEvent(GetUUID());
		EventDispatcher::Dispatch(e);
	}
}

void LiveRoom::OnDetach()
{
}

LiveRoom::~LiveRoom()
{
	LOG_INFO("[RoomExit] LiveRoom deconstruction");
}
