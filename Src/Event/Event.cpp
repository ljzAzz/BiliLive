#include "stdafx.h"
#include "Event.h"

EventDispatcher::EventDispatcher()
{
}

EventDispatcher::~EventDispatcher()
{
}

void EventDispatcher::Dispatch()
{
	while (!s_events.Empty()) {
		Event* event = nullptr;
		if (!s_events.Pop(event)) {
			continue;
		}
		auto type = event->GetEventType();
		auto num = s_eventHandles.count(type);
		if (num != 0) {
			if (num == 1) {
				auto res = ((s_eventHandles.find(type)->second).operator())(event);
				if (res) {
					event->SetHandle();
				}
			}
			else {
				for (const auto& fun : s_eventHandles) {
					if (fun.first == type) {
						auto res = ((fun.second).operator())(event);
						if (res) {
							event->SetHandle();
							break;
						}
					}
				}
			}
			if (event) {
				delete event;
			}
		}
		else {
			LOG_WARN("[EventDispatcher] Event has not be handled");
		}
	}
}
