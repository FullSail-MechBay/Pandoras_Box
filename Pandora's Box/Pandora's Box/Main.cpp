#include <array>
#include <future>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <mutex>
#include "UDPClient.h"
#include "MBCtoHostReader.h"
#include "DirtRallyPacketStructure.h"
#include "PlatformDataManager.h"




int main()
{
	//Receiving Buffers
	std::array<char, DirtRally::UDPPacketNoExtra::GetStructSizeinByte()> incomingbuffer{ 0 };



	//TODO: Reading from .ini
	UDPClient client("10.20.24.139", 10991);
	std::chrono::high_resolution_clock timer;
	DataManager::PlatformDataManager datamanager;
	DataManager::MBCtoHostReader reader;
	datamanager.SetDataMode(DataManager::DataMode_DOF);
	std::mutex mut;
	uint64_t packetSent = 0;
	bool incomingBufferChanged = false;
	bool statusBufferChanged = false;

	bool networkSwitch = true;
	bool programRun = true;
	auto programBeginTime = timer.now();

	//Platform related data
	const char* platformBufferPtr = reinterpret_cast<const char *>(datamanager.GetDataBufferAddress());
	size_t platformBufferSize = datamanager.GetDataSize();


	//Platform boot up sequences
	//1. Sending engage message
	datamanager.SetCommandState(DataManager::CommandState_ENGAGE);
	datamanager.SyncBufferChanges();
	auto result = client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
	if (result == SOCKET_ERROR)
	{
		wprintf(L"sendto failed with error: %d\n", WSAGetLastError());
	}

	//2. Wait for Engaged message 
	while (-1 != client.ReceiveFromRemote(reinterpret_cast<char *>(reader.GetBufferAdder()), reader.ReaderDataBufferSize))
	{
		if (DataManager::MachineState::MS_Engaged == reader.GetStatusResponse()->GetMachineState())
		{
			break;
		}
	}



	auto SendingToPlatform = [&]()
	{
		using namespace std::chrono_literals;
		auto delay = 500us;
		while (programRun)
		{
			if (networkSwitch)
			{
				static auto lastTimeStamp = timer.now();

				if (timer.now() >= lastTimeStamp + delay)
				{
					if (mut.try_lock())
					{
						static bool samePacket = false;
						if (!samePacket)
						{
							datamanager.IncrimentPacketSequence();
							samePacket = true;
						}
						else
						{
							samePacket = false;
						}

						auto result = client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
						if (result == SOCKET_ERROR)
						{
							wprintf(L"sendto failed with error: %d\n", WSAGetLastError());
						}
						else
						{
							++packetSent;
						}
						mut.unlock();
					}
					lastTimeStamp += delay;
				}
				auto timelapsed = (double)std::chrono::duration_cast<std::chrono::seconds>(timer.now() - programBeginTime).count();
				auto sendingRate = (double)packetSent / timelapsed;
				std::cout << "\r  Packet Sent Rate: " << sendingRate;
				if (sendingRate <= 2000)
				{
					if (delay > 0us)
					{
						delay -= 10us;
					}
				}
				else
				{
					delay += 5us;
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
				if (-1 != client.ReceiveFromRemote(reinterpret_cast<char *>(reader.GetBufferAdder()), reader.ReaderDataBufferSize))
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
				if (-1 != client.ReceiveFromLocal(reinterpret_cast<char *>(incomingbuffer.data()), incomingbuffer.size()))
				{
					if (mut.try_lock())
					{
						incomingBufferChanged = true;
						mut.unlock();
					}
				}
			}
		}
	};

	auto parseDirtPacket = [&]()
	{
		if (incomingBufferChanged)
		{
			if (mut.try_lock())
			{
				incomingBufferChanged = false;
				//No need to convert endianness
				DirtRally::UDPPacketNoExtra packet;
				memcpy(&packet, incomingbuffer.data(), sizeof(packet));
				// Converting to platform data struct
				datamanager.SetCommandState(DataManager::CommandState_NO_CHANGE);

				datamanager.SetDofData(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
				datamanager.SyncBufferChanges();
				platformBufferSize = datamanager.GetDataSize();
				mut.unlock();

			}
		}

	};

	std::array<std::shared_future<void>, 3> asyncTasks;
	enum eTASK_NAME
	{
		RecvGameData = 0,
		Send,
		RecvStatus,
	};
	asyncTasks[RecvGameData] = std::async(std::launch::async, DataFromDirt);
	asyncTasks[Send] = std::async(std::launch::async, SendingToPlatform);
	asyncTasks[RecvStatus] = std::async(std::launch::async, StatusFromPlatform);




	programBeginTime = timer.now();
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



		parseDirtPacket();





	}




	//Finishing up
	/*auto timelapsed = (double)std::chrono::duration_cast<std::chrono::seconds>(timer.now() - programBeginTime).count();
	std::cout << "Packet Sent Rate: " << (double)packetSent / timelapsed << "\n";*/


	//Send disengage message to platform

	//shutdown sockets
	client.Shutdown();
	//Join all other threads
	for (auto& task : asyncTasks)
	{
		task.get();
	}

	return 0;

}