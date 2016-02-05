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

int main(void)
{
	RakNet::RakPeerInterface *peer = RakNet::RakPeerInterface::GetInstance();
	peer->SetMaximumIncomingConnections(MAX_CONNECTIONS);

    RakNet::UDPProxyCoordinator proxyCoordinator;
    proxyCoordinator.SetRemoteLoginPassword("balls");
    peer->AttachPlugin(&proxyCoordinator);

    RakNet::UDPProxyServer proxyServer;
    UDPProxyServerResultHandler_SimpleServer resultHandler;
    proxyServer.SetResultHandler(&resultHandler);
    peer->AttachPlugin(&proxyServer);

    RakNet::SocketDescriptor socketDescriptor(60000, 0);
    RakNet::StartupResult result = peer->Startup(MAX_CONNECTIONS, &socketDescriptor, 1);

    printf("RakPeer startup %d\n", result);

    //If running the coordinator and server on the same peer, then we need to deduce our external IP.
    //Easiest way to do this is to connect to the punchthrough server and look at our external IP address for it

    bool m_needToDeduceExternalIp = true;

    //proxyServer.LoginToCoordinator("balls", RakNet::UNASSIGNED_SYSTEM_ADDRESS);

    if (m_needToDeduceExternalIp)
        peer->Connect("54.173.17.206", 60000, "", 0);
    else
        proxyServer.LoginToCoordinator("balls", peer->GetInternalID(RakNet::UNASSIGNED_SYSTEM_ADDRESS, 0));

	while (true)
	{
		for (RakNet::Packet *p = peer->Receive();
			p != NULL;
			peer->DeallocatePacket(p), p = peer->Receive())
		{
			//printf("Packet %d %s\n", p->data[0], p->systemAddress.ToString());

            if (m_needToDeduceExternalIp && p->data[0] == ID_CONNECTION_REQUEST_ACCEPTED)
            {
                RakNet::SystemAddress externalAddress = peer->GetExternalID(p->systemAddress);
                printf("%s\n", externalAddress.ToString());
                proxyServer.SetServerPublicIP(externalAddress.ToString(false));
                peer->CloseConnection(p->systemAddress, false);

                proxyServer.LoginToCoordinator("balls", peer->GetInternalID(RakNet::UNASSIGNED_SYSTEM_ADDRESS, 0));

                m_needToDeduceExternalIp = false;
            }
		}

		// Keep raknet threads responsive
		RakSleep(30);
	}
}