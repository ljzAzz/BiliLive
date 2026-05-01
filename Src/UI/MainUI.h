#pragma once
#include "RenderLayer.h"
#include <Event/Event.h>
#include <boost/uuid/uuid_hash.hpp>

class MenuLayer :public RenderLayer {
private:
	bool m_searchOpen;
	bool m_loginOpen;
	bool m_settingOpen;
	std::shared_ptr<class RenderLayer> m_pLoginLayer;
	std::shared_ptr<class RenderLayer> m_pSearchLayer;
	std::shared_ptr<class RenderLayer> m_pSettingLayer;
public:
	MenuLayer() 
		:RenderLayer("首页")
		,m_searchOpen(false)
		,m_loginOpen(false)
		,m_settingOpen(false)
	{}
	~MenuLayer() {}
	virtual void OnUpdate() override;
	virtual void OnAttach() override;
	virtual void OnImGuiRender(bool fullScreen = false) override;
	virtual void OnDetach() override;

};

class SearchLayer :public RenderLayer {
private:
	std::string m_inputBuffer;
public:
	SearchLayer() 
		:RenderLayer("搜索")
	{}
	~SearchLayer() 
	{}
	virtual void OnUpdate() override;
	virtual void OnAttach() override;
	virtual void OnImGuiRender(bool fullScreen = false) override;
	virtual void OnDetach() override;
	void Open() { m_open = true; }
};

class LoginLayer :public RenderLayer {
private:
	std::string m_sessData;
	std::string m_biliJct;
	int m_flag;
	float m_displayTime;
public:
	LoginLayer()
		:RenderLayer("登录")
		,m_flag(0)
		,m_displayTime(0)
	{
	}
	~LoginLayer()
	{
	}
	virtual void OnUpdate() override;
	virtual void OnAttach() override;
	virtual void OnImGuiRender(bool fullScreen = false) override;
	virtual void OnDetach() override;
	void Open() { m_open = true; }

};

class SettingLayer :public RenderLayer {
private:
	struct AppSetting& m_editSetting;
	bool m_editing;
private:
	void RenderWindowSetting(struct WindowSetting& setting);
	void RenderDanmakuSetting(struct DanmakuSetting& setting);
	void RenderDecoderSetting(struct PlayerSetting& setting);
	void RenderRendererSetting(struct RenderSetting& setting, const struct WindowSetting& windowSetting);

	void ApplySetting(const struct AppSetting& setting);
public:
	SettingLayer();
	~SettingLayer()
	{
	}
	virtual void OnUpdate() override;
	virtual void OnAttach() override;
	virtual void OnImGuiRender(bool fullScreen = false) override;
	virtual void OnDetach() override;
	void Open() { m_open = true; }

};

class MainUI {
private:
	static inline ImGuiID s_dockingID;
	static inline std::unordered_map<std::string, std::shared_ptr<class RenderLayer>> s_renderLayers;
	static inline bool s_buildDockingSpace;
	static inline std::shared_ptr<class RenderLayer> s_pMenuLayer;
private:
	static void BuildDockingSpace();
	static void RenderLayer(bool fullScreen);
	static bool RemoveAllLayer(Event* e);
	MainUI() = default;
	~MainUI() = default;

public:
	static void Init();
	static void Update();
	static void RenderUI(bool fullScreen = false);
	static void RemoveLayer(boost::uuids::uuid uuid);
	static void AddRenderLayer(std::shared_ptr<class RenderLayer> layer);
	static std::shared_ptr<class RenderLayer> GetRenderLayer(boost::uuids::uuid uuid);
};