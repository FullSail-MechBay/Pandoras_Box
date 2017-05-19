#include <array>
#include <future>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "UDPClient.h"
#include "MBCtoHostReader.h"
#include "DirtRallyPacketStructure.h"
#include "PlatformDataManager.h"




int main()
{
	//Receiving Buffers
	std::atomic<std::array<char, DirtRally::UDPPacketNoExtra::GetStructSizeinByte()>> incomingbuffer;



	//TODO: Reading from .ini
	//"192.168.21.3", 10991
	UDPClient client("192.168.21.3", 10991);
	std::chrono::high_resolution_clock timer;
	DataManager::PlatformDataManager datamanager;
	DataManager::MBCtoHostReader reader;
	//datamanager.SetDataMode(DataManager::DataMode_DOF);
	std::mutex mut;
	std::condition_variable cv;
	uint64_t packetSent = 0;
	std::atomic<bool> incomingBufferChanged = false;
	std::atomic<bool> statusBufferChanged = false;

	std::atomic<bool> networkSwitch = true;
	std::atomic<bool> isProgramRunning = true;


	std::atomic<bool> shouldSend = false;
	std::atomic<bool> shouldRecvGameData = false;
	std::atomic<bool> shouldRecvStatus = false;
	auto programBeginTime = timer.now();

	//Platform related data
	const char* platformBufferPtr = reinterpret_cast<const char *>(datamanager.GetDataBufferAddress());
	size_t platformBufferSize = datamanager.GetDataSize();




	auto SendingToPlatform = [&]()
	{


		using namespace std::chrono_literals;
		auto delay = 450us;
		while (isProgramRunning)
		{
			std::unique_lock<std::mutex> lk(mut);
			cv.wait(lk, [&] {return shouldSend.load(); });
			lk.unlock();

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
						if (result != SOCKET_ERROR)
						{
							++packetSent;
						}
						mut.unlock();
					}
					lastTimeStamp += delay;
				}

				auto timelapsed = (double)std::chrono::duration_cast<std::chrono::seconds>(timer.now() - programBeginTime).count();
				auto sendingRate = (double)packetSent / timelapsed;
				if (sendingRate <= 2200)
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


				std::ostringstream os;
				os << "\rPacket Sent Rate: " << sendingRate;
				std::cout << os.str();
			}

		}
	};

	auto StatusFromPlatform = [&]()
	{

		while (isProgramRunning)
		{
			std::unique_lock<std::mutex> lk(mut);
			cv.wait(lk, [&] {return shouldRecvStatus.load(); });
			lk.unlock();

			if (networkSwitch)
			{
				if (-1 != client.ReceiveFromRemote(reinterpret_cast<char *>(reader.GetBufferAdder()), reader.ReaderDataBufferSize))
				{
					statusBufferChanged = true;
				}
			}
		}

	};

	auto DataFromDirt = [&]()
	{
		while (isProgramRunning)
		{
			std::unique_lock<std::mutex> lk(mut);
			cv.wait(lk, [&] {return shouldRecvGameData.load(); });
			lk.unlock();

			if (networkSwitch)
			{
				if (-1 != client.ReceiveFromLocal(reinterpret_cast<char *>(incomingbuffer.load().data()), incomingbuffer.load().size()))
				{
					incomingBufferChanged = true;
				}
			}
		}
	};

	auto parseDirtPacket = [&]()
	{
		if (incomingBufferChanged)
		{
			incomingBufferChanged = false;
			if (mut.try_lock())
			{
				//No need to convert endianness
				DirtRally::UDPPacketNoExtra packet;
				memcpy(&packet, incomingbuffer.load().data(), sizeof(packet));
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


	auto PlatformBootUp = [&]()
	{
		//Disable spamming thread
		std::cout << "Paused\n";
		shouldSend = false;
		//Send two reset packets with the same sequence number
		datamanager.SetCommandState(DataManager::CommandState_RESET);
		datamanager.SyncBufferChanges();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		datamanager.IncrimentPacketSequence();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		//Send one engage packet
		datamanager.SetCommandState(DataManager::CommandState_ENGAGE);
		datamanager.SetPacketSequence(1);
		datamanager.SyncBufferChanges();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		datamanager.IncrimentPacketSequence();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		datamanager.SetCommandState(DataManager::CommandState_NO_CHANGE);
		datamanager.SyncBufferChanges();
		datamanager.IncrimentPacketSequence();



		shouldSend = true;
		cv.notify_all();

		std::cout << "Resumed\n";

	};


	programBeginTime = timer.now();
	bool wasKeyPressed[5]{ false };
	bool isKeyPressed[5]{ false };

	PlatformBootUp();
	while (isProgramRunning)
	{

		if (GetAsyncKeyState(VK_ESCAPE))
		{
			isProgramRunning = !isProgramRunning;
			//std::cout << "Program Ends!\n";
			break;
		}

		uint8_t index = 0;

		isKeyPressed[index] = GetAsyncKeyState('G');
		if (isKeyPressed[index] && !wasKeyPressed[index])
		{
			PlatformBootUp();
		}
		wasKeyPressed[index] = isKeyPressed[index];
		++index;


		isKeyPressed[index] = GetAsyncKeyState('P');
		if (isKeyPressed[index] && !wasKeyPressed[index])
		{
			networkSwitch = !networkSwitch;
			std::cout << (networkSwitch ? "\nNetwork On!\n" : "\nNetwork Off!\n");
		}
		wasKeyPressed[index] = isKeyPressed[index];
		++index;


		isKeyPressed[index] = GetAsyncKeyState('L');
		if (isKeyPressed[index] && !wasKeyPressed[index])
		{
			shouldSend = !shouldSend;
			cv.notify_all();
		}
		wasKeyPressed[index] = isKeyPressed[index];
		++index;

		isKeyPressed[index] = GetAsyncKeyState('K');
		if (isKeyPressed[index] && !wasKeyPressed[index])
		{
			shouldRecvGameData = !shouldRecvGameData;
			cv.notify_all();
		}
		wasKeyPressed[index] = isKeyPressed[index];
		++index;

		isKeyPressed[index] = GetAsyncKeyState('J');
		if (isKeyPressed[index] && !wasKeyPressed[index])
		{
			shouldRecvStatus = !shouldRecvStatus;
			cv.notify_all();
		}
		wasKeyPressed[index] = isKeyPressed[index];

		parseDirtPacket();
	}



	//Shutdown block
	{
		//Shutdown Platform
		datamanager.SetCommandState(DataManager::CommandState_DISENGAGE);
		datamanager.IncrimentPacketSequence();
		datamanager.SyncBufferChanges();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		datamanager.IncrimentPacketSequence();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		datamanager.SetCommandState(DataManager::CommandState_RESET);
		datamanager.IncrimentPacketSequence();
		datamanager.SyncBufferChanges();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		datamanager.IncrimentPacketSequence();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);


		//Shutdown sockets
		client.Shutdown();
		//Join all other threads
		for (auto& task : asyncTasks)
		{
			task.get();
		}
	}
	return 0;

}