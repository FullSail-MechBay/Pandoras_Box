#include <vector>
#include <future>
#include <chrono>
#include <iostream>
#include <mutex>
#include "UDPClient.h"

std::vector<char32_t> mainbuffer(20, 0);
std::vector<char32_t> sendingbuffer(20, 0);
std::mutex mut;
bool mainBufferChanged = false;
bool sending = true;

uint64_t packetSent = 0;
void SendingWorker()
{
	UDPClient client("192.168.1.3", 10991);
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
				client.Send((char *)sendingbuffer.data(), sendingbuffer.size() * sizeof(char));
				++packetSent;

				mut.unlock();
			}
		}
		
	}
}

int main()
{
	std::shared_future<void> net_thread = std::async(std::launch::async, SendingWorker);
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
	net_thread.get();

	return 0;

}