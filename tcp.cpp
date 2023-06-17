#include "tcp.h"

void Server::_init()
{
    m_socket = -1;
    m_isRunning = false;
    m_port = 0;
    m_rcvCallback = nullptr;
    m_disconCallback = nullptr;
    m_userData = nullptr;
    m_acceptTh = nullptr;
    m_rcvTh = nullptr;
    m_logger.setLevel(LOG_LEVEL_DEBUG);

    m_maxfd = 0;
    FD_ZERO(&m_readfds);

    return;
}

Server::Server()
{
    _init();
}

Server::~Server()
{
    if(m_isRunning)
        stop();
    _init();
}

void Server::_acceptThread(Server *obj)
{
    fd_set fd;
    struct timeval tv;
    int *clientSock = nullptr;
    struct sockaddr_in clientAddress;
    unsigned int addrLenth;
    char *clientIP = nullptr;

    while(obj->m_isRunning)
    {
        fd = obj->m_readfds;
        tv.tv_sec = 3;
        tv.tv_usec = 1000 * 10;

        if(select(obj->m_maxfd + 1, &fd, NULL, NULL, &tv) == 0) continue;
        obj->m_logger.debug("1");
        if(FD_ISSET(obj->m_socket, &fd))
        {
            obj->m_logger.debug("2");
            clientSock = new int;
            clientIP = new char[20];
            memset(clientIP, 0, 20);
            *clientSock = accept(obj->m_socket, (sockaddr *)&clientAddress, &addrLenth);
            obj->m_logger.debug("3");
            //? 처음 연결된 클라이언트는 무조건 0.0.0.0 으로 표기됨
            inet_ntop(PF_INET, &(clientAddress.sin_addr), clientIP, 20);
            FD_SET(*clientSock, &obj->m_readfds);
            obj->m_maxfd = *clientSock;
            obj->m_logger.debug("4");

            obj->m_clientList.emplace_back(clientSock, clientIP);
            obj->m_logger.info("connect client [IP:{} [con:{}]]", clientIP, obj->m_clientList.size());
        }

    }
    if(clientSock != nullptr)
    {
        delete clientSock;
        clientSock = nullptr;
    }
    if(clientIP != nullptr)
    {
        delete clientIP;
        clientIP = nullptr;
    }
    obj->m_logger.info("finished accept thread");
    return;
}

void Server::setPort(int port)
{
    m_port = port;
    m_logger.info("set port {}", port);
}

void Server::_receiveThread(Server *obj)
{
    fd_set fd;
    struct timeval tv;
    int clientSocket = -1;
    std::string clientIP;
    int receiveSize = -1;
    ReceiveCallback receiveCallback = obj->m_rcvCallback;
    DisconCallback disconCallback = obj->m_disconCallback;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    unsigned char *data = nullptr;
    bool isDeleted = false;

    while(obj->m_isRunning)
    {
        fd = obj->m_readfds;
        tv.tv_sec = 3;
        tv.tv_usec = 1000 * 10;
        int fdnum = 0;
        if((fdnum=select(obj->m_maxfd + 1, &fd, NULL, NULL, &tv)) == 0);
        obj->m_logger.debug("5 {}", fdnum);
        for(auto iter = obj->m_clientList.begin(); iter != obj->m_clientList.end();)
        {
            obj->m_logger.debug("6");
            isDeleted = false;
            clientSocket = *(iter->first);
            clientIP = iter->second;
            if(FD_ISSET(clientSocket, &fd))
            {
                do
                {
                    obj->m_logger.debug("7");
                    receiveSize = recv(clientSocket, packet, __BUFFER_SIZE, MSG_PEEK);
                    if(receiveSize <= 0)
                    {
                        obj->m_logger.debug("8");
                        obj->m_logger.warn("client was disconnected [IP:{}]", clientIP);
                        if(disconCallback)
                            disconCallback(iter->second, obj->m_userData);
                        obj->m_logger.debug("9");
                        FD_CLR(clientSocket, &obj->m_readfds);
                        close(clientSocket);
                        delete iter->first;
                        delete[] iter->second;
                        iter = obj->m_clientList.erase(iter);
                        obj->m_logger.debug("10");
                        if(data != nullptr)
                        {
                            delete[] data;
                            data = nullptr;
                        }
                        obj->m_logger.debug("11");
                        isDeleted = true;
                        break;
                    }
                    else if(receiveSize >= __BUFFER_SIZE)
                    {
                        recv(clientSocket, packet, __BUFFER_SIZE, 0);
                        
                        if(packet->accumulatedSize == packet->currentSize)
                            data = new unsigned char[packet->totalSize];

                        if(data != nullptr)
                            memcpy(data + packet->accumulatedSize - packet->currentSize, packet->data, packet->currentSize);

                        if(packet->totalSize == packet->accumulatedSize)
                        {
                            obj->m_logger.debug("receive packet [totalSize:{}]", packet->totalSize);
                            if(receiveCallback)
                                receiveCallback(packet->totalSize, data, obj->m_userData);

                            if(data != nullptr)
                            {
                                delete[] data;
                                data = nullptr;
                            }
                        }
                    }
                } while (packet->accumulatedSize < packet->totalSize);
            }
            if(!isDeleted)
                iter++;
        }
    }

    delete[] packet;
    if(data != nullptr)
    {
        delete[] data;
        data = nullptr;
    }
    return;
}

bool Server::start( ReceiveCallback rcvCallback,
                    DisconCallback  disconCallback,
                    void*           userData)
{
    if(!rcvCallback || !disconCallback)
    {
        m_logger.error("not register callback function");
        return false;
    }
    if(m_isRunning)
    {
        m_logger.warn("aleady server is started");
        return true;
    }
    bool result = false;
    m_rcvCallback = rcvCallback;
    m_disconCallback = disconCallback;
    m_userData = userData;

    m_socket = socket(PF_INET, SOCK_STREAM, 0);
    if(m_socket < 0)
    {
        m_logger.error("can't create socket");
        return result;
    }

    m_address.sin_family = PF_INET;
    m_address.sin_addr.s_addr = INADDR_ANY;
    m_address.sin_port = htons(m_port);
    
    if(bind(m_socket, (sockaddr *)&m_address, sizeof(m_address)) < 0)
    {
        m_logger.error("can't bind {} port", m_port);
        close(m_socket);
        return result;
    }

    if(listen(m_socket, 10) < 0)
    {
        m_logger.error("can't listen");
        close(m_socket);
        return result;
    }

    FD_SET(m_socket, &m_readfds);
    m_maxfd = m_socket;

    m_isRunning = true;
    m_acceptTh = new std::thread(&Server::_acceptThread, this);
    m_rcvTh = new std::thread(&Server::_receiveThread, this);

    result = true;
    return false;
}

bool Server::stop()
{
    m_isRunning = false;
    if(m_rcvTh != nullptr)
    {
        m_rcvTh->join();
        delete m_rcvTh;
        m_rcvTh = nullptr;
    }
    
    if(m_rcvTh != nullptr)
    {
        m_acceptTh->join();
        delete m_acceptTh;
        m_acceptTh = nullptr;
    }

    return false;
}

unsigned long long Server::_send(std::string ip, unsigned char *data, unsigned long long size)
{
    unsigned long long sendSize = 0;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    bool isBroadcasting = true;

    std::vector<std::pair<int*, char*>>::iterator selectedSock;
    
    if(!ip.empty())
    {
        isBroadcasting = false;
        selectedSock = std::find_if(m_clientList.begin(), m_clientList.end(), [&ip](std::pair<int *, char*> &sockInfo) -> bool {
            return strcmp(sockInfo.second, ip.c_str())==0?true:false;
        });
    }

    m_mutex.lock();
    packet->totalSize = size;
    packet->accumulatedSize = 0;
    packet->currentSize = 0;
    while(packet->accumulatedSize < packet->totalSize)
    {
        if(packet->totalSize - packet->accumulatedSize < __BUFFER_SIZE - sizeof(Packet))
            packet->currentSize = packet->totalSize - packet->accumulatedSize;
        else
            packet->currentSize = __BUFFER_SIZE - sizeof(Packet);

        memset(packet->data, 0, __BUFFER_SIZE - sizeof(Packet));
        memcpy(packet->data, data + packet->accumulatedSize, packet->currentSize);

        packet->accumulatedSize += packet->currentSize;

        if(isBroadcasting)
            for(auto iter = m_clientList.begin(); iter != m_clientList.end(); iter++)
                sendSize += send(*iter->first, &packet, __BUFFER_SIZE, 0);
        else
            sendSize += send(*selectedSock->first, &packet, __BUFFER_SIZE, 0);

    }
    m_mutex.unlock();

    delete[] packet;
    return sendSize;
}

unsigned long long Server::sendTo(std::string ip, unsigned char *data, unsigned long long size)
{
    return _send(ip, data, size);
}

unsigned long long Server::sendToAll(unsigned char *data, unsigned long long size)
{
    return _send(nullptr, data, size);
}

void Client::_init()
{
    m_ip.clear();
    m_isConnected = false;
    m_socket = -1;
    memset(&m_address, 0, sizeof(struct sockaddr_in));
    m_port = -1;
    FD_ZERO(&m_readfds);
    m_rcvCallback = nullptr;
    m_disconCallback = nullptr;
    m_userData = nullptr;
    m_rcvTh = nullptr;
    m_logger.setLevel(LOG_LEVEL_DEBUG);

    return;
}

Client::Client()
{
    _init();

}

void Client::_receiveThread(Client *obj)
{
    fd_set fd;
    struct timeval tv;
    int socket = obj->m_socket;
    int receiveSize = -1;
    ReceiveCallback receiveCallback = obj->m_rcvCallback;
    DisconCallback disconCallback = obj->m_disconCallback;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    unsigned char *data = nullptr;

    while(obj->m_isConnected)
    {
        fd = obj->m_readfds;
        tv.tv_sec = 3;
        tv.tv_usec = 1000 * 10;

        if(select(obj->m_maxfd + 1, &fd, NULL, NULL, &tv) == 0) continue;
        if(FD_ISSET(socket, &fd))
        {
            do
            {
                receiveSize = recv(socket, packet, __BUFFER_SIZE, MSG_PEEK);
                if(receiveSize <= 0)
                {
                    if(disconCallback)
                        disconCallback(0, obj->m_userData);

                    obj->m_logger.warn("server was disconnected");
                    close(socket);
                    if(data != nullptr)
                    {
                        delete[] data;
                        data = nullptr;
                    }

                    break;
                }
                else if(receiveSize >= __BUFFER_SIZE)
                {
                    recv(socket, packet, __BUFFER_SIZE, 0);
                    if(packet->accumulatedSize == packet->currentSize)
                        data = new unsigned char[packet->totalSize];

                    memcpy(data + packet->accumulatedSize - packet->currentSize, packet->data, packet->currentSize);

                    if(packet->totalSize == packet->accumulatedSize)
                    {
                        obj->m_logger.debug("receive packet [totalSize:{}]", packet->totalSize);
                        if(receiveCallback)
                            receiveCallback(packet->totalSize, data, obj->m_userData);

                        if(data != nullptr)
                        {
                            delete[] data;
                            data = nullptr;
                        }
                    }
                }
            } while (packet->accumulatedSize < packet->totalSize);
            
        }
        
    }

    delete[] packet;
    if(data != nullptr)
    {
        delete[] data;
        data = nullptr;
    }
    return;
}

bool Client::conn(std::string ip, int port, ReceiveCallback rcvCallback, DisconCallback disconCallback, void *userData)
{
    if(!rcvCallback || !disconCallback)
    {
        m_logger.error("not register callback function");
        return false;
    }
    if(m_isConnected)
    {
        m_logger.warn("aleady client is connected to server");
        return true;
    }

    m_ip.assign(ip);
    m_port = port;
    m_userData = userData;
    m_rcvCallback = rcvCallback;
    m_disconCallback = disconCallback;

    bool result = false;

    m_socket = socket(PF_INET, SOCK_STREAM, 0);
    if(m_socket < 0)
    {
        m_logger.error("can't create socket");
        return result;
    }

    m_address.sin_family = PF_INET;
    m_address.sin_port = htons(m_port);
    m_address.sin_addr.s_addr = inet_addr(m_ip.c_str());

    if(connect(m_socket, (sockaddr *)&m_address, sizeof(m_address)) < 0)
    {
        m_logger.error("can't connect server");
        close(m_socket);

        return result;
    }

    FD_SET(m_socket, &m_readfds);
    m_maxfd = m_socket;

    m_isConnected = true;
    m_rcvTh = new std::thread(&Client::_receiveThread, this);

    result = true;
    return result;
}

bool Client::disconn()
{
    bool result = false;

    m_isConnected = false;
    if(m_rcvTh != nullptr)
    {
        m_rcvTh->join();
        delete m_rcvTh;
        m_rcvTh = nullptr;
    }

    close(m_socket);
    FD_CLR(m_socket, &m_readfds); 

    return result;
}

unsigned long long Client::sendTo(unsigned char *data, unsigned long long size)
{
    unsigned long long sendSize = 0;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];

    packet->totalSize = size;
    packet->accumulatedSize = 0;
    packet->currentSize = 0;
    while(packet->accumulatedSize < packet->totalSize)
    {
        if(packet->totalSize - packet->accumulatedSize < __BUFFER_SIZE - sizeof(Packet))
            packet->currentSize = packet->totalSize - packet->accumulatedSize;
        else
            packet->currentSize = __BUFFER_SIZE - sizeof(Packet);

        memset(packet->data, 0, __BUFFER_SIZE - sizeof(Packet));
        memcpy(packet->data, data + packet->accumulatedSize, packet->currentSize);

        packet->accumulatedSize += packet->currentSize;
        sendSize += send(m_socket, packet, __BUFFER_SIZE, 0);
    }

    delete[] packet;
    return sendSize;
}

Client::~Client()
{
    if(m_isConnected)
        disconn();
    _init();
}