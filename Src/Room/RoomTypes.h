#include <cstdint>
#include <string>

struct RoomInfo {
	uint64_t realRoomID;
	uint64_t ownerUid;
	std::string ownerName;
	std::string roomTitle;
	bool liveStatus;
};

struct RoomState {
	std::atomic_int onlineCount;
};