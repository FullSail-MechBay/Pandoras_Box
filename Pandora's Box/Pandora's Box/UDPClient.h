#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <cstdint>
#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")

class UDPClient
{
public:
	UDPClient(const char*  targetIP, int32_t targetPort);
	int Send(const char* buffer, size_t bufferSize);
	int ReceiveFromLocal(char* buffer, size_t bufferSize);
	int ReceiveFromRemote(char* buffer, size_t bufferSize);
	inline uint64_t PacketSent() const { return packetSent; }
	void Shutdown();
	~UDPClient();
private:
	SOCKET receivingSocket;
	SOCKET outgoingSocket;
	sockaddr_in outgoingAddress;
	sockaddr_in receivingAddress;
	uint64_t packetSent;
};

