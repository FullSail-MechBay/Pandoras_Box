#include "UDPClient.h"




UDPClient::UDPClient(const char*  targetIP, int32_t targetPort)
{
	int ret = 0;
	WSADATA data;
	ret = WSAStartup(WINSOCK_VERSION, &data);

	outgoingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	/*Switch this to Platform Address*/
	outgoingAddress.sin_family = AF_INET;
	outgoingAddress.sin_port = ntohs(targetPort);
	outgoingAddress.sin_addr.S_un.S_addr = inet_addr(targetIP);

}
size_t UDPClient::Send(const char* buffer, size_t bufferSize)
{
	return sendto(outgoingSocket, buffer, bufferSize, 0, (sockaddr*)&outgoingAddress, sizeof(sockaddr_in));
}

UDPClient::~UDPClient()
{
	WSACleanup();
}
