//Starts a RakNet peer on port 60000 and runs a punchthrough server. No extra fluff

#include "RakPeerInterface.h"
#include "RakSleep.h"
#include "UDPProxyCoordinator.h"
#include "UDPProxyServer.h"
#include "MessageIdentifiers.h"

using namespace RakNet;

const unsigned short MAX_CONNECTIONS = 65535;

bool g_terminate = false;

struct UDPProxyServerResultHandler_SimpleServer : public RakNet::UDPProxyServerResultHandler
{
    virtual void OnLoginSuccess(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        printf("Logged into UDPProxyCoordinator.\n");
    }
    virtual void OnAlreadyLoggedIn(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        printf("Already logged into UDPProxyCoordinator.\n");
    }
    virtual void OnNoPasswordSet(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        printf("Failed login to UDPProxyCoordinator. No password set.\n");
        g_terminate = true;
    }
    virtual void OnWrongPassword(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        printf("Failed login to UDPProxyCoordinator. Wrong password.\n");
        g_terminate = true;
    }
};

enum eConnectTarget
{
    NONE,
    COORDINATOR,
    EXTERNAL
};

bool Connect(RakNet::RakPeerInterface* peer, char* addressAndPort, char* password)
{
    char* addressCopy = new char[strlen(addressAndPort) + 1];
    strcpy(addressCopy, addressAndPort);

    char* address = strtok(addressCopy, ":");
    char* portString = strtok(NULL, ":");

    if (address && portString)
    {
        unsigned short port = (unsigned short)atoi(portString);
        peer->Connect(address, port, password, strlen(password));
        return true;
    }
    else
    {
        printf("addresses should be in form 'address:port'");
        return false;
    }
}

eConnectTarget LoginToCoordinator(RakNet::RakPeerInterface* peer, RakNet::UDPProxyServer* proxyServer, char* connectPassword, char* coordinatorAddress, char* coordinatorPassword)
{
    if (Connect(peer, coordinatorAddress, connectPassword))
    {
        return COORDINATOR;
    }

    return NONE;
}

void ConnectFailure(eConnectTarget target, const char* errorCode)
{
    if (target == COORDINATOR)
    {
        printf("Failed to connect to coordinator: %s\n", errorCode);
    }
    else if (target == EXTERNAL)
    {
        printf("Failed to connect to external server: %s\n", errorCode);
    }
}

#define STARTUP_ERROR_CASE(X) case X: printf(#X); break;
#define CONNECT_ERROR_CASE(X) case X: ConnectFailure(target, #X); return -1;

int main(int argc, char *argv[])
{
    char* coordinatorPassword = "balls";
    char* connectPassword = "";

    char* coordinatorAddress = NULL;
    char* externalAddress = NULL;
    char* externalServer = NULL;

    unsigned short port = 60000;
    
    for (int i = 0; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-coordinatorPassword") && i < (argc - 1))
        {
            coordinatorPassword = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-coordinator") && i < (argc - 1))
        {
            coordinatorAddress = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-connectPassword") && i < (argc - 1))
        {
            connectPassword = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-externalAddress") && i < (argc - 1))
        {
            externalAddress = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-externalServer") && i < (argc - 1))
        {
            externalServer = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-port") && i < (argc - 1))
        {
            port = (unsigned short)atoi(argv[i + 1]);
        }
    }

    if (coordinatorAddress == NULL)
    {
        printf("-coordinator argument not provided. Don't know what coordinator to use");
        return -1;
    }

    eConnectTarget target = NONE;
        
	RakNet::RakPeerInterface *peer = RakNet::RakPeerInterface::GetInstance();
	peer->SetMaximumIncomingConnections(MAX_CONNECTIONS);

    RakNet::UDPProxyServer proxyServer;
    UDPProxyServerResultHandler_SimpleServer resultHandler;
    proxyServer.SetResultHandler(&resultHandler);
    peer->AttachPlugin(&proxyServer);

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
        
    if (externalServer)
    {
        if (Connect(peer, externalServer, ""))
        {
            target = EXTERNAL;
        }
    }
    else
    {
        if (externalAddress)
            proxyServer.SetServerPublicIP(externalAddress);

        target = LoginToCoordinator(peer, &proxyServer, connectPassword, coordinatorAddress, coordinatorPassword);
    }

	while (true)
	{
		for (RakNet::Packet *p = peer->Receive();
			p != NULL;
			peer->DeallocatePacket(p), p = peer->Receive())
		{
            //printf("Packet %d %d %s\n", p->data[0], p->data[1], p->systemAddress.ToString());

            switch (p->data[0])
            {
            case ID_CONNECTION_REQUEST_ACCEPTED:
            {
                if (target == COORDINATOR)
                {
                    target = NONE;
                    proxyServer.LoginToCoordinator(coordinatorPassword, p->systemAddress);
                }
                else if (target == EXTERNAL)
                {
                    RakNet::SystemAddress externalSystemAddress = peer->GetExternalID(p->systemAddress);
                    printf("External address detected as %s\n", externalSystemAddress.ToString());
                    proxyServer.SetServerPublicIP(externalSystemAddress.ToString(false));
                    peer->CloseConnection(p->systemAddress, false);

                    target = LoginToCoordinator(peer, &proxyServer, connectPassword, coordinatorAddress, coordinatorPassword);
                }
                break;
            }

            CONNECT_ERROR_CASE(ID_CONNECTION_ATTEMPT_FAILED);
            CONNECT_ERROR_CASE(ID_ALREADY_CONNECTED);
            CONNECT_ERROR_CASE(ID_NO_FREE_INCOMING_CONNECTIONS);
            CONNECT_ERROR_CASE(ID_CONNECTION_BANNED);
            CONNECT_ERROR_CASE(ID_INVALID_PASSWORD);
            }
		}

		// Keep raknet threads responsive
		RakSleep(30);

        if (g_terminate)
            return -1;
	}

}