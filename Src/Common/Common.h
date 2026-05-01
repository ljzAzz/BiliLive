#pragma once
//just for spsc
template<typename T>
class LockFreeQueue {
	static_assert(std::is_nothrow_copy_constructible_v<T> || std::is_nothrow_move_constructible_v<T>,
		"T must not throw in copy/move constructor for exception safety");
private:
	size_t m_queueSize;
	std::atomic_size_t m_fillSize;
	std::vector<T> m_queue;
	size_t m_headPos;
	alignas(64) std::array<char, 0> padding;
	size_t m_tailPos;
public:
	LockFreeQueue(size_t queueSize);
	~LockFreeQueue();
	void SetSize(size_t queueSize) { m_queueSize = queueSize; }
	bool Pop(T& data);
	bool Push(const T& data);
	size_t GetContentSize() const { return m_fillSize.load(std::memory_order_relaxed); }
	bool Empty() const { return m_fillSize.load(std::memory_order_relaxed) == 0; }
};

template<typename T>
inline LockFreeQueue<T>::LockFreeQueue(size_t queueSize)
	:m_queueSize(queueSize)
	, m_queue(queueSize)
	, m_fillSize(0)
	, m_headPos(0)
	, m_tailPos(0)
{

}

template<typename T>
inline LockFreeQueue<T>::~LockFreeQueue()
{
}

template<typename T>
inline bool LockFreeQueue<T>::Pop(T& data)
{
	if (m_fillSize.load(std::memory_order_acquire) < 0) {
		LOG_ERROR("[LockFreeQueue] Fatal error：The number of data items to fill the queue is less than 0");
		throw std::runtime_error("The number of LockFreeQueue's data items to fill the queue is less than 0");
	}
	else if (m_fillSize.load(std::memory_order_acquire) == 0) {
		return false;
	}
	else {
		auto pos = m_headPos;
		data = m_queue[pos];
		m_headPos = (pos + 1) % m_queueSize;
		m_fillSize.fetch_sub(1, std::memory_order_release);
		return true;
	}
}

template<typename T>
inline bool LockFreeQueue<T>::Push(const T& data)
{
	if (m_fillSize.load(std::memory_order_acquire) > m_queueSize) {
		LOG_ERROR("[LockFreeQueue] Fatal error：The number of data items to fill the queue is more than queue size");
		throw std::runtime_error("LockFreeQueue oversize");
	}
	else if (m_fillSize.load(std::memory_order_acquire) == m_queueSize) {
		return false;
	}
	else {
		auto pos = m_tailPos;
		m_tailPos = (pos + 1) % m_queueSize;
		m_queue[pos] = data;
		m_fillSize.fetch_add(1, std::memory_order_release);
		return true;
	}
}

std::string AcpToUtf8(const std::string& s);

std::string Utf16ToUtf8(const std::wstring& ws);

std::string winErrToUtf8(DWORD err);

std::filesystem::path GetExePath();