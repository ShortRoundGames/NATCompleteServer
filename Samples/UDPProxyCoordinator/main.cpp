//Starts a RakNet peer on port 60000 and runs a punchthrough server. No extra fluff

#include "RakPeerInterface.h"
#include "RakSleep.h"
#include "UDPProxyCoordinator.h"
#include "UDPProxyServer.h"
#include "MessageIdentifiers.h"

using namespace RakNet;

const unsigned short MAX_CONNECTIONS = 65535;

#define STARTUP_ERROR_CASE(X) case X: printf(#X); break;

int main(int argc, char *argv[])
{
    char* coordinatorPassword = "balls";
    char* connectPassword = "";
    USHORT port = 60000;
    
    for (int i = 0; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-coordinatorPassword") && i < (argc - 1))
        {
            coordinatorPassword = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-connectPassword") && i < (argc - 1))
        {
            connectPassword = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-port") && i < (argc - 1))
        {
            port = (USHORT)atoi(argv[i + 1]);
        }
    }
        
	RakNet::RakPeerInterface *peer = RakNet::RakPeerInterface::GetInstance();
	peer->SetMaximumIncomingConnections(MAX_CONNECTIONS);

    RakNet::UDPProxyCoordinator* proxyCoordinator = new RakNet::UDPProxyCoordinator();
    proxyCoordinator->SetRemoteLoginPassword(coordinatorPassword);
    peer->AttachPlugin(proxyCoordinator);

    RakNet::SocketDescriptor socketDescriptor(port, 0);
    RakNet::StartupResult result = peer->Startup(MAX_CONNECTIONS, &socketDescriptor, 1);

    if (connectPassword[0])
        peer->SetIncomingPassword(connectPassword, strlen(connectPassword));

    printf("RakPeer startup %d\n", result);

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