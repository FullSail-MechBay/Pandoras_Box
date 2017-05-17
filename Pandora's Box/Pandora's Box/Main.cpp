#include <array>
#include <future>
#include <chrono>
#include <iostream>
#include <mutex>
#include "UDPClient.h"
#include "DirtRallyPacketStructure.h"
#include "PlatformDataManager.h"


int main()
{
	//Receiving Buffers
	std::array<char32_t, (DirtRally::UDPPacketNoExtra::GetStructSizeinByte() >> 2)> receivingbuffer{ 0 };
	std::array<char32_t, (DataManager::MBC_Header::GetStructSizeinByte() >> 2)> statusbuffer{ 0 };


	//TODO: Reading from .ini
	UDPClient client("10.20.24.139", 10991);
	std::chrono::high_resolution_clock timer;
	DataManager::PlatformDataManager datamanager;
	std::mutex mut;
	uint64_t packetSent = 0;
	bool ongoingBufferChanged = false;
	bool receivingBufferChanged = false;
	bool statusBufferChanged = false;

	bool networkSwitch = true;
	bool programRun = true;


	//Platform related data
	const char* platformBufferPtr = reinterpret_cast<const char *>(datamanager.GetDataBufferAddress());
	size_t platformBufferSize = datamanager.GetDataSize();


	auto SendingToPlatform = [&]()
	{
		while (programRun)
		{
			if (networkSwitch)
			{
				if (ongoingBufferChanged)
				{
					if (mut.try_lock())
					{
						ongoingBufferChanged = false;
						auto result = client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
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
	};

	auto StatusFromPlatform = [&]()
	{
		while (programRun)
		{
			if (networkSwitch)
			{
				if (-1 != client.ReceiveFromRemote(reinterpret_cast<char *>(statusbuffer.data()), statusbuffer.size() * sizeof(statusbuffer[0])))
				{
					if (mut.try_lock())
					{
						statusBufferChanged = true;
						mut.unlock();
					}
				}
			}
		}
	};


	auto DataFromDirt = [&]()
	{
		while (programRun)
		{
			if (networkSwitch)
			{
				if (-1 != client.ReceiveFromLocal(reinterpret_cast<char *>(receivingbuffer.data()), receivingbuffer.size() * sizeof(receivingbuffer[0])))
				{
					if (mut.try_lock())
					{
						receivingBufferChanged = true;
						mut.unlock();
					}
				}
			}
		}
	};

	auto parseDirtPacket = [&]()
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

	std::array<std::shared_future<void>, 3> asyncTasks;
	enum eTASK_NAME
	{
		RecvGameData = 0,
		RecvStatus,
		Send,
	};
	asyncTasks[RecvGameData] = std::async(std::launch::async, DataFromDirt);
	asyncTasks[RecvStatus] = std::async(std::launch::async, StatusFromPlatform);
	asyncTasks[Send] = std::async(std::launch::async, SendingToPlatform);




	auto programBeginTime = timer.now();
	while (programRun)
	{
		if (GetAsyncKeyState('P'))
		{
			mut.lock();
			networkSwitch = !networkSwitch;
			mut.unlock();
		}

		if (GetAsyncKeyState(VK_ESCAPE))
		{
			mut.lock();
			programRun = !programRun;
			mut.unlock();
		}

		static auto startTimeStamp = timer.now();

		auto curTimeStamp = timer.now();
		if (curTimeStamp >= startTimeStamp + std::chrono::microseconds(495))
		{
			mut.lock();
			datamanager.SetDataMode(DataManager::DataMode_DOF);
			datamanager.SetDofData(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			datamanager.IncrimentPacketSequence();
			datamanager.SyncBufferChanges();
			platformBufferSize = datamanager.GetDataSize();
			ongoingBufferChanged = true;
			mut.unlock();
			startTimeStamp = curTimeStamp;
		}

		parseDirtPacket();


	}




	//Finishing up
	auto timelapsed = (double)std::chrono::duration_cast<std::chrono::seconds>(timer.now() - programBeginTime).count();
	std::cout << "Packet Sent Rate: " << (double)packetSent / timelapsed << "\n";

	//Join all other threads
	for (auto& task : asyncTasks)
	{
		task.get();
	}

	return 0;

}