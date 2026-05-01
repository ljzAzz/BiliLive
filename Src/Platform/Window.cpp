#include "stdafx.h"
#include "Window.h"
#include "WindowsWindow.h"

Window::Window()
	:m_windowHandle(-1)
{
}

void Window::CreateWindowInstance()
{
	static std::once_flag flag;
#ifdef __WINDOWS__
	std::call_once(flag, [=]() {
		auto windowsWindow = new WindowsWinodw();
		s_pWindowInstance = std::unique_ptr<Window>(windowsWindow);
		});
#endif

}
