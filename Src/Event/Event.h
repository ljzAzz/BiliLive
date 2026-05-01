#pragma once
#include <boost/uuid.hpp>
#include "Common/Common.h"

enum class EventType {
	ApplicationClose,
	RoomOpen,RoomClose, RoomRemove, AllRoomClose,
	WindowResize,WindowMaximize,WindowRestore,WindowAdjust
};

class Event {
private:
	EventType m_eventType;
	bool m_handle;
public:
	Event(EventType e) :m_eventType(e), m_handle(false){}
	virtual ~Event() = default;
	bool IsHandle() const { return m_handle; }
	void SetHandle() { m_handle = true; }
	virtual EventType GetEventType() const { return m_eventType; }
};


class ApplicationCloseEvent:public Event {
private:
public:
	ApplicationCloseEvent() :Event(EventType::ApplicationClose) {}
	~ApplicationCloseEvent() {}
};

class RoomOpenEvent :public Event {
private:
	uint64_t m_roomID;
public:
	RoomOpenEvent(uint64_t rid) :Event(EventType::RoomOpen), m_roomID(rid) {}
	~RoomOpenEvent() {}
	uint64_t GetRoomID() const { return m_roomID; }
};

class RoomCloseEvent :public Event {
private:
	boost::uuids::uuid m_uuid;
public:
	RoomCloseEvent(boost::uuids::uuid rid) :Event(EventType::RoomClose), m_uuid(rid) {}
	~RoomCloseEvent() {}
	boost::uuids::uuid GetUUID() const { return m_uuid; }
};

class RoomRemoveEvent :public Event {
private:
	boost::uuids::uuid m_uuid;
public:
	RoomRemoveEvent(boost::uuids::uuid rid) :Event(EventType::RoomRemove), m_uuid(rid) {}
	~RoomRemoveEvent() {}
	boost::uuids::uuid GetUUID() const { return m_uuid; }
};

class WindowResizeEvent :public Event {
private:
	uint32_t m_width;
	uint32_t m_height;
public:
	WindowResizeEvent(uint32_t w, uint32_t h) :Event(EventType::WindowResize), m_width(w), m_height(h) {}
	~WindowResizeEvent() {}
	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
};

class WindowAdjustEvent :public Event {
private:
	uint32_t m_width;
	uint32_t m_height;
public:
	WindowAdjustEvent(uint32_t w, uint32_t h) :Event(EventType::WindowAdjust), m_width(w), m_height(h) {}
	~WindowAdjustEvent() {}
	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
};


class AllRoomCloseEvent :public Event {
private:
public:
	AllRoomCloseEvent() :Event(EventType::AllRoomClose) {}
	~AllRoomCloseEvent() {}
};

class WindowMaximizeEvent :public Event {
private:
public:
	WindowMaximizeEvent() :Event(EventType::WindowMaximize) {}
	~WindowMaximizeEvent() {}
};

class WindowRestoreEvent :public Event {
private:
public:
	WindowRestoreEvent() :Event(EventType::WindowRestore) {}
	~WindowRestoreEvent() {}
};

class EventDispatcher {
	friend class App;
	using EventFn = std::function<bool(Event*)>;
private:
	static inline std::unordered_multimap<EventType, EventFn> s_eventHandles;
	static inline LockFreeQueue<Event*> s_events = LockFreeQueue<Event*>(100); //100 should be enough

private:
	static void Dispatch();
public:
	EventDispatcher();
	~EventDispatcher();
	template<EventType E>
	static void RegisterEventHandle(EventFn f) {
		s_eventHandles.insert({ E,f });
	}
	static void Init(){}
	static void Dispatch(Event* event) {
		s_events.Push(event);
	}
};