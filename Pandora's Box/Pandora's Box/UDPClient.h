#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdint.h>
#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")

class UDPClient
{
public:
	UDPClient(const char*  targetIP, int32_t targetPort);
	size_t Send(const char* buffer, size_t bufferSize);
	~UDPClient();
private:
	SOCKET receivingSocket;
	SOCKET outgoingSocket;
	sockaddr_in outgoingAddress;

};

