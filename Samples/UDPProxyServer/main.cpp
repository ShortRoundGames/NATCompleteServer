//Starts a RakNet peer on port 60000 and runs a punchthrough server. No extra fluff

#include "RakPeerInterface.h"
#include "RakPeer.h"
#include "RakSleep.h"
#include "UDPProxyCoordinator.h"
#include "UDPProxyServer.h"
#include "MessageIdentifiers.h"

#include <time.h> 

using namespace RakNet;

const unsigned short MAX_CONNECTIONS = 65535;

bool g_terminate = false;
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

struct UDPProxyServerResultHandler_SimpleServer : public RakNet::UDPProxyServerResultHandler
{
    virtual void OnLoginSuccess(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        Log("Logged into UDPProxyCoordinator.\n");
    }
    virtual void OnAlreadyLoggedIn(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        Log("Already logged into UDPProxyCoordinator.\n");
    }
    virtual void OnNoPasswordSet(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        Log("Failed login to UDPProxyCoordinator. No password set.\n");
        g_terminate = true;
    }
    virtual void OnWrongPassword(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
    {
        Log("Failed login to UDPProxyCoordinator. Wrong password.\n");
        g_terminate = true;
    }
};

enum eConnectTarget
{
    NONE,
    COORDINATOR,
    EXTERNAL
};


#define CONNECTION_ATTEMPT_RESULT_CASE(X) case X: return #X; break;
const char* ToString(ConnectionAttemptResult result)
{
    switch (result)
    {
        CONNECTION_ATTEMPT_RESULT_CASE(CONNECTION_ATTEMPT_STARTED);
        CONNECTION_ATTEMPT_RESULT_CASE(INVALID_PARAMETER);
        CONNECTION_ATTEMPT_RESULT_CASE(CANNOT_RESOLVE_DOMAIN_NAME);
        CONNECTION_ATTEMPT_RESULT_CASE(ALREADY_CONNECTED_TO_ENDPOINT);
        CONNECTION_ATTEMPT_RESULT_CASE(CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS);
        CONNECTION_ATTEMPT_RESULT_CASE(SECURITY_INITIALIZATION_FAILED);
    }

    return "(unknown)";
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

#define MESSAGE_ID_CASE(X) case X: return #X; break;
const char* ToString(unsigned char id)
{
    switch (id)
    {
        MESSAGE_ID_CASE(ID_CONNECTION_REQUEST_ACCEPTED);
        MESSAGE_ID_CASE(ID_CONNECTION_LOST);
        MESSAGE_ID_CASE(ID_CONNECTION_ATTEMPT_FAILED);
        MESSAGE_ID_CASE(ID_ALREADY_CONNECTED);
        MESSAGE_ID_CASE(ID_NO_FREE_INCOMING_CONNECTIONS);
        MESSAGE_ID_CASE(ID_CONNECTION_BANNED);
        MESSAGE_ID_CASE(ID_INVALID_PASSWORD);
    }

    return "(unknown)";
}

bool Connect(RakNet::RakPeerInterface* peer, char* addressAndPort, char* password)
{
    char* addressCopy = new char[strlen(addressAndPort) + 1];
    strcpy(addressCopy, addressAndPort);

    char* address = strtok(addressCopy, ":");
    char* portString = strtok(NULL, ":");

    bool success = false;

    if (address && portString)
    {
        unsigned short port = (unsigned short)atoi(portString);
        ConnectionAttemptResult result = peer->Connect(address, port, password, strlen(password));
        Log("ConnectionAttemptResult for %s : %s\n", addressAndPort, ToString(result));

        success = (result == CONNECTION_ATTEMPT_STARTED);
    }
    else
    {
        Log("addresses should be in form 'address:port', addr:%s\n", addressCopy);
    }

    delete[] addressCopy;
    return success;
}

time_t g_reconnectTime = -1;
time_t g_reconnectTimeDelta = 10;

void ResetReconnectTimeDelta()
{
    g_reconnectTimeDelta = 10;
}

void ActivateReconnectTime()
{
    Log("Reattempting connection in %d seconds\n", g_reconnectTimeDelta);

    time(&g_reconnectTime);
    g_reconnectTime += g_reconnectTimeDelta;

    //Sequential fails should wait longer between re-attempts, but cap max wait time so it doesn't wait too long
    if (g_reconnectTimeDelta < 300)
        g_reconnectTimeDelta *= 2;
}

int main(int argc, char *argv[])
{
    char* coordinatorPassword = "balls";
    char* clientPassword = "";

    char* coordinatorAddress = NULL;
    char* externalAddress = NULL;
    char* externalServer = NULL;
    char* logFilename = NULL;

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
        else if (!strcmp(argv[i], "-clientPassword") && i < (argc - 1))
        {
            clientPassword = argv[i + 1];
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

    if (coordinatorAddress == NULL)
    {
        Log("-coordinator argument not provided. Don't know what coordinator to use\n");
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

    if (clientPassword[0])
        peer->SetIncomingPassword(clientPassword, strlen(clientPassword));

    Log("RakPeer startup %s\n", ToString(result));

    if (result != RAKNET_STARTED)
        return -1;
    
    if (externalServer)
    {
        target = EXTERNAL;

        if (!Connect(peer, externalServer, ""))
        {
            ActivateReconnectTime();
        }
    }
    else
    {
        if (externalAddress)
            proxyServer.SetServerPublicIP(externalAddress);

        target = COORDINATOR;

        if (!Connect(peer, coordinatorAddress, clientPassword))
        {
            ActivateReconnectTime();
        }
    }

	while (true)
	{
		for (RakNet::Packet *p = peer->Receive();
			p != NULL;
			peer->DeallocatePacket(p), p = peer->Receive())
		{
            //Log("Packet %d %d %s\n", p->data[0], p->data[1], p->systemAddress.ToString());

            switch (p->data[0])
            {
            case ID_CONNECTION_REQUEST_ACCEPTED:
            {
                ResetReconnectTimeDelta();

                if (target == COORDINATOR)
                {
                    Log("Connected to coordinator, logging in\n");

                    target = NONE;
                    proxyServer.LoginToCoordinator(coordinatorPassword, p->systemAddress);
                }
                else if (target == EXTERNAL)
                {
                    RakNet::SystemAddress externalSystemAddress = peer->GetExternalID(p->systemAddress);
                    Log("External address detected as %s\n", externalSystemAddress.ToString());
                    proxyServer.SetServerPublicIP(externalSystemAddress.ToString(false));
                    peer->CloseConnection(p->systemAddress, false);

                    target = COORDINATOR;

                    if (!Connect(peer, coordinatorAddress, clientPassword))
                    {
                        ActivateReconnectTime();
                    }
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
                    Log("Failed to connect to coordinator: %s\n", ToString(p->data[0]));
                    ActivateReconnectTime();
                }
                else if (target == EXTERNAL)
                {
                    Log("Failed to connect to external server: %s\n", ToString(p->data[0]));
                    ActivateReconnectTime();
                }
            }
            break;

            case ID_DISCONNECTION_NOTIFICATION:
            case ID_CONNECTION_LOST:
            {
                const char* sysAddress = p->systemAddress.ToString(true, ':');

                if (strcmp(sysAddress, coordinatorAddress) == 0)
                {
                    Log("Connection lost to %s : %s\n", sysAddress, ToString(p->data[0]));
                    ActivateReconnectTime();

                    target = COORDINATOR;
                }
            }
            break;

            }
		}

        if (g_reconnectTime != -1)
        {
            time_t now;
            time(&now);

            if (now > g_reconnectTime)
            {
                g_reconnectTime = -1;

                if (target == COORDINATOR)
                {
                    if (Connect(peer, coordinatorAddress, clientPassword))
                        g_reconnectTime = -1;
                    else
                        ActivateReconnectTime();
                }
                else if (target == EXTERNAL)
                {
                    if(Connect(peer, externalServer, ""))
                        g_reconnectTime = -1;
                    else
                        ActivateReconnectTime();
                }
            }
        }

		// Keep raknet threads responsive
		RakSleep(30);

        if (g_terminate)
            return -1;
	}

}