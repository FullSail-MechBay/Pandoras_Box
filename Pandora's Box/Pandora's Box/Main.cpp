#include <array>
#include <future>
#include <chrono>
#include <iostream>
#include <mutex>
#include "UDPClient.h"
#include "DirtRallyPacketStructure.h"
#include "PlatformDataManager.h"



std::array<char32_t, 20> mainbuffer{ 0 };
std::array<char32_t, 20> sendingbuffer{ 0 };
std::array<char32_t, DirtRally::UDPPacketNoExtra::GetStructSizeinByte() / 4> receivingbuffer{ 0 };
std::array<char32_t, 256> statusbuffer{ 0 };


std::mutex mut;
bool mainBufferChanged = false;
bool receivingBufferChanged = false;
bool networkSwitch = true;
uint64_t packetSent = 0;
void SendingWorker(UDPClient& client)
{

	while (networkSwitch)
	{
		if (mainBufferChanged)
		{
			if (mut.try_lock())
			{
				mainBufferChanged = false;
				//Copy & Convert endianness 
				for (size_t i = 0; i < sendingbuffer.size(); i++)
				{
					sendingbuffer[i] = htonl(mainbuffer[i]);
				}

				auto result = client.Send(reinterpret_cast<char *>(sendingbuffer.data()), sendingbuffer.size() * sizeof(sendingbuffer[0]));
				if (result == SOCKET_ERROR)
				{
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
	while (networkSwitch)
	{
		if (-1 != client.ReceiveFromLocal(reinterpret_cast<char *>(receivingbuffer.data()), receivingbuffer.size() * sizeof(receivingbuffer[0])))
		{
			std::cout << "received!\n";
		}
	}
}

int main()
{
	//TODO: Reading from .ini
	UDPClient client("10.20.24.139", 10991);
	std::chrono::high_resolution_clock timer;

	auto ReceivingFromDirt = [&client]()
	{
		while (networkSwitch)
		{
			if (-1 != client.ReceiveFromLocal(reinterpret_cast<char *>(receivingbuffer.data()), receivingbuffer.size() * sizeof(receivingbuffer[0])))
			{
				if (mut.try_lock())
				{
					//std::cout << "received!\n";
					receivingBufferChanged = true;
					mut.unlock();
				}
			}
		}
	};

	auto parseDirtPacket = []()
	{
		if (receivingBufferChanged)
		{
			if (mut.try_lock())
			{
				receivingBufferChanged = false;
				//No need to convert endianness
				DirtRally::UDPPacketNoExtra packet;
				memcpy(&packet, receivingbuffer.data(), receivingbuffer.size() * sizeof(receivingbuffer[0]));
				// Converting to platform data struct
				mut.unlock();
			}
		}

	};


	std::shared_future<void> net_SendThread = std::async(std::launch::async, SendingWorker, std::ref(client));
	std::shared_future<void> net_RecvThread = std::async(std::launch::async, ReceivingFromDirt);

	bool run = true;
	auto programBeginTime = timer.now();
	while (run)
	{
		if (GetAsyncKeyState('P'))
		{
			networkSwitch = false;
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

		parseDirtPacket();


	}
	auto timelapsed = (double)std::chrono::duration_cast<std::chrono::seconds>(timer.now() - programBeginTime).count();
	std::cout << "Packet Sent Rate: " << (double)packetSent / timelapsed << "\n";
	net_SendThread.get();
	net_RecvThread.get();
	return 0;

}