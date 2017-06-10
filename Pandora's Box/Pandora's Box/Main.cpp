#include <array>
#include <future>
#include <chrono>
#include <ctime>
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
constexpr T map(T x, T in_min, T in_max, T out_min, T out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


//Helper constant expressions 
constexpr inline int GetWaitTimeinMills(int hz)
{
	return 1000 / hz;
}




inline double interpolate(double start, double end, double coefficient)
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
	const int GAME_DATATICKRATE = 1000;
	const int TICKRATE = 500;

	//Variables
	std::chrono::high_resolution_clock timer;
	std::shared_mutex mut;
	std::condition_variable_any cv;
	CSignalGenerator generator;
	generator.SetParameters(CSignalGenerator::eWAVEFORM_SINE, 0.5, 0.1, 0.0, GAME_DATATICKRATE);

	//Receiving Buffers
	std::array<uint8_t, Simtools::DOFPacket::GetStructSizeinByte()> incomingbuffer;
	std::array<Simtools::DOFPacket, TICKRATE> packetStack;
	ZeroMemory(packetStack.data(), packetStack.size() * Simtools::DOFPacket::GetStructSizeinByte());


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


	//Setting Up
	datamanager.SetDataMode(DEFAULT_DATA_MODE);




	//auto SinYaw = [&datamanager, &platformBufferSize, &generator, &timer]()
	//{

	//	float yaw = generator.GetSample();


	//	//yaw = map(static_cast<float>(yaw), -1.0f, 1.0f, -0.3f, 0.3f);
	//	//std::cout << "TimeStamp: " <<(timer.now().time_since_epoch().count()/1000000) << "\t" << yaw << "\n";
	//	datamanager.SetDofData(0.0f, 0.0f, yaw, 0.0f, 0.0f, -0.12f);
	//	datamanager.SyncBufferChanges();
	//	platformBufferSize = datamanager.GetDataSize();
	//	//std::this_thread::sleep_for(std::chrono::microseconds(500));
	//};

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
			datamanager.IncrimentPacketSequence();
			client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
			std::this_thread::sleep_for(std::chrono::microseconds(1));
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



	auto parseGameDataMotionCue = [&]()
	{
		if (incomingBufferChanged)
		{
			incomingBufferChanged = false;
			if (mut.try_lock())
			{
				//No need to convert endianness
				DirtRally::UDPPacketNoExtra packet;
				ZeroMemory(&packet, sizeof packet);
				memcpy(&packet, incomingbuffer.data(), sizeof(packet));
				// Converting to platform data struct
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
				/*datamanager.SetMotionCueData
				(	packet.m_roll, packet.m_pitch, packet.m_heading,
					packet.m_angvely, packet.m_angvelx, packet.m_angvelz,
					packet.m_accely, packet.m_accelx, packet.m_accelz,
					packet.m_vely, packet.m_velx, packet.m_velz
					);*/
					//const double PI = 3.1415926535897932384626433832795;
					//std::cout << 1.5708 - packet.m_roll << "\n";
				datamanager.SetDofData(1.5708 - packet.m_roll, 0.0f, 0.0f, 0.0f, 0.0f, -0.12f);
				//datamanager.SetDofData(0.0f*packet.m_roll, 0.0f* packet.m_pitch, 0.0f, 10.0f*36.0f * packet.m_z, 0.0f, -0.15f);
				//std::cout.precision(50);
				//std::cout << std::fixed << packet.m_z << "\n";
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
		//std::this_thread::sleep_for(std::chrono::milliseconds(GetWaitTimeinMills(GAME_DATATICKRATE)));
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
		static Simtools::DOFPacket lastpacket;
		Simtools::DOFPacket newPacket;
		ZeroMemory(&newPacket, Simtools::DOFPacket::GetStructSizeinByte());
		memcpy(&newPacket, incomingbuffer.data(), Simtools::DOFPacket::GetStructSizeinByte());
		newPacket.m_heave = ntohs(newPacket.m_heave);
		newPacket.m_pitch = ntohs(newPacket.m_pitch);
		newPacket.m_roll = ntohs(newPacket.m_roll);
		newPacket.m_surge = ntohs(newPacket.m_surge);
		newPacket.m_sway = ntohs(newPacket.m_sway);
		newPacket.m_yaw = ntohs(newPacket.m_yaw);

		//std::cout << packet.m_yaw << "\n";
		static auto Lastframe = timer.now();
		double deltaTime = (double)(timer.now() - Lastframe).count() / 1000000000.0*(double)TICKRATE;
		double co = 0.0f;

		for (auto& p : packetStack)
		{
			p.m_yaw = interpolate(static_cast<double>(lastpacket.m_yaw), static_cast<double>(newPacket.m_yaw), co);
			co += deltaTime;
		}

		static	bool test = true;
		if (test)
		{
			for (auto& p : packetStack)
			{
				std::cout << p.m_yaw << "\n";
			}
			std::cout << co << "\n";
			test = false;
		}


		/*datamanager.SetDofData(0.0f*map(static_cast<float>(packet.m_roll), 0.0f, 255.f, -15.0f, 15.f),
			0.0f* packet.m_pitch,
			map(static_cast<float>(packet.m_yaw), 0.0f, static_cast<float>(0xffff), -0.3f, 0.3f),
			0.0f*packet.m_surge,
			0.0f*packet.m_sway,
			-0.09f);*/
		datamanager.SyncBufferChanges();

		//std::cout << static_cast<float>(ntohs(packet.m_yaw)) << "\n";
		//std::cout << map(static_cast<float>(packet.m_yaw), 0.0f, static_cast<float>(0xffff), -0.3f, 0.3f) << "\n";
		platformBufferSize = datamanager.GetDataSize();


		Lastframe = timer.now();
		memcpy(&lastpacket, &newPacket, sizeof newPacket);
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

			//std::this_thread::sleep_for(std::chrono::milliseconds(GetWaitTimeinMills(GAME_DATATICKRATE)));
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
		shouldSend = true;
		cv.notify_all();
	};


	const uint8_t NUM_BUFFERED_KEYS = 4;
	bool wasKeyPressed[NUM_BUFFERED_KEYS]{ false };
	bool isKeyPressed[NUM_BUFFERED_KEYS]{ false };
	uint8_t i = 0;
	Reset();


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



	}



	//Shutdown block
	{
		//Shutdown Platform
		datamanager.SetCommandState(DataManager::CommandState_DISENGAGE);
		datamanager.IncrimentPacketSequence();
		datamanager.SyncBufferChanges();
		client.Send(reinterpret_cast<const char *>(platformBufferPtr), platformBufferSize);
		datamanager.SetCommandState(DataManager::CommandState_RESET);
		datamanager.IncrimentPacketSequence();
		datamanager.SyncBufferChanges();
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