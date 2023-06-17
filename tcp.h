#ifndef __SOCKET_H__
#define __SOCKET_H__
// C++ header
#include <algorithm>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <utility>
#include <mutex>
// POSIX socket header
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
typedef std::function<void(int, void*)> DisconCallback;

class Server
{
private:
    void _init();
    Logger m_logger;
    bool m_isRunning;
    int m_socket;
    struct sockaddr_in m_address;
    int m_port;
    unsigned int m_maxfd;
    struct timeval m_tv;
    fd_set m_readfds;
    std::vector<std::pair<int*, char*>> m_clientList;
    ReceiveCallback m_rcvCallback;
    DisconCallback m_disconCallback;
    void *m_userData;
    std::thread *m_acceptTh;
    std::thread *m_rcvTh;
    std::mutex m_mutex;

    static void _acceptThread(Server *obj);
    static void _receiveThread(Server *obj);

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
    int m_socket;
    struct sockaddr_in m_address;
    int m_port;
    unsigned int m_maxfd;
    struct timeval m_tv;
    fd_set m_readfds;

    ReceiveCallback m_rcvCallback;
    DisconCallback m_disconCallback;
    void *m_userData;
    std::thread *m_rcvTh;

    static void _receiveThread(Client *obj);
public:
    Client();
    ~Client();

    bool conn(std::string ip, int port, ReceiveCallback rcvCallback, DisconCallback disconCallback, void *userData);
    bool disconn();
};

#endif /* __SOCKET_H__ */