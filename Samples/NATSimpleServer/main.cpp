#include "RakPeerInterface.h"
#include "NatPunchthroughServer.h"
#include "RakSleep.h"

const unsigned short MAX_CONNECTIONS = 65535;

struct NatPunchthroughServerDebugInterface_SimpleServer : public RakNet::NatPunchthroughServerDebugInterface
{
	virtual void OnServerMessage(const char *msg)
	{
		printf("%s\n", msg);
	}
};

int main(void)
{
	RakNet::RakPeerInterface *peer = RakNet::RakPeerInterface::GetInstance();
	peer->SetMaximumIncomingConnections(MAX_CONNECTIONS);

	RakNet::NatPunchthroughServer serverPlugin;
	NatPunchthroughServerDebugInterface_SimpleServer debugInterface;
	serverPlugin.SetDebugInterface(&debugInterface);

	peer->AttachPlugin(&serverPlugin);

	RakNet::SocketDescriptor socketDescriptor(60000, 0);
	RakNet::StartupResult result = peer->Startup(MAX_CONNECTIONS, &socketDescriptor, 1);

	printf("RakPeer startup %d\n", result);

	while (true)
	{
		for (RakNet::Packet *p = peer->Receive();
			p != NULL;
			peer->DeallocatePacket(p), p = peer->Receive())
		{
			//printf("Packet %d %s\n", p->data[0], p->systemAddress.ToString());
		}

		// Keep raknet threads responsive
		RakSleep(30);
	}
}