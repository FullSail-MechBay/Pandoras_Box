#include <array>
#include <future>
#include <chrono>
//#include <iostream>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cmath>
#include "UDPClient.h"
#include "MBCtoHostReader.h"
#include "DirtRallyPacketStructure.h"
#include "PlatformDataManager.h"



//TODO: Reading setting from file
//"192.168.21.3", 10991
int main()
{
	//Consts
	const auto DEFAULT_DATA_MODE = DataManager::DataMode_DOF;
	const auto DEFAULT_REMOTE_IP = "192.168.21.3";
	const auto DEFAULT_REMOTE_PORT = 10991;

	//Variables
	std::chrono::high_resolution_clock timer;
	std::mutex mut;
	std::condition_variable cv;
	//Receiving Buffers
	std::atomic<std::array<char, DirtRally::UDPPacketNoExtra::GetStructSizeinByte()>> incomingbuffer;



	UDPClient client(DEFAULT_REMOTE_IP, DEFAULT_REMOTE_PORT);
	DataManager::PlatformDataManager datamanager;
	DataManager::MBCtoHostReader reader;



	datamanager.SetDataMode(DEFAULT_DATA_MODE);

	std::atomic<bool> resetPlatform = true;
	std::atomic<bool> networkSwitch = true;
	std::atomic<bool> isProgramRunning = true;

	std::atomic<bool> incomingBufferChanged = false;
	std::atomic<bool> statusBufferChanged = false;
	std::atomic<bool> shouldSend = false;
	std::atomic<bool> shouldRecvGameData = false;
	std::atomic<bool> shouldRecvStatus = false;

	//Platform related data
	const char* platformBufferPtr = reinterpret_cast<const char *>(datamanager.GetDataBufferAddress());
	size_t platformBufferSize = datamanager.GetDataSize();



	auto SendingToPlatform = [&]()
	{
		while (isProgramRunning)
		{
			if (networkSwitch)
			{
				if (mut.try_lock())
				{

					if (resetPlatform)
					{
						datamanager.SetCommandState(DataManager::CommandState_RESET);
						datamanager.SyncBufferChanges();
						client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
						datamanager.IncrimentPacketSequence();
						client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
						datamanager.SetCommandState(DataManager::CommandState_ENGAGE);
						datamanager.SetPacketSequence(1);
						datamanager.SyncBufferChanges();
						client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
						datamanager.IncrimentPacketSequence();
						client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
						datamanager.SetCommandState(DataManager::CommandState_NO_CHANGE);
						datamanager.SyncBufferChanges();
						resetPlatform = false;
					}

					datamanager.IncrimentPacketSequence();
					client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
					datamanager.IncrimentPacketSequence();
					client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
					mut.unlock();
					std::this_thread::sleep_for(std::chrono::microseconds(1000000 / datamanager.GetCurrentDataRate()));
				}

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
			static auto lastframeTP = timer.now();
			static int64_t packGained = 0;
			if (networkSwitch)
			{
				if (-1 != client.ReceiveFromLocal(reinterpret_cast<char *>(incomingbuffer.load().data()), incomingbuffer.load().size()))
				{
					incomingBufferChanged = true;
					packGained++;

					auto curframeTP = std::chrono::duration_cast<std::chrono::seconds>(timer.now() - lastframeTP).count();
					if (curframeTP >= 1)
					{
						//std::cout << (float)packGained / (float)curframeTP << "\n";
						packGained = 0;
						lastframeTP = timer.now();
					}


				}
			}
		}
	};

	auto parseDirtPacket = [&]()
	{
		//if (incomingBufferChanged)
		//{
		//	incomingBufferChanged = false;
		//	if (mut.try_lock())
		//	{
		//		//No need to convert endianness
		//		DirtRally::UDPPacketNoExtra packet;
		//		memcpy(&packet, incomingbuffer.load().data(), sizeof(packet));
		//		// Converting to platform data struct
		//		datamanager.SetCommandState(DataManager::CommandState_NO_CHANGE);
		//		datamanager.SetDofData(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		//		datamanager.SyncBufferChanges();
		//		platformBufferSize = datamanager.GetDataSize();
		//		mut.unlock();
		//	}
		//}

		static float heave = -0.1f;
		static float step = 0.00002f;
		heave -= step;
		if (heave <= -0.3f || heave >= -0.1f)
		{
			step *= -1.0f;
		}

		//std::cout << heave << "\n";
		datamanager.SetDofData(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, heave);
		datamanager.SyncBufferChanges();
		platformBufferSize = datamanager.GetDataSize();

		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	};

	std::array<std::shared_future<void>, 3> asyncTasks;
	enum eTASK_NAME
	{
		Send = 0,
		RecvGameData,
		RecvStatus,
	};
	asyncTasks[RecvGameData] = std::async(std::launch::async, DataFromDirt);
	asyncTasks[Send] = std::async(std::launch::async, SendingToPlatform);
	asyncTasks[RecvStatus] = std::async(std::launch::async, StatusFromPlatform);


	auto SignalPlatformToReset = [&resetPlatform]()
	{
		resetPlatform = true;
	};


	bool wasKeyPressed[5]{ false };
	bool isKeyPressed[5]{ false };

	SignalPlatformToReset();


	while (isProgramRunning)
	{

		if (GetAsyncKeyState(VK_ESCAPE))
		{
			isProgramRunning = !isProgramRunning;
			break;
		}

		uint8_t index = 0;

		isKeyPressed[index] = GetAsyncKeyState('G');
		if (isKeyPressed[index] && !wasKeyPressed[index])
		{
			SignalPlatformToReset();
		}
		wasKeyPressed[index] = isKeyPressed[index];
		++index;


		isKeyPressed[index] = GetAsyncKeyState('P');
		if (isKeyPressed[index] && !wasKeyPressed[index])
		{
			networkSwitch = !networkSwitch;
			//std::cout << (networkSwitch ? "\nNetwork On!\n" : "\nNetwork Off!\n");
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
		isProgramRunning = false;
		for (auto& task : asyncTasks)
		{
			task.get();
		}
	}
	return 0;

}