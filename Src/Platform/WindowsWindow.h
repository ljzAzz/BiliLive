#pragma once
#include "Window.h"
#include <Event/Event.h>

class WindowsWinodw :public Window{
private:
	WNDCLASSEXW m_wc;
private:
	float m_scaleFactor;   //dpi scale factor
private:
	void EnableDpiAwareness();
	float GetMonitorDpiScale() const;
	std::pair<int, int> GetMonitorSize() const;
public:
	WindowsWinodw();
	virtual ~WindowsWinodw() override;
	virtual bool Resize(Event* e) override;
	virtual void Show() override;
	virtual void Destroy() override;
	virtual bool IsFullScreen() override;
	virtual void PollEvents() override;
	virtual bool MaximizeEvent(Event* e) override;
	virtual bool RestoreEvent(Event* e) override;
private:
	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

};