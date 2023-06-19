// C++ Header
#include <iostream>
#include <fstream>
// TCP socket
#include "tcp.h"
#include "logger.h"
#define PORT 2342

typedef struct _SockParam
{
    bool isServer;
    Logger *logger;
} SockParam;

void onReceive(int size, unsigned char *data, void *userData)
{
    SockParam *param = (SockParam*)userData;
    Logger *logger = param->logger;
    if(param->isServer)
    {
        // Server side
        logger->info("server is receive data [size:{}]", size);
        logger->info("server is receive data [data:{}]", (char*)data);
    }
    else
    {
        // Client side
        logger->info("client is receive data [size:{}]", size);
        logger->info("client is receive data [data:{}]", (char*)data);
    }

    return;
}

void onDisconn(const char *ip, void *userData)
{
    SockParam *param = (SockParam*)userData;
    Logger *logger = param->logger;
    if(param->isServer)
    {
        // Server side
        logger->warn("disconnect client [IP:{}]", ip);
    }
    else
    {
        // Client side
        logger->warn("disconnect server [IP:{}]", ip);
    }

    return;
}

int main()
{
    Logger logger;

    SockParam svrParam;
    svrParam.isServer = true;
    svrParam.logger = &logger;
    Server server;
    
    SockParam cliParam;
    cliParam.isServer = false;
    cliParam.logger = &logger;
    Client client;

    logger.info("Press 'ENTER' to start server (exit:'x')");
    if(getchar() == 'x') return 0;

    server.setPort(PORT);
    server.start(onReceive, onDisconn, &svrParam);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    logger.info("Press 'ENTER' to connect server (exit:'x')");
    if(getchar() == 'x') return 0;

    if(client.conn("127.0.0.1", PORT, onReceive, onDisconn, &cliParam))
        logger.info("Now is avaliable to recevie and send data!");
    else
        logger.error("failed to connect server");

    // Logic ...
    logger.info("Press 'ENTER' to send data (Client > Server) (exit:'x')");
    if(getchar() == 'x') return 0;
    client.sendTo((unsigned char*)"C > S", 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    logger.info("Press 'ENTER' to send data (Server > Client) (exit:'x')");
    if(getchar() == 'x') return 0;
    server.sendToAll((unsigned char*)"S > C", 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    logger.info("Press 'ENTER' to exit");
    getchar();
    client.disconn();
    server.stop();
    
    return 0;
}