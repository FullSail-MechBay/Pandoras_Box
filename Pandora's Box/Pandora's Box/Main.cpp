#include <vector>
#include <future>
#include <chrono>
#include <iostream>
#include <mutex>
#include "UDPClient.h"

std::vector<char32_t> mainbuffer(20, 0);
std::vector<char32_t> sendingbuffer(20, 0);
std::vector<char32_t> statusbuffer(256, 0);
std::mutex mut;
bool mainBufferChanged = false;
bool sending = true;

uint64_t packetSent = 0;
void SendingWorker(UDPClient& client)
{

	while (sending)
	{
		if (mainBufferChanged)
		{
			if (mut.try_lock())
			{
				mainBufferChanged = false;
				for (size_t i = 0; i < sendingbuffer.size(); i++)
				{
					sendingbuffer[i] = htonl(mainbuffer[i]);
				}

				auto iResult = client.Send((char *)sendingbuffer.data(), sendingbuffer.size() * sizeof(sendingbuffer[0]));
				if (iResult == SOCKET_ERROR) {
					wprintf(L"sendto failed with error: %d\n", WSAGetLastError());
				}
				++packetSent;

				mut.unlock();
			}
		}

	}
}

void ReceivingWorker(UDPClient& client)
{
	while (sending)
	{
		if (-1 != client.ReceiveFromLocal((char *)statusbuffer.data(), statusbuffer.size() * sizeof(statusbuffer[0])))
		{
			std::cout << "received!\n";
		}

	}
}

int main()
{
	UDPClient client("10.20.24.139", 10991);
	std::shared_future<void> net_SendThread = std::async(std::launch::async, SendingWorker, std::ref(client));
	std::shared_future<void> net_RecvThread = std::async(std::launch::async, ReceivingWorker, std::ref(client));
	bool run = true;
	std::chrono::high_resolution_clock timer;
	auto programBeginTime = timer.now();
	while (run)
	{
		if (GetAsyncKeyState('P'))
		{
			sending = false;
		}

		if (GetAsyncKeyState(VK_ESCAPE))
		{
			run = false;
		}

		static auto startTimeStamp = timer.now();

		auto curTimeStamp = timer.now();
		if (curTimeStamp >= startTimeStamp + std::chrono::microseconds(495))
		{
			static char dummyDataSegement = 0;
			mut.lock();
			for (auto& i : mainbuffer)
			{
				i = dummyDataSegement;
			}
			mainBufferChanged = true;
			mut.unlock();
			dummyDataSegement++;
			startTimeStamp = curTimeStamp;
		}







	}
	auto timelsped = (double)std::chrono::duration_cast<std::chrono::seconds>(timer.now() - programBeginTime).count();
	std::cout << "Packet Sent Rate: " << (double)packetSent / timelsped << "\n";
	net_SendThread.get();
	net_RecvThread.get();

	return 0;

}