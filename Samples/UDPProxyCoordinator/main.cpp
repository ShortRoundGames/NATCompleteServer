//Starts a RakNet peer on port 60000 and runs a punchthrough server. No extra fluff

#include "RakPeerInterface.h"
#include "RakPeer.h"
#include "RakSleep.h"
#include "UDPProxyCoordinator.h"
#include "UDPProxyServer.h"
#include "MessageIdentifiers.h"

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

#define STARTUP_ERROR_CASE(X) case X: Log(#X); break;

int main(int argc, char *argv[])
{
    char* coordinatorPassword = "balls";
    char* clientPassword = "";
    unsigned short port = 60000;
    char* logFilename = NULL;
    
    for (int i = 0; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-coordinatorPassword") && i < (argc - 1))
        {
            coordinatorPassword = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-clientPassword") && i < (argc - 1))
        {
            clientPassword = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-port") && i < (argc - 1))
        {
            port = (unsigned short)atoi(argv[i + 1]);
        }
        else if (!strcmp(argv[i], "-logFile") && i < (argc - 1))
        {
            logFilename = argv[i + 1];
        }
    }

    if (logFilename)
    {
        g_logFile = fopen(logFilename, "a");
        if (g_logFile)
        {
            fputs("\n", g_logFile);
            fflush(g_logFile);
        }

        RakNet::OpenLogFile(logFilename);
    }
        
	RakNet::RakPeerInterface *peer = RakNet::RakPeerInterface::GetInstance();
	peer->SetMaximumIncomingConnections(MAX_CONNECTIONS);

    RakNet::UDPProxyCoordinator* proxyCoordinator = new RakNet::UDPProxyCoordinator();
    proxyCoordinator->SetRemoteLoginPassword(coordinatorPassword);
    peer->AttachPlugin(proxyCoordinator);

    RakNet::SocketDescriptor socketDescriptor(port, 0);
    RakNet::StartupResult result = peer->Startup(MAX_CONNECTIONS, &socketDescriptor, 1);

    if (clientPassword[0])
        peer->SetIncomingPassword(clientPassword, strlen(clientPassword));

    Log("RakPeer startup %d\n", result);

    if (result != 0)
    {
        switch (result)
        {
            STARTUP_ERROR_CASE(RAKNET_ALREADY_STARTED);
            STARTUP_ERROR_CASE(INVALID_SOCKET_DESCRIPTORS);
            STARTUP_ERROR_CASE(INVALID_MAX_CONNECTIONS);
            STARTUP_ERROR_CASE(SOCKET_FAMILY_NOT_SUPPORTED);
            STARTUP_ERROR_CASE(SOCKET_PORT_ALREADY_IN_USE);
            STARTUP_ERROR_CASE(SOCKET_FAILED_TO_BIND);
            STARTUP_ERROR_CASE(SOCKET_FAILED_TEST_SEND);
            STARTUP_ERROR_CASE(PORT_CANNOT_BE_ZERO);
            STARTUP_ERROR_CASE(FAILED_TO_CREATE_NETWORK_THREAD);
            STARTUP_ERROR_CASE(COULD_NOT_GENERATE_GUID);
            STARTUP_ERROR_CASE(STARTUP_OTHER_FAILURE);
        }

        return -1;
    }

    //If running the coordinator and server on the same peer, then we need to deduce our external IP.
    //Easiest way to do this is to connect to the punchthrough server and look at our external IP address for it

	while (true)
	{
		for (RakNet::Packet *p = peer->Receive();
			p != NULL;
			peer->DeallocatePacket(p), p = peer->Receive())
		{
            //printf("Packet %d %d %s\n", p->data[0], p->data[1], p->systemAddress.ToString());
		}

		// Keep raknet threads responsive
		RakSleep(30);
	}
}