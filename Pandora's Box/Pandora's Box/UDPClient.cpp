#include "UDPClient.h"
#include <iostream>



UDPClient::UDPClient(const char*  targetIP, int32_t targetPort) : packetSent(0)
{
	int ret = 0;
	WSADATA data;
	ret = WSAStartup(WINSOCK_VERSION, &data);

	outgoingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	receivingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	/*Switch this to Platform Address*/
	outgoingAddress.sin_family = AF_INET;
	outgoingAddress.sin_port = ntohs(targetPort);
	outgoingAddress.sin_addr.S_un.S_addr = inet_addr(targetIP);

	receivingAddress.sin_family = AF_INET;
	receivingAddress.sin_port = ntohs(20777);
	receivingAddress.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	//Limit outgoingAddress to only receiving from this remoteHost
	//ret = connect(outgoingSocket, (SOCKADDR *)&outgoingAddress, sizeof(outgoingAddress));

	ret = bind(receivingSocket, (SOCKADDR *)& receivingAddress, sizeof(receivingAddress));
	//ret = connect(receivingSocket, (SOCKADDR *)&receivingAddress, sizeof(receivingAddress));

}
int UDPClient::Send(const char* buffer, size_t bufferSize)
{
	packetSent++;
	return sendto(outgoingSocket, buffer, static_cast<int>(bufferSize), 0, (sockaddr*)&outgoingAddress, sizeof(sockaddr_in));
}

int UDPClient::ReceiveFromRemote(char* buffer, size_t bufferSize)
{
	return recvfrom(outgoingSocket, buffer, static_cast<int>(bufferSize), 0, nullptr, nullptr);
}

int UDPClient::ReceiveFromLocal(char* buffer, size_t bufferSize)
{
	return recvfrom(receivingSocket, buffer, static_cast<int>(bufferSize), 0, nullptr, nullptr);
}

void UDPClient::Shutdown()
{
	shutdown(outgoingSocket, 2);
	shutdown(receivingSocket, 2);
	WSACleanup();
}

UDPClient::~UDPClient()
{

}
