#ifndef __SOCKET_H__
#define __SOCKET_H__
// C++ header
#include <algorithm>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>
#include <utility>
// POSIX socket header
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
// Logger header
#include "logger.h"
// Definition
#define __BUFFER_SIZE 1024
typedef struct Packet_t
{
    unsigned long long totalSize;
    unsigned long long accumulatedSize;
    unsigned long long currentSize;
    unsigned char data[];
} Packet;
typedef std::function<void(int, unsigned char*, void*)> ReceiveCallback;
typedef std::function<void(char *, void*)> DisconCallback;
typedef struct ClientInfo_t
{
    unsigned int socket;
    char ipAddress[20];

    ClientInfo_t(unsigned int fd, char *ip)
    {
        socket = fd;
        memset(ipAddress, 0, 20);
        memcpy(ipAddress, ip, 20);
    }
} ClientInfo;

class Server
{
private:
    void _init();
    Logger m_logger;
    bool m_isRunning;
    unsigned int m_socket;
    struct sockaddr_in m_address;
    int m_port;
    unsigned int m_maxfd;
    fd_set m_readfds;
    unsigned long long m_clientCount;
    std::vector<ClientInfo> m_clientList; //! critical section
    ReceiveCallback m_rcvCallback;
    DisconCallback m_disconCallback;
    void *m_userData;
    std::thread *m_rcvTh;
    std::mutex m_mutex;

    static void _receiveThread(Server *obj);
    static void _getIpAddress(int fd, char *ret);

    unsigned long long _send(std::string ip, unsigned char *data, unsigned long long size);
public:
    Server();
    ~Server();

    void setPort(int port);
    bool start( ReceiveCallback rcvCallback,
                DisconCallback  disconCallback,
                void*           userData);

    bool stop();
    unsigned long long sendToAll(unsigned char *data, unsigned long long size);
    unsigned long long sendTo(std::string ip, unsigned char *data, unsigned long long size);

};

class Client
{
private:
    void _init();
    std::string m_ip;
    Logger m_logger;
    bool m_isConnected;
    unsigned int m_socket;
    struct sockaddr_in m_address;
    int m_port;
    unsigned int m_maxfd;
    fd_set m_readfds;

    ReceiveCallback m_rcvCallback;
    DisconCallback m_disconCallback;
    void *m_userData;
    std::thread *m_rcvTh;

    static void _receiveThread(Client *obj);
    static void _getIpAddress(int fd, char *ret);
public:
    Client();
    ~Client();

    bool conn(std::string ip, int port, ReceiveCallback rcvCallback, DisconCallback disconCallback, void *userData);
    bool disconn();

    unsigned long long sendTo(unsigned char *data, unsigned long long size);
};

#endif /* __SOCKET_H__ */