#include <array>
#include <future>
#include <chrono>
#include <iostream>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cmath>
#include "UDPClient.h"
#include "MBCtoHostReader.h"
#include "DirtRallyPacketStructure.h"
#include "PlatformDataManager.h"


//Helper constant expressions 
constexpr int GetWaitTimeinMills(int hz)
{
	return 1000 / hz;
}


using ThreadSafeBool = std::atomic<bool>;

//TODO: Reading setting from file
//"192.168.21.3", 10991
int main()
{
	//Constants
	const auto DEFAULT_DATA_MODE = DataManager::DataMode_Motion;
	const auto DEFAULT_REMOTE_IP = "192.168.21.3";
	const int DEFAULT_REMOTE_PORT = 10991;
	const int GAME_DATATICKRATE = 60;

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


	ThreadSafeBool resetPlatform = true;
	ThreadSafeBool isProgramRunning = true;
	ThreadSafeBool incomingBufferChanged = false;
	ThreadSafeBool statusBufferChanged = false;
	ThreadSafeBool shouldSend = false;
	ThreadSafeBool shouldRecvGameData = false;
	ThreadSafeBool shouldRecvStatus = true;

	//Platform related data
	const char* platformBufferPtr = reinterpret_cast<const char *>(datamanager.GetDataBufferAddress());
	size_t platformBufferSize = datamanager.GetDataSize();



	auto SendingToPlatform = [&]()
	{
		while (isProgramRunning)
		{
			std::unique_lock<std::mutex> lk(mut);
			cv.wait(lk, [&] {return shouldSend.load(); });
			lk.unlock();

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
	};

	auto StatusFromPlatform = [&]()
	{

		while (isProgramRunning)
		{
			std::unique_lock<std::mutex> lk(mut);
			cv.wait(lk, [&] {return shouldRecvStatus.load(); });
			lk.unlock();
			if (-1 != client.ReceiveFromRemote(reinterpret_cast<char *>(reader.GetBufferAdder()), reader.ReaderDataBufferSize))
			{
				statusBufferChanged = true;
				auto response = reader.GetStatusResponse();
				static auto lastMachineState = response->GetMachineState();
				if (response->GetMachineState() != lastMachineState)
				{
					lastMachineState = response->GetMachineState();
					std::cout << "Current Machine State: " << DataManager::MachineStateString[lastMachineState] << "\n";
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
	};

	auto parseGameDataMotionCue = [&]()
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
				//	Dirt:Rally 
				//	X and Y axes are on the ground, Z is up
				//	Heading	anticlockwise from above (Z)
				//	Pitch	anticlockwise from right (X)
				//	Roll	anticlockwise from front (Y)
				//	Motion Platform:
				//	X = Forward
				//	Y = Right Wing
				//	Z = Down
				//	Roll about X
				//	Pitch about Y
				//	Yaw about Z
				datamanager.SetMotionCueData
				(	packet.m_roll, packet.m_pitch, packet.m_heading,
					packet.m_angvely, packet.m_angvelx, packet.m_angvelz,
					packet.m_accely, packet.m_accelx, packet.m_accelz,
					packet.m_vely, packet.m_velx, packet.m_velz
					);
				datamanager.SyncBufferChanges();
				platformBufferSize = datamanager.GetDataSize();
				mut.unlock();
			}
		}

		//{ // Testing 
		//	static float heave = -0.1f;
		//	static float step = 0.00002f;
		//	heave -= step;
		//	if (heave <= -0.3f || heave >= -0.1f)
		//	{
		//		step *= -1.0f;
		//	}

		//	//std::cout << heave << "\n";
		//	datamanager.SetDofData(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, heave);
		//	datamanager.SyncBufferChanges();
		//	platformBufferSize = datamanager.GetDataSize();
		//}
		std::this_thread::sleep_for(std::chrono::milliseconds(GetWaitTimeinMills(GAME_DATATICKRATE)));
	};


	enum eTASK_NAME
	{
		Send = 0,
		RecvGameData,
		RecvStatus,
		TASK_COUNT
	};
	std::array<std::shared_future<void>, TASK_COUNT> asyncTasks;
	asyncTasks[RecvGameData] = std::async(std::launch::async, DataFromDirt);
	asyncTasks[Send] = std::async(std::launch::async, SendingToPlatform);
	asyncTasks[RecvStatus] = std::async(std::launch::async, StatusFromPlatform);


	auto SignalPlatformToReset = [&shouldSend, &resetPlatform]()
	{
		resetPlatform = true;
		shouldSend = true;
	};


	const uint8_t NUM_BUFFERED_KEYS = 4;
	bool wasKeyPressed[NUM_BUFFERED_KEYS]{ false };
	bool isKeyPressed[NUM_BUFFERED_KEYS]{ false };
	uint8_t i = 0;
	SignalPlatformToReset();


	while (isProgramRunning)
	{

		if (GetAsyncKeyState(VK_ESCAPE))
		{
			isProgramRunning = !isProgramRunning;
			break;
		}


		isKeyPressed[i] = GetAsyncKeyState('G');
		if (isKeyPressed[i] && !wasKeyPressed[i])
		{
			SignalPlatformToReset();
		}
		wasKeyPressed[i] = isKeyPressed[i];
		++i;


		isKeyPressed[i] = GetAsyncKeyState('H');
		if (isKeyPressed[i] && !wasKeyPressed[i])
		{
			shouldRecvGameData = !shouldRecvGameData;
			cv.notify_all();
		}
		wasKeyPressed[i] = isKeyPressed[i];
		++i;

		isKeyPressed[i] = GetAsyncKeyState('K');
		if (isKeyPressed[i] && !wasKeyPressed[i])
		{
			shouldRecvGameData = !shouldRecvGameData;
			cv.notify_all();
		}
		wasKeyPressed[i] = isKeyPressed[i];
		++i;

		isKeyPressed[i] = GetAsyncKeyState('J');
		if (isKeyPressed[i] && !wasKeyPressed[i])
		{
			shouldRecvStatus = !shouldRecvStatus;
			cv.notify_all();
		}
		wasKeyPressed[i] = isKeyPressed[i];
		i = 0;

		parseGameDataMotionCue();
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