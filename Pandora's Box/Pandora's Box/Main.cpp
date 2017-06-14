#include <array>
#include <future>
#include <chrono>
#include <iostream>
#include <atomic>
#include <condition_variable>
#include <shared_mutex>
#include <cmath>
#include "UDPClient.h"
#include "MBCtoHostReader.h"
#include "SimtoolsPacketStructure.h"
#include "DirtRallyPacketStructure.h"
#include "PlatformDataManager.h"
#include "SignalGenerator.hpp"


template <typename T>
constexpr  T map(T x, T in_min, T in_max, T out_min, T out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


//Helper constant expressions 
constexpr  int GetWaitTimeinMills(int hz)
{
	return 1000 / hz;
}




double interpolate(double start, double end, double coefficient)
{
	return start + coefficient * (end - start);
}




//TODO: Reading setting from file
//"192.168.21.3", 10991
int main()
{
	using ThreadSafeBool = std::atomic<bool>;


	//Constants
	const auto DEFAULT_DATA_MODE = DataManager::DataMode_DOF;
	const auto DEFAULT_REMOTE_IP = "192.168.21.3";
	const int DEFAULT_REMOTE_PORT = 10991;
	const uint8_t DATAOUTSTEP_ms = 2;

	//Variables
	std::chrono::high_resolution_clock hiZtimer;
	std::shared_mutex mut;
	std::condition_variable_any cv;


	//Receiving Buffers
	std::array<uint8_t, Simtools::DOFPacket::GetStructSizeinByte()> incomingbuffer;


	UDPClient client(DEFAULT_REMOTE_IP, DEFAULT_REMOTE_PORT);
	//Platform related variables
	DataManager::PlatformDataManager datamanager;
	DataManager::MBCtoHostReader reader;
	const char* platformBufferPtr = reinterpret_cast<const char *>(datamanager.GetDataBufferAddress());
	size_t platformBufferSize = datamanager.GetDataSize();

	ThreadSafeBool resetPlatform = false;
	ThreadSafeBool isProgramRunning = true;
	ThreadSafeBool incomingBufferChanged = false;
	ThreadSafeBool statusBufferChanged = false;
	ThreadSafeBool shouldSend = false;
	ThreadSafeBool shouldRecvGameData = false;
	ThreadSafeBool shouldRecvStatus = false;

	ThreadSafeBool axisControl[6]{ true,true,true,true,true,true };


	//Setting Up
	datamanager.SetDataMode(DEFAULT_DATA_MODE);
	datamanager.DataOutStep_ms = DATAOUTSTEP_ms;





	auto SendingToPlatform = [&]()
	{
		while (isProgramRunning)
		{
			{
				std::shared_lock<std::shared_mutex> lk(mut);
				cv.wait(lk, [&] {return shouldSend.load(); });
			}
			if (resetPlatform)
			{
				if (mut.try_lock_shared())
				{
					datamanager.SetCommandState(DataManager::CommandState_RESET);
					datamanager.SyncBufferChanges();
					client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
					datamanager.SetCommandState(DataManager::CommandState_ENGAGE);
					datamanager.SetPacketSequence(1);
					datamanager.SyncBufferChanges();
					client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
					datamanager.SetCommandState(DataManager::CommandState_NO_CHANGE);
					datamanager.SyncBufferChanges();
					resetPlatform = false;
					mut.unlock_shared();
				}

			}
			datamanager.IncDataFrame();
			datamanager.IncrimentPacketSequence();
			datamanager.SyncBufferChanges();

			client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
			std::this_thread::sleep_for(std::chrono::milliseconds(DATAOUTSTEP_ms));
		}
	};

	auto StatusFromPlatform = [&]()
	{

		while (isProgramRunning)
		{
			{
				std::shared_lock<std::shared_mutex> lk(mut);
				cv.wait(lk, [&] {return shouldRecvStatus.load(); });
			}
			if (-1 != client.ReceiveFromRemote(reinterpret_cast<char *>(reader.GetBufferAdder()), reader.ReaderDataBufferSize))
			{
				std::cout << "Received!";
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







	// Converting to platform data struct
	//	Motion Platform:
	//	X = Forward
	//	Y = Right Wing
	//	Z = Down
	//	Roll about X
	//	Pitch about Y
	//	Yaw about Z
	auto parseGameDataDOF = [&]()
	{
		std::unique_lock<std::shared_mutex> lk(mut);
		Simtools::DOFPacket packet;
		ZeroMemory(&packet, Simtools::DOFPacket::GetStructSizeinByte());
		memcpy(&packet, incomingbuffer.data(), Simtools::DOFPacket::GetStructSizeinByte());

		//for (auto& field : packet.m_array)
		//{
		//	field = ntohs(field);
		//}

		packet.m_roll = ntohs(packet.m_roll);
		packet.m_pitch = ntohs(packet.m_pitch);
		packet.m_yaw = ntohs(packet.m_yaw);
		packet.m_surge = ntohs(packet.m_surge);
		packet.m_sway = ntohs(packet.m_sway);
		packet.m_heave = ntohs(packet.m_heave);


		static auto Lastframe = hiZtimer.now();
		float deltaTime = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(hiZtimer.now() - Lastframe).count());


		static constexpr float RPY_LIMIT = 0.5f; //Roll Pitch Yaw Limits
		static constexpr float SURGE_SWAY_LIMIT = 0.4f;
		static constexpr float HEAVE_MIN = 0.05f;
		static constexpr float HEAVE_MAX = 0.45f;

		float roll = (axisControl[0] ? 1.0f : 0.0f) * map(static_cast<float>(packet.m_roll), 0.0f, static_cast<float>(0xffff), -RPY_LIMIT, RPY_LIMIT);
		float pitch = (axisControl[1] ? 1.0f : 0.0f) * map(static_cast<float>(packet.m_pitch), 0.0f, static_cast<float>(0xffff), -RPY_LIMIT, RPY_LIMIT);
		float yaw = (axisControl[2] ? 1.0f : 0.0f) * map(static_cast<float>(packet.m_yaw), 0.0f, static_cast<float>(0xffff), -RPY_LIMIT, RPY_LIMIT);
		float surge = (axisControl[3] ? 1.0f : 0.0f) * map(static_cast<float>(packet.m_surge), 0.0f, static_cast<float>(0xffff), -SURGE_SWAY_LIMIT, SURGE_SWAY_LIMIT);
		float sway = (axisControl[4] ? 1.0f : 0.0f) * map(static_cast<float>(packet.m_sway), 0.0f, static_cast<float>(0xffff), -SURGE_SWAY_LIMIT, SURGE_SWAY_LIMIT);
		float heave = -map(static_cast<float>(packet.m_heave), 0.0f, static_cast<float>(0xffff), HEAVE_MIN, HEAVE_MAX);
		axisControl[5] ? true : heave = -0.15f;
		datamanager.SetDofData(roll, pitch, yaw, surge, sway, heave, deltaTime);
		platformBufferSize = datamanager.GetDataSize();

		Lastframe = hiZtimer.now();

	};



	auto DataFromGame = [&]()
	{
		while (isProgramRunning)
		{
			{
				std::shared_lock<std::shared_mutex> lk(mut);
				cv.wait(lk, [&] {return shouldRecvGameData.load(); });
			}
			//static auto lastframeTP = timer.now();
			//static int64_t packGained = 0;
			if (-1 != client.ReceiveFromLocal(reinterpret_cast<char *>(incomingbuffer.data()), incomingbuffer.size()))
			{
				incomingBufferChanged = true;
				parseGameDataDOF();
				//packGained++;

				//auto curframeTP = std::chrono::duration_cast<std::chrono::seconds>(timer.now() - lastframeTP).count();
				//if (curframeTP >= 1)
				//{
				//	//std::cout << (float)packGained / (float)curframeTP << "\n";
				//	packGained = 0;
				//	lastframeTP = timer.now();
				//}
			}


		}
	};


	enum eTASK_NAME
	{
		Send = 0,
		RecvGameData,
		RecvStatus,
		TASK_COUNT
	};
	std::array<std::shared_future<void>, TASK_COUNT> asyncTasks;
	asyncTasks[RecvGameData] = std::async(std::launch::async, DataFromGame);
	asyncTasks[Send] = std::async(std::launch::async, SendingToPlatform);
	asyncTasks[RecvStatus] = std::async(std::launch::async, StatusFromPlatform);


	auto Reset = [&shouldSend, &resetPlatform, &cv]()
	{
		resetPlatform = true;
		cv.notify_all();
	};
	auto DisengagePlatform = [&datamanager, &client, &platformBufferPtr, &platformBufferSize]()
	{
		datamanager.SetCommandState(DataManager::CommandState_DISENGAGE);
		datamanager.IncrimentPacketSequence();
		datamanager.SyncBufferChanges();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		datamanager.SetCommandState(DataManager::CommandState_RESET);
		datamanager.IncrimentPacketSequence();
		datamanager.SyncBufferChanges();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
	};


	constexpr uint8_t NUM_BUFFERED_KEYS = 5 + 6;
	bool wasKeyPressed[NUM_BUFFERED_KEYS]{ false };
	bool isKeyPressed[NUM_BUFFERED_KEYS]{ false };
	uint8_t i = 0;


	std::cout
		<< "Press G to Reset Platform \n"
		<< "Press D to Disengage Platform \n"
		<< "Press H to toggle receiving motion Data \n"
		<< "Press J to toggle receiving status Data \n"
		<< "Press K to toggle sending motion Data \n";
	std::cout << "Receiving Motion Data: " << (shouldRecvGameData ? "On" : "Off") << "\n";
	std::cout << "Sending Motion Data: " << (shouldSend ? "On" : "Off") << "\n";
	std::cout << "Receiving Status Data: " << (shouldRecvStatus ? "On" : "Off") << "\n";
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
			Reset();
			std::cout << "Platform Reset command issued.\n";
		}
		wasKeyPressed[i] = isKeyPressed[i];
		++i;


		isKeyPressed[i] = GetAsyncKeyState('H');
		if (isKeyPressed[i] && !wasKeyPressed[i])
		{
			shouldRecvGameData = !shouldRecvGameData;
			std::cout << "Receiving Motion Data: " << (shouldRecvGameData ? "On" : "Off") << "\n";
			cv.notify_all();
		}
		wasKeyPressed[i] = isKeyPressed[i];
		i++;

		isKeyPressed[i] = GetAsyncKeyState('K');
		if (isKeyPressed[i] && !wasKeyPressed[i])
		{
			shouldSend = !shouldSend;
			std::cout << "Sending Motion Data: " << (shouldSend ? "On" : "Off") << "\n";
			cv.notify_all();
		}
		wasKeyPressed[i] = isKeyPressed[i];
		++i;

		isKeyPressed[i] = GetAsyncKeyState('J');
		if (isKeyPressed[i] && !wasKeyPressed[i])
		{
			shouldRecvStatus = !shouldRecvStatus;
			std::cout << "Receiving Status Data: " << (shouldRecvStatus ? "On" : "Off") << "\n";
			cv.notify_all();
		}
		wasKeyPressed[i] = isKeyPressed[i];
		i++;


		isKeyPressed[i] = GetAsyncKeyState('D');
		if (isKeyPressed[i] && !wasKeyPressed[i])
		{
			DisengagePlatform();
			std::cout << "Disengage Platform\n";
		}
		wasKeyPressed[i] = isKeyPressed[i];
		++i;

		//Axis controls
		for (char j = 0; j < 6; j++)
		{
			isKeyPressed[i] = GetAsyncKeyState(char(49 + j));
			if (isKeyPressed[i] && !wasKeyPressed[i])
			{
				axisControl[j] = !axisControl[j];
				std::cout << "Axis " << char(49 + j) << ": " << (axisControl[j] ? "On" : "Off") << "\n";
			}
			wasKeyPressed[i] = isKeyPressed[i];
			++i;
		}
		i = 0;
	}



	//Shutdown block
	{
		//Shutdown Platform
		DisengagePlatform();
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