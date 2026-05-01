#include "stdafx.h"
#include "ImGuiRenderer.h"
#include "MainUI.h"
#include <Common/IconsFontAwesome6.h>
#include "Room/RoomSession.h"
#include "Event/Event.h"
#include "App/Application.h"

void MainUI::BuildDockingSpace()
{
	ImGui::DockBuilderRemoveNode(s_dockingID);
	ImGui::DockBuilderAddNode(s_dockingID, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(s_dockingID, ImGui::GetMainViewport()->Size);
	for (const auto& [uuid ,layer] : s_renderLayers) {
		ImGui::DockBuilderDockWindow(layer->GetLayerName().c_str(), s_dockingID);
	}
	ImGui::DockBuilderFinish(s_dockingID);

}

void MainUI::RenderLayer(bool fullScreen)
{
	for (auto& layer : s_renderLayers) {
		layer.second->OnImGuiRender(fullScreen);
	}
}

bool MainUI::RemoveAllLayer(Event* e)
{
	for (auto& [uuid, layer] : s_renderLayers) {
		layer = nullptr;
	}
	return true;
}


void MainUI::Init()
{
	s_buildDockingSpace = true;
	s_dockingID = 0;
	s_pMenuLayer = std::shared_ptr<class RenderLayer>(new MenuLayer);
	s_renderLayers.insert({ boost::uuids::to_string(s_pMenuLayer->GetUUID()),s_pMenuLayer});
	EventDispatcher::RegisterEventHandle<EventType::AllRoomClose>(MainUI::RemoveAllLayer);
}

void MainUI::Update()
{
	for (const auto& layer : s_renderLayers) {
		layer.second->OnUpdate();
	}
}

void MainUI::RenderUI(bool fullScreen)
{

	static ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
	windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
	windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	static ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("MainUI", nullptr, windowFlags);
	ImGui::PopStyleVar(3);

	s_dockingID = ImGui::GetID("MainUI");
	ImGui::DockSpace(s_dockingID);
	if (s_buildDockingSpace) {
		BuildDockingSpace();
		s_buildDockingSpace = false;
	}

	RenderLayer(fullScreen);
	ImGui::End();
}

void MainUI::RemoveLayer(boost::uuids::uuid uuid)
{
	if (s_renderLayers.find(boost::uuids::to_string(uuid)) != s_renderLayers.end()) {
		s_renderLayers.erase(boost::uuids::to_string(uuid));
		return;
	}
	return;
}

void MainUI::AddRenderLayer(std::shared_ptr<class RenderLayer> layer)
{
	auto pos = s_renderLayers.size();
	s_renderLayers.insert({ boost::uuids::to_string(layer->GetUUID()),layer });
	s_buildDockingSpace = true;
	return;
}

std::shared_ptr<class RenderLayer> MainUI::GetRenderLayer(boost::uuids::uuid uuid)
{
	auto it = s_renderLayers.find(boost::uuids::to_string(uuid));
	if (it != s_renderLayers.end()) {
		return it->second;
	}
	LOG_DEBUG("[MainUI] Get Layer key: {}", boost::uuids::to_string(uuid));
	assert(false && "Never should be happened");
	return nullptr;
}

void MenuLayer::OnUpdate()
{
	if (m_pSearchLayer!=nullptr&&!m_pSearchLayer->IsOpen()) {
		m_searchOpen = false;
	}
	if (m_pLoginLayer != nullptr && !m_pLoginLayer->IsOpen()) {
		m_loginOpen = false;
	}
	if (m_pSettingLayer != nullptr && !m_pSettingLayer->IsOpen()) {
		m_settingOpen = false;
	}
}

void MenuLayer::OnAttach()
{
}

void MenuLayer::OnImGuiRender(bool fullScreen)
{
	auto canSee = ImGui::Begin("首页");
	if (!canSee) {
		ImGui::End();
		return;
	}
	auto size = ImGui::GetContentRegionAvail();
	std::string login;
	if (!User::IsLogin()) {
		login = "登录";
	}
	else {
		login = "退出登录";
	}
	auto fontSize1 = ImGui::CalcTextSize(login.c_str());
	auto fontSize2 = ImGui::CalcTextSize("设置");
	auto fontSize3 = ImGui::CalcTextSize("搜索");
	auto startPos = ImVec2((size.x - fontSize1.x) / 2, (size.y - fontSize1.y - fontSize2.y - fontSize3.y) / 2);
	ImGui::SetCursorPos(startPos);
	if (ImGui::Button(login.c_str())) {
		if (login == "退出登录") {
			User::clear();
			User::Save();
		}
		else {
			if (!m_loginOpen) {
				if (m_pLoginLayer == nullptr) {
					m_pLoginLayer = std::shared_ptr<class RenderLayer>(new LoginLayer);
					MainUI::AddRenderLayer(m_pLoginLayer);
				}
				m_loginOpen = true;
				static_cast<LoginLayer*>(m_pLoginLayer.get())->Open();
			}
			else {
				ImGui::SetWindowFocus("登录");
			}
		}
	}
	ImGui::SetCursorPosX((size.x- fontSize2.x)/2);
	if (ImGui::Button("设置")) {
		if (!m_settingOpen) {
			if (m_pSettingLayer == nullptr) {
				m_pSettingLayer = std::shared_ptr<class RenderLayer>(new SettingLayer);
				MainUI::AddRenderLayer(m_pSettingLayer);
			}
			m_settingOpen = true;
			static_cast<SettingLayer*>(m_pSettingLayer.get())->Open();
		}
		else {
			ImGui::SetWindowFocus("设置");
		}
	}
	ImGui::SetCursorPosX((size.x - fontSize3.x) / 2);
	if (ImGui::Button("搜索")) {
		if (!m_searchOpen) {
			if (m_pSearchLayer == nullptr) {
				m_pSearchLayer = std::shared_ptr<class RenderLayer>(new SearchLayer);
				MainUI::AddRenderLayer(m_pSearchLayer);
			}
			m_searchOpen = true;
			static_cast<SearchLayer*>(m_pSearchLayer.get())->Open();
		}
		else {
			ImGui::SetWindowFocus("搜索");
		}
	}
	ImGui::End();
}

void MenuLayer::OnDetach()
{
}

void SearchLayer::OnUpdate()
{
}

void SearchLayer::OnAttach()
{
}

void SearchLayer::OnImGuiRender(bool fullScreen)
{
	if (m_open){
		auto canSee =ImGui::Begin("搜索", &m_open);
		if (!canSee) {
			ImGui::End();
			return;
		}
		auto size = ImGui::GetWindowSize();
		auto pos = ImGui::GetWindowPos();
		auto fsize_1 = ImGui::CalcTextSize("输入房间号...");
		auto fsize_2 = ImGui::CalcTextSize(ICON_FA_MAGNIFYING_GLASS);
		auto fsize_3 = ImGui::CalcTextSize("打开");
		float avail_width = ImGui::GetContentRegionAvail().x;
		float input_width = avail_width * 0.7f;

		auto fsize = ImVec2(fsize_1.x + input_width, std::max({ fsize_1.y + fsize_3.y, fsize_2.y + fsize_3.y }));
		ImGui::SetCursorPos(ImVec2((size.x - fsize.x) / 2, (size.y - fsize.y) / 2));
		ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), ICON_FA_MAGNIFYING_GLASS);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(input_width);
		ImGui::InputTextWithHint("##searchRoom", "输入房间号...", &m_inputBuffer);
		ImGui::SetCursorPosX((avail_width - fsize_3.x) / 2);
		if (ImGui::Button("打开")) {
			if (!m_inputBuffer.empty()) {
				Event* event = new RoomOpenEvent(std::stoull(m_inputBuffer));
				EventDispatcher::Dispatch(event);
				m_inputBuffer.clear();
			}
		}
		ImGui::End();
	}
}

void SearchLayer::OnDetach()
{
}

void LoginLayer::OnUpdate()
{
}

void LoginLayer::OnAttach()
{
}

void LoginLayer::OnImGuiRender(bool fullScreen)
{
	if (m_open) {
		auto canSee = ImGui::Begin("登录", &m_open);
		if (!canSee) {
			ImGui::End();
			return;
		}
		auto size = ImGui::GetWindowSize();
		auto pos = ImGui::GetWindowPos();
		static auto io = ImGui::GetIO();
		static auto fsize_1 = ImGui::CalcTextSize("输入SESSDATA...");
		static auto fsize_2 = ImGui::CalcTextSize("登录");
		static auto fsize_3 = ImGui::CalcTextSize("登录失败，请检查SESSDATA是否正确或重试");
		static auto fsize_4 = ImGui::CalcTextSize("登录成功");
		static auto fsize_5 = ImGui::CalcTextSize("输入bili_jct(可选。如果需要发送弹幕，此项必填)...");

		float avail_width = ImGui::GetContentRegionAvail().x;
		float input_width = avail_width * 0.7f;
		if (m_displayTime >0 && m_displayTime > io.DeltaTime) {
			m_displayTime -= io.DeltaTime;
		}
		else {
			if (User::IsLogin()) {
				m_open = false;
			}
			m_displayTime = 0;
			m_flag = 0;
		}
		auto fsize = m_flag ? ImVec2(input_width, fsize_1.y + fsize_2.y) : ImVec2(input_width, fsize_1.y + fsize_2.y + fsize_3.y + fsize_5.y);
		ImGui::SetCursorPos(ImVec2((size.x - fsize.x) / 2, (size.y - fsize.y) / 2));
		ImGui::SetNextItemWidth(input_width);
		auto end = false;
		if (m_flag == 2 || m_flag == 3) {
			ImGui::BeginDisabled(true);
			m_flag = 3;
		}
		ImGui::InputTextWithHint("##login", "输入SESSDATA...", &m_sessData);
		ImGui::SetCursorPosX((size.x - input_width) / 2);
		ImGui::SetNextItemWidth(input_width);
		ImGui::InputTextWithHint("##bilijct", "输入bili_jct(可选。如果需要发送弹幕，此项必填)...", &m_biliJct);

		if (m_flag != 0) {
			auto str = m_flag == 1 ? "登录失败，请检查SESSDATA是否正确或重试" : "登录成功";
			auto ssize = m_flag == 1 ? fsize_3 : fsize_4;
			auto color = m_flag == 1 ? ImVec4(1.0f, 0, 0, 1.0f) : ImVec4(0.0f, 1.0f, 0, 1.0f);
			ImGui::SetCursorPosX((avail_width - ssize.x) / 2);
			ImGui::TextColored(color, str);
		}
		ImGui::SetCursorPosX((avail_width - fsize_2.x) / 2);

		if (ImGui::Button("登录")) {
			if (!m_sessData.empty()) {
				User::SetSessData(m_sessData);
				UserInfo::UpdateAll();
				if (!User::IsLogin()) {
					m_flag = 1;
				}
				else {
					m_flag = 2;
				}
				m_displayTime = 3.0f;
				m_sessData.clear();
				if (!m_biliJct.empty()) {
					User::SetBilijct(m_biliJct);
					m_biliJct.clear();
				}
			}
		}
		if (m_flag==3) {
			ImGui::EndDisabled();
		}
		ImGui::End();
	}
}

void LoginLayer::OnDetach()
{
}

void SettingLayer::RenderWindowSetting(WindowSetting& setting)
{
	struct ResolutionOption
	{
		const char* label;
		int width;
		int height;
	};

	static constexpr ResolutionOption resolutions[] = {
		{ "1080 x 720", 1080, 720 },
		{ "1920 x 1080", 1920, 1080 },
	};

	int currentResolution = 0;

	for (int i = 0; i < _countof(resolutions); ++i) {
		if (setting.width == resolutions[i].width &&
			setting.height == resolutions[i].height) {
			currentResolution = i;
			break;
		}
	}

	static constexpr const char* resolutionLabels[] = {
		"1080 x 720",
		"1920 x 1080",
	};

	if (ImGui::Combo("窗口分辨率", &currentResolution, resolutionLabels, IM_ARRAYSIZE(resolutionLabels))) {
		setting.width = resolutions[currentResolution].width;
		setting.height = resolutions[currentResolution].height;
		Event* e = new WindowAdjustEvent(setting.width, setting.height);
		EventDispatcher::Dispatch(e);
	}
	ImGui::Checkbox("垂直同步", &setting.vsync);

	ImGui::Spacing();
	ImGui::Text("当前窗口设置:");
	ImGui::BulletText("宽度: %d", setting.width);
	ImGui::BulletText("高度: %d", setting.height);
	ImGui::BulletText("垂直同步: %s", setting.vsync ? "开启" : "关闭");
}

void SettingLayer::RenderDanmakuSetting(DanmakuSetting& setting)
{
	static float fontSize = setting.fontSize.load(std::memory_order_relaxed);
	if (ImGui::SliderFloat("字体大小", &fontSize, 20.0f, 80.0f, "%.1f")) {
		setting.fontSize.store(fontSize, std::memory_order_relaxed);
	}

	static constexpr const char* areaItems[] = {
		"全屏区域",
		"3/4 区域",
		"1/2 区域",
		"1/4 区域",
	};

	static int areaIndex = setting.areaIndex.load(std::memory_order_relaxed);
	if (ImGui::Combo("弹幕区域", &areaIndex, areaItems, IM_ARRAYSIZE(areaItems))) {
		setting.areaIndex.store(areaIndex, std::memory_order_relaxed);
		setting.danmaArea.store(DanmakuSetting::arr[areaIndex], std::memory_order_relaxed);
	}

	static float alpha = static_cast<float>(setting.alpha.load(std::memory_order_relaxed))/255.f;
	if (ImGui::SliderFloat("弹幕透明度", &alpha, 0.5f, 1.0f, "%.2f")) {
		setting.alpha.store(static_cast<int>(floor(alpha * 255.f)), std::memory_order_relaxed);
	}

	ImGui::Spacing();
	ImGui::Text("弹幕预览:");
	ImGui::BulletText("字体大小: %.1f", fontSize);
	ImGui::BulletText("区域: %s", areaItems[areaIndex]);
	ImGui::BulletText("透明度: %.2f", alpha);
}

void SettingLayer::RenderDecoderSetting(PlayerSetting& setting)
{
	static constexpr const char* audioApiItems[] = {
		"WASAPI",
	};

	ImGui::BeginDisabled(true);
	if (ImGui::Combo("Audio Player API", &setting.audioApiIndex, audioApiItems, IM_ARRAYSIZE(audioApiItems))){
		setting.ap=static_cast<AudioOuput>(setting.audioApiIndex);
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::TextDisabled("目前仅支持 WASAPI");

	static constexpr const char* videoApiItems[] = {
		"DX12 Video Processor",
		"DX12 Shader",
	};

	if (ImGui::Combo("Video Decoder API", &setting.videoApiIndex, videoApiItems, IM_ARRAYSIZE(videoApiItems))) {
		setting.vd = static_cast<VideoDecoder>(setting.videoApiIndex);
	}

	const char* threadItems[] = {
		"1",
		"2",
		"4",
	};
	static int threadNum = 0;
	switch (setting.vThreadNum) {
	case 1:
		threadNum = 0;
		break;
	case 2:
		threadNum = 1;
		break;
	case 4:
		threadNum = 2;
		break;
	}

	if (ImGui::Combo("视频解码线程数量", &threadNum, threadItems, IM_ARRAYSIZE(threadItems))) {
		setting.vThreadNum = threadNum;
	}

	ImGui::Spacing();
	ImGui::Text("解码设置:");
	ImGui::BulletText("音频 API: %s", audioApiItems[setting.audioApiIndex]);
	ImGui::BulletText("视频 API: %s", videoApiItems[setting.videoApiIndex]);
	ImGui::BulletText("线程数量: %s", threadItems[threadNum]);
}

void SettingLayer::RenderRendererSetting(RenderSetting& setting, const WindowSetting& windowSetting)
{
	static constexpr const char* backendItems[] = {
		"Vulkan",
		"Dx12" ,
		"OpenGL"
	};

	ImGui::BeginDisabled(true);
	ImGui::Combo("后端渲染 API", &setting.backendApiIndex, backendItems, IM_ARRAYSIZE(backendItems));
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::TextDisabled("目前仅支持 DX12");

	const char* vsyncLevelItems[] = {
		"1",
		"2",
		"3",
	};

	ImGui::BeginDisabled(!windowSetting.vsync);
	if (ImGui::Combo("垂直同步 Level", &setting.vsyncLevelIndex, vsyncLevelItems, IM_ARRAYSIZE(vsyncLevelItems))) {
		setting.level = static_cast<SyncInterval>(setting.vsyncLevelIndex);
	}
	ImGui::EndDisabled();

	if (!windowSetting.vsync) {
		ImGui::TextDisabled("WindowSetting 中的垂直同步关闭后，此项不可设置");
	}

	ImGui::Spacing();
	ImGui::Text("渲染设置:");
	ImGui::BulletText("渲染 API: %s", backendItems[setting.backendApiIndex]);

	if (windowSetting.vsync) {
		ImGui::BulletText("VSync Level: %s", vsyncLevelItems[setting.vsyncLevelIndex]);
	}
	else {
		ImGui::BulletText("VSync Level: 已禁用");
	}
}

void SettingLayer::ApplySetting(const AppSetting& setting)
{
}

SettingLayer::SettingLayer()
	:RenderLayer("设置")
	,m_editing(false)
	,m_editSetting(App::GetSetting())
{
}

void SettingLayer::OnUpdate()
{
}

void SettingLayer::OnAttach()
{
}

void SettingLayer::OnImGuiRender(bool fullScreen)
{
	if (m_open) {
		bool canSee = ImGui::Begin("设置", &m_open);

		if (!canSee) {
			ImGui::End();
			return;
		}
		if (ImGui::BeginTabBar("SettingTabs")) {
			if (ImGui::BeginTabItem("窗口")) {
				RenderWindowSetting(m_editSetting.windowSetting);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("弹幕")) {
				RenderDanmakuSetting(m_editSetting.damukuSetting);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("解码")) {
				RenderDecoderSetting(m_editSetting.playerSetting);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("渲染")) {
				RenderRendererSetting(m_editSetting.renderSetting, m_editSetting.windowSetting);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		//ImGui::Separator();

		//if (ImGui::Button("应用")) {
		//	// TODO
		//	//m_setting = m_editSetting;
		//	//ApplySetting(m_setting);
		//	
		//}

		//ImGui::SameLine();

		//if (ImGui::Button("确定")) {
		//	//m_setting = m_editSetting;
		//	//ApplySetting(m_setting);
		//	//m_open = false;
		//	//m_editing = false;
		//}

		//ImGui::SameLine();

		//if (ImGui::Button("取消")) {
		//	m_editSetting = m_setting;
		//	m_open = false;
		//	m_editing = false;
		//}

		//ImGui::SameLine();

		//if (ImGui::Button("恢复默认")) {
		//	m_editSetting = AppSetting{};
		//}
		ImGui::End();
	}

	if (!m_open) {
		m_editing = false;
	}

}

void SettingLayer::OnDetach()
{
}
