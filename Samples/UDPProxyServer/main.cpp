//Starts a RakNet peer on port 60000 and runs a punchthrough server. No extra fluff

#include "RakPeerInterface.h"
#include "RakSleep.h"
#include "UDPProxyCoordinator.h"
#include "UDPProxyServer.h"
#include "MessageIdentifiers.h"

const unsigned short MAX_CONNECTIONS = 65535;

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
    }
    virtual void OnWrongPassword(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        printf("Failed login to UDPProxyCoordinator. Wrong password.\n");
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
        USHORT port = (USHORT)atoi(portString);
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
    if (coordinatorAddress)
    {
        if (Connect(peer, coordinatorAddress, connectPassword))
        {
            return COORDINATOR;
        }
    }
    else
    {
        proxyServer->LoginToCoordinator(coordinatorPassword, peer->GetInternalID(RakNet::UNASSIGNED_SYSTEM_ADDRESS, 0));
    }

    return NONE;
}

int main(int argc, char *argv[])
{
    char* coordinatorPassword = "balls";
    char* connectPassword = "";

    char* coordinatorAddress = NULL;
    char* externalAddress = NULL;
    char* externalServer = NULL;

    USHORT port = 60000;
    
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
            port = (USHORT)atoi(argv[i + 1]);
        }
    }

    eConnectTarget target = NONE;

    RakNet::UDPProxyCoordinator* proxyCoordinator = NULL;
    
	RakNet::RakPeerInterface *peer = RakNet::RakPeerInterface::GetInstance();
	peer->SetMaximumIncomingConnections(MAX_CONNECTIONS);

    if (!coordinatorAddress)
    {
        proxyCoordinator = new RakNet::UDPProxyCoordinator();
        proxyCoordinator->SetRemoteLoginPassword(coordinatorPassword);
        peer->AttachPlugin(proxyCoordinator);
    }

    RakNet::UDPProxyServer proxyServer;
    UDPProxyServerResultHandler_SimpleServer resultHandler;
    proxyServer.SetResultHandler(&resultHandler);
    peer->AttachPlugin(&proxyServer);

    RakNet::SocketDescriptor socketDescriptor(port, 0);
    RakNet::StartupResult result = peer->Startup(MAX_CONNECTIONS, &socketDescriptor, 1);

    if (connectPassword[0])
        peer->SetIncomingPassword(connectPassword, strlen(connectPassword));

    printf("RakPeer startup %d\n", result);

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

            case ID_CONNECTION_ATTEMPT_FAILED:
            case ID_ALREADY_CONNECTED:
            case ID_NO_FREE_INCOMING_CONNECTIONS:
            case ID_CONNECTION_BANNED:
            case ID_INVALID_PASSWORD:
            {
                if (target == COORDINATOR)
                {
                    printf("Failed to connect to coordinator, code %d\n", p->data[0]);
                }
                else if (target == EXTERNAL)
                {
                    printf("Failed to connect to external server, code %d\n", p->data[0]);
                }
                break;
            }
            }
		}

		// Keep raknet threads responsive
		RakSleep(30);
	}
}