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
    _init();
}

void Server::_acceptThread(Server *obj)
{
    fd_set fd;
    int *clientSock = nullptr;
    struct sockaddr_in clientAddress;
    unsigned int addrLenth;
    char *clientIP = nullptr;

    while(obj->m_isRunning)
    {
        fd = obj->m_readfds;
        obj->m_tv.tv_sec = 5;
        obj->m_tv.tv_usec = 0;

        select(obj->m_socket + 1, &fd, NULL, NULL, &obj->m_tv);
        if(FD_ISSET(obj->m_socket, &obj->m_readfds))
        {
            clientSock = new int;
            clientIP = new char[20];
            memset(clientIP, 0, 20);
            *clientSock = accept(obj->m_socket, (sockaddr *)&clientAddress, &addrLenth);
            //? 처음 연결된 클라이언트는 무조건 0.0.0.0 으로 표기됨
            inet_ntop(PF_INET, &(clientAddress.sin_addr), clientIP, 20);
            FD_SET(*clientSock, &obj->m_readfds);
            obj->m_maxfd = *clientSock;

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
    int clientSocket = -1;
    std::string clientIP;
    int receiveSize = -1;
    ReceiveCallback receiveCallback = obj->m_rcvCallback;
    DisconCallback disconCallback = obj->m_disconCallback;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    unsigned char *data = nullptr;

    while(obj->m_isRunning)
    {
        fd = obj->m_readfds;
        obj->m_tv.tv_sec = 5;
        obj->m_tv.tv_usec = 0;

        select(obj->m_maxfd + 1, &fd, NULL, NULL, &obj->m_tv);
        for(int i=0; i<obj->m_clientList.size(); i++)
        {
            clientSocket = *(obj->m_clientList[i].first);
            clientIP = obj->m_clientList[i].second;
            if(FD_ISSET(clientSocket, &fd))
            {
                do
                {
                    receiveSize = recv(clientSocket, packet, __BUFFER_SIZE, MSG_PEEK);
                    if(receiveSize <= 0)
                    {
                        obj->m_logger.warn("client was disconnected [IP:{}]", clientIP);
                        close(clientSocket);
                        FD_CLR(clientSocket, &obj->m_readfds);
                        delete obj->m_clientList[i].first;
                        delete[] obj->m_clientList[i].second;
                        obj->m_clientList.erase(obj->m_clientList.begin() + i);

                        if(data != nullptr)
                        {
                            delete[] data;
                            data = nullptr;
                        }

                        if(disconCallback)
                            disconCallback(i, obj->m_userData);

                        break;
                    }
                    else if(receiveSize >= __BUFFER_SIZE)
                    {
                        recv(clientSocket, packet, __BUFFER_SIZE, 0);
                        if(packet->accumulatedSize == packet->currentSize)
                            data = new unsigned char[packet->totalSize];

                        memcpy(data + packet->accumulatedSize - packet->currentSize, packet->data, packet->currentSize);

                        if(packet->totalSize == packet->accumulatedSize)
                        {
                            if(receiveCallback)
                                receiveCallback(receiveSize, data, obj->m_userData);

                            if(data != nullptr)
                            {
                                delete[] data;
                                data = nullptr;
                            }
                        }
                    }
                    else
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } while (packet->accumulatedSize < packet->totalSize);
                
            }
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

    m_rcvCallback = rcvCallback;
    m_disconCallback = disconCallback;
    m_userData = userData;

    m_socket = socket(PF_INET, SOCK_STREAM, 0);
    if(m_socket < 0)
    {
        m_logger.error("can't create socket");
        return false;
    }

    m_address.sin_family = PF_INET;
    m_address.sin_addr.s_addr = INADDR_ANY;
    m_address.sin_port = htons(m_port);
    
    if(bind(m_socket, (sockaddr *)&m_address, sizeof(m_address)) < 0)
    {
        m_logger.error("can't bind {} port", m_port);
        close(m_socket);
    }

    if(listen(m_socket, 10) < 0)
    {
        m_logger.error("can't listen");
        close(m_socket);
    }

    FD_SET(m_socket, &m_readfds);
    m_maxfd = m_socket;

    m_isRunning = true;
    m_acceptTh = new std::thread(&Server::_acceptThread, this);
    m_rcvTh = new std::thread(&Server::_receiveThread, this);

    return false;
}

bool Server::stop()
{
    m_isRunning = false;
    m_rcvTh->join();
    m_acceptTh->join();
    return false;
}

unsigned long long Server::_send(std::string ip, unsigned char *data, unsigned long long size)
{
    unsigned long long sendSize = -1;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    bool isBroadcasting = true;

    std::vector<std::pair<int*, char*>>::iterator sockInfo;
    
    if(!ip.empty())
    {
        isBroadcasting = false;
        sockInfo = std::find(m_clientList.begin(), m_clientList.end(), [&ip](std::pair<int *, char*> &sockInfo) {
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

        packet->accumulatedSize = packet->currentSize;

        if(isBroadcasting)
            for(auto iter = m_clientList.begin(); iter != m_clientList.end(); iter++)
                sendSize = send(*iter->first, &packet, __BUFFER_SIZE, 0);
        else
            sendSize = send(*sockInfo->first, &packet, __BUFFER_SIZE, 0);

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
    memset(&m_tv, 0, sizeof(struct timeval));
    FD_ZERO(&m_readfds);
    m_rcvCallback = nullptr;
    m_disconCallback = nullptr;
    m_userData = nullptr;
    m_rcvTh = nullptr;

    return;
}

Client::Client()
{
    _init();

}

void Client::_receiveThread(Client *obj)
{
    fd_set fd;
    int socket = obj->m_socket;
    int receiveSize = -1;
    ReceiveCallback receiveCallback = obj->m_rcvCallback;
    DisconCallback disconCallback = obj->m_disconCallback;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    unsigned char *data = nullptr;

    while(obj->m_isConnected)
    {
        fd = obj->m_readfds;
        obj->m_tv.tv_sec = 5;
        obj->m_tv.tv_usec = 0;

        select(obj->m_maxfd + 1, &fd, NULL, NULL, &obj->m_tv);
        
        if(FD_ISSET(socket, &fd))
        {
            do
            {
                receiveSize = recv(socket, packet, __BUFFER_SIZE, MSG_PEEK);
                if(receiveSize <= 0)
                {
                    obj->m_logger.warn("server was disconnected");
                    obj->disconn();

                    if(data != nullptr)
                    {
                        delete[] data;
                        data = nullptr;
                    }

                    if(disconCallback)
                        disconCallback(0, obj->m_userData);
                }
                else if(receiveSize >= __BUFFER_SIZE)
                {
                    recv(socket, packet, __BUFFER_SIZE, 0);
                    if(packet->accumulatedSize == packet->currentSize)
                        data = new unsigned char[packet->totalSize];

                    memcpy(data + packet->accumulatedSize - packet->currentSize, packet->data, packet->currentSize);

                    if(packet->totalSize == packet->accumulatedSize)
                    {
                        if(receiveCallback)
                            receiveCallback(receiveSize, data, obj->m_userData);

                        if(data != nullptr)
                        {
                            delete[] data;
                            data = nullptr;
                        }
                    }
                }
                else
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
        m_logger.warn("aleady server is started");
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

    return result;
}

Client::~Client()
{
    _init();

}