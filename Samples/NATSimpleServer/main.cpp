#include "RakPeerInterface.h"
#include "RakPeer.h"
#include "RakSleep.h"
#include "NatPunchthroughServer.h"
#include "MessageIdentifiers.h"

#if defined(_WIN32)
#include "gettimeofday.h"
#include <Windows.h>
#else
#include <sys/time.h>
#endif

using namespace RakNet;

const unsigned short MAX_CONNECTIONS = 65535;

FILE* g_logFile;

void Log(const char * format, ...)
{
	char buffer[1024];

	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	printf(buffer);
	if (g_logFile)
	{
		fputs(buffer, g_logFile);
		fflush(g_logFile);
	}
}

#define STARTUP_RESULT_CASE(X) case X: return #X; break;
const char* ToString(StartupResult result)
{
	switch (result)
	{
		STARTUP_RESULT_CASE(RAKNET_STARTED);
		STARTUP_RESULT_CASE(RAKNET_ALREADY_STARTED);
		STARTUP_RESULT_CASE(INVALID_SOCKET_DESCRIPTORS);
		STARTUP_RESULT_CASE(INVALID_MAX_CONNECTIONS);
		STARTUP_RESULT_CASE(SOCKET_FAMILY_NOT_SUPPORTED);
		STARTUP_RESULT_CASE(SOCKET_PORT_ALREADY_IN_USE);
		STARTUP_RESULT_CASE(SOCKET_FAILED_TO_BIND);
		STARTUP_RESULT_CASE(SOCKET_FAILED_TEST_SEND);
		STARTUP_RESULT_CASE(PORT_CANNOT_BE_ZERO);
		STARTUP_RESULT_CASE(FAILED_TO_CREATE_NETWORK_THREAD);
		STARTUP_RESULT_CASE(COULD_NOT_GENERATE_GUID);
		STARTUP_RESULT_CASE(STARTUP_OTHER_FAILURE);
	}

	return "(unknown)";
}

struct NatPunchthroughServerDebugInterface_SimpleServer : public RakNet::NatPunchthroughServerDebugInterface
{
	virtual void OnServerMessage(const char *msg)
	{
		printf("%s\n", msg);
	}
};

//A structure for holding running calculations that we can memset to 0 after each Log 
struct Stats
{
	int messages;
	int loopCount;
	int msTaken;
	int minMsTaken;
	int maxMsTaken;

	int newConnectionCount;
	int disconnectionCount;
	int connectionLostCount;

	uint64_t bytesSent;
	uint64_t bytesReceived;

	unsigned int messagesInSendBuffer[NUMBER_OF_PRIORITIES];
	double bytesInSendBuffer[NUMBER_OF_PRIORITIES];
};

const int SLEEP_DURATION = 30;

int main(int argc, char *argv[])
{
	unsigned short port = 60000;
	int loopsPerLog = -1;

	for (int i = 0; i < argc; ++i)
	{
		if (!strcmp(argv[i], "-port") && i < (argc - 1))
		{
			port = (unsigned short)atoi(argv[i + 1]);
		}
		else if (!strcmp(argv[i], "-logFile") && i < (argc - 1))
		{
			char* logFilename = argv[i + 1];

			if (logFilename)
			{
				g_logFile = fopen(logFilename, "a");
				if (g_logFile)
				{
					fputs("\n", g_logFile);
					fflush(g_logFile);
				}

				//RakNet::OpenLogFile(logFilename);
			}
		}
		else if (!strcmp(argv[i], "-statPeriod") && i < (argc - 1))
		{
			int logPeriod = (unsigned short)atoi(argv[i + 1]);
			loopsPerLog = logPeriod / SLEEP_DURATION;
		}
	}

	RakNet::RakPeerInterface *peer = RakNet::RakPeerInterface::GetInstance();
	peer->SetMaximumIncomingConnections(MAX_CONNECTIONS);

	RakNet::NatPunchthroughServer serverPlugin;
	//NatPunchthroughServerDebugInterface_SimpleServer debugInterface;
	//serverPlugin.SetDebugInterface(&debugInterface);

	peer->AttachPlugin(&serverPlugin);

	RakNet::SocketDescriptor socketDescriptor(port, 0);
	RakNet::StartupResult result = peer->Startup(MAX_CONNECTIONS, &socketDescriptor, 1);

	Log("RakPeer startup %s\n\n", ToString(result));

	if (result != RAKNET_STARTED)
		return -1;
		
	Stats stats;
	memset(&stats, 0, sizeof(Stats));
	
	//Two arrays for tracking addresses/stats so we always the previous frame's stats available
	bool statsFlipFlop = true;
	SystemAddress* addresses[2];
	RakNetStatistics* statistics[2];
	if (loopsPerLog != -1)
	{
		addresses[0] = new SystemAddress[MAX_CONNECTIONS];
		addresses[1] = new SystemAddress[MAX_CONNECTIONS];
		statistics[0] = new RakNetStatistics[MAX_CONNECTIONS];
		statistics[1] = new RakNetStatistics[MAX_CONNECTIONS];
	}

	int lastNumConnections = 0;

	while (true)
	{
		if (loopsPerLog == -1)
		{
			//The bare minimum RakNet needs a client update to do
			for (RakNet::Packet *p = peer->Receive();
				p != NULL;
				peer->DeallocatePacket(p), p = peer->Receive())
			{
				//printf("Packet %d %s\n", p->data[0], p->systemAddress.ToString());
			}
		}
		else
		{
			struct timeval before, after;

			gettimeofday(&before, NULL);
			stats.messages += peer->GetReceiveBufferSize();

			for (RakNet::Packet *p = peer->Receive();
				p != NULL;
				peer->DeallocatePacket(p), p = peer->Receive())
			{
				switch (p->data[0])
				{
				case ID_NEW_INCOMING_CONNECTION:
					stats.newConnectionCount++;
					break;
				case ID_DISCONNECTION_NOTIFICATION:
					stats.disconnectionCount++;
					break;
				case ID_CONNECTION_LOST:
					stats.connectionLostCount++;
					break;
				}
			}

			gettimeofday(&after, NULL);

			long msTaken = ((after.tv_sec - before.tv_sec) * 1000)
				+
				((after.tv_usec - before.tv_usec) / 1000);

			stats.msTaken += msTaken;
			if (msTaken > stats.maxMsTaken)
				stats.maxMsTaken = msTaken;
			if (msTaken < stats.minMsTaken)
				stats.minMsTaken = msTaken;

			stats.loopCount++;

			SystemAddress* lastAddresses = addresses[statsFlipFlop];
			RakNetStatistics* lastStats = statistics[statsFlipFlop];
			statsFlipFlop = !statsFlipFlop;
			SystemAddress* thisAddresses = addresses[statsFlipFlop];
			RakNetStatistics* thisStats = statistics[statsFlipFlop];

			int numConnections = peer->GetStatisticsArray(thisAddresses, 0, thisStats, MAX_CONNECTIONS);

			for (int i = 0; i < numConnections; ++i)
			{
				RakNetStatistics* thisConnectionStats = &(thisStats[i]);

				stats.bytesSent += thisConnectionStats->runningTotal[ACTUAL_BYTES_SENT];
				stats.bytesReceived += thisConnectionStats->runningTotal[ACTUAL_BYTES_RECEIVED];

				for (int j = 0; j < lastNumConnections; ++j)
				{
					for (int j = 0; j < NUMBER_OF_PRIORITIES; ++j)
						stats.messagesInSendBuffer[j] += thisConnectionStats->messageInSendBuffer[j];

					for (int j = 0; j < NUMBER_OF_PRIORITIES; ++j)
						stats.bytesInSendBuffer[j] += thisConnectionStats->bytesInSendBuffer[j];

					if (thisAddresses[i] == lastAddresses[j]
						&&
						thisConnectionStats->connectionStartTime == lastStats[i].connectionStartTime)
					{
						stats.bytesSent -= lastStats[j].runningTotal[ACTUAL_BYTES_SENT];
						stats.bytesReceived -= lastStats[j].runningTotal[ACTUAL_BYTES_RECEIVED];

						break;
					}
				}
			}

			lastNumConnections = numConnections;

			if (stats.loopCount >= loopsPerLog)
			{
				Log("Connections %d (New %d Disconnects %d Lost %d)\n"
					"Messages %d. Time: Avg %d Min %d Max %d\n"
					"Bytes Sent %llu Received %llu\n"
					"Messages pending %u %u %u %u\n"
					"Bytes pending %u %u %u %u\n"
					"\n",
					numConnections, stats.newConnectionCount, stats.disconnectionCount, stats.connectionLostCount,
					stats.messages, stats.msTaken / stats.loopCount, stats.minMsTaken, stats.maxMsTaken,
					stats.bytesSent, stats.bytesReceived,
					stats.messagesInSendBuffer[LOW_PRIORITY], stats.messagesInSendBuffer[MEDIUM_PRIORITY], stats.messagesInSendBuffer[HIGH_PRIORITY], stats.messagesInSendBuffer[IMMEDIATE_PRIORITY],
					(unsigned int)stats.bytesInSendBuffer[LOW_PRIORITY], (unsigned int)stats.bytesInSendBuffer[MEDIUM_PRIORITY], (unsigned int)stats.bytesInSendBuffer[HIGH_PRIORITY], (unsigned int)stats.bytesInSendBuffer[IMMEDIATE_PRIORITY]
				);

				memset(&stats, 0, sizeof(Stats));
				stats.minMsTaken = 999999;
			}
		}

		// Keep raknet threads responsive
		RakSleep(SLEEP_DURATION);
	}
}