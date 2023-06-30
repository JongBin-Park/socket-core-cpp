#include "tcp.h"

void Server::_init()
{
    m_socket = -1;
    m_isRunning = false;
    m_port = 0;
    m_rcvCallback = nullptr;
    m_disconCallback = nullptr;
    m_userData = nullptr;
    m_rcvTh = nullptr;
    m_logger.setLevel(LOG_LEVEL_DEBUG);

    m_maxfd = 0;
    m_clientCount = 0;
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

void Server::setPort(int port)
{
    m_port = port;
    m_logger.info("set port {}", port);
}

void Server::_getIpAddress(int fd, char *ret)
{
    if(ret == nullptr)
        return;
    
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(fd, (sockaddr*)&addr, &addr_size);
    memset(ret, 0, 20);
    memcpy(ret, inet_ntoa(addr.sin_addr), 20);
}

void Server::_receiveThread(Server *obj)
{
    fd_set fd;
    struct timeval tv;
    int fdNum = -1;
    
    struct sockaddr_in clientAddress;
    unsigned int addrLenth;
    
    int clientSocket = -1;
    char clientIP[20];
    int receiveSize = -1;
    ReceiveCallback receiveCallback = obj->m_rcvCallback;
    DisconCallback disconCallback = obj->m_disconCallback;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    unsigned char *data = nullptr;

    while(obj->m_isRunning)
    {
        fd = obj->m_readfds;
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        fdNum = select(obj->m_maxfd + 1, &fd, NULL, NULL, &tv);
        if(fdNum == -1) break;
        else if(fdNum == 0) continue;

        for(unsigned int i=0; i<obj->m_maxfd + 1; i++)
        {
            if(FD_ISSET(i, &fd))
            {
                if(i == obj->m_socket)
                {
                    clientSocket = accept(obj->m_socket, (sockaddr *)&clientAddress, &addrLenth);
                    if(clientSocket > 0)
                    {
                        _getIpAddress(clientSocket, clientIP);
                        FD_SET(clientSocket, &obj->m_readfds);
                        if(obj->m_maxfd < clientSocket) obj->m_maxfd = clientSocket;
                        obj->m_acceptingMutex.lock();
                        obj->m_clientList.emplace_back(clientSocket, clientIP);
                        obj->m_acceptingMutex.unlock();
                        obj->m_logger.debug("client is connected [IP:{} cur con:{}]", clientIP, ++obj->m_clientCount);

                        if(obj->m_acceptCallback)
                            obj->m_acceptCallback(clientIP, obj->m_userData);
                    }
                }
                else
                {
                    clientSocket = i;
                    do
                    {
                        receiveSize = recv(clientSocket, packet, __BUFFER_SIZE, MSG_PEEK);
                        if(receiveSize <= 0)
                        {
                            _getIpAddress(i, clientIP);
                            obj->m_logger.warn("client was disconnected [IP:{} cur con:{}]", clientIP, --obj->m_clientCount);

                            FD_CLR(clientSocket, &obj->m_readfds);
                            shutdown(clientSocket, SHUT_RDWR);
                            close(clientSocket);
                            obj->m_acceptingMutex.lock();
                            auto iter = std::find_if(obj->m_clientList.begin(), obj->m_clientList.end(), [&clientSocket](ClientInfo &info) {return info.socket == clientSocket;});
                            obj->m_clientList.erase(iter);
                            obj->m_acceptingMutex.unlock();
                                
                            if(data != nullptr)
                            {
                                delete[] data;
                                data = nullptr;
                            }

                            if(disconCallback)
                                disconCallback(clientIP, obj->m_userData);

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
                    AcceptCallback  acceptCallback,
                    void*           userData)
{
    if(!rcvCallback || !disconCallback || !acceptCallback)
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
    m_acceptCallback = acceptCallback;
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
        shutdown(m_socket, SHUT_RDWR);
        return result;
    }

    if(listen(m_socket, 5) < 0)
    {
        m_logger.error("can't listen");
        shutdown(m_socket, SHUT_RDWR);
        return result;
    }

    FD_SET(m_socket, &m_readfds);
    m_maxfd = m_socket;

    m_isRunning = true;
    m_rcvTh = new std::thread(&Server::_receiveThread, this);
    // m_rcvTh->detach();

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

    if(!m_clientList.empty())
    {
        auto iter = m_clientList.begin();
        while(iter!=m_clientList.end())
        {
            FD_CLR(iter->socket, &m_readfds);
            shutdown(iter->socket, SHUT_RDWR);
            close(iter->socket);

            iter++;
        }

        m_clientList.clear();
    }

    FD_CLR(m_socket, &m_readfds);
    shutdown(m_socket, SHUT_RDWR);
    close(m_socket);

    return false;
}

unsigned long long Server::_send(std::string ip, unsigned char *data, unsigned long long size)
{
    unsigned long long sendSize = 0;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    std::vector<ClientInfo>::iterator iter;
    int selectedSocket = -1;

    if(!ip.empty())
    {
        m_acceptingMutex.lock();
        iter = std::find_if(m_clientList.begin(), m_clientList.end(), [&ip](ClientInfo &info) -> bool {
            return strcmp(info.ipAddress, ip.c_str())==0;
        });
        m_acceptingMutex.unlock();

        selectedSocket = iter->socket;
    }

    packet->totalSize = size;
    packet->accumulatedSize = 0;
    packet->currentSize = 0;
    m_sendingMutex.lock();
    while(packet->accumulatedSize < packet->totalSize)
    {
        if(packet->totalSize - packet->accumulatedSize < __BUFFER_SIZE - sizeof(Packet))
            packet->currentSize = packet->totalSize - packet->accumulatedSize;
        else
            packet->currentSize = __BUFFER_SIZE - sizeof(Packet);

        memset(packet->data, 0, __BUFFER_SIZE - sizeof(Packet));
        memcpy(packet->data, data + packet->accumulatedSize, packet->currentSize);

        packet->accumulatedSize += packet->currentSize;

        if(ip.empty())
        {
            m_acceptingMutex.lock();
            std::vector<ClientInfo>::iterator iter = m_clientList.begin();
            while(iter!=m_clientList.end())
            {
                sendSize += send(iter->socket, packet, __BUFFER_SIZE, 0);
                iter++;
            }
            m_acceptingMutex.unlock();
        }
        else
            sendSize += send(selectedSocket, packet, __BUFFER_SIZE, 0);
    }
    m_sendingMutex.unlock();

    delete[] packet;
    return sendSize;
}

unsigned long long Server::sendTo(std::string ip, unsigned char *data, unsigned long long size)
{
    return _send(ip, data, size);
}

unsigned long long Server::sendToAll(unsigned char *data, unsigned long long size)
{
    return _send("", data, size);
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

void Client::_getIpAddress(int fd, char *ret)
{
    if(ret == nullptr)
        return;
    
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(fd, (sockaddr*)&addr, &addr_size);
    memset(ret, 0, 20);
    memcpy(ret, inet_ntoa(addr.sin_addr), 20);
}

void Client::_receiveThread(Client *obj)
{
    fd_set fd;
    struct timeval tv;
    int fdNum = -1;
    int socket = obj->m_socket;
    char serverIP[20];
    int receiveSize = -1;
    ReceiveCallback receiveCallback = obj->m_rcvCallback;
    DisconCallback disconCallback = obj->m_disconCallback;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];
    unsigned char *data = nullptr;

    while(obj->m_isConnected)
    {
        fd = obj->m_readfds;
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        fdNum = select(obj->m_maxfd + 1, &fd, NULL, NULL, &tv);
        if(fdNum == -1) break;
        else if(fdNum == 0) continue;

        if(FD_ISSET(socket, &fd))
        {
            do
            {
                receiveSize = recv(socket, packet, __BUFFER_SIZE, MSG_PEEK);
                if(receiveSize <= 0)
                {
                    _getIpAddress(socket, serverIP);
                    obj->m_logger.warn("server was disconnected");
                    shutdown(socket, SHUT_RDWR);
                    close(socket);
                    if(data != nullptr)
                    {
                        delete[] data;
                        data = nullptr;
                    }

                    if(disconCallback)
                        disconCallback(serverIP, obj->m_userData);

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
        shutdown(m_socket, SHUT_RDWR);

        return result;
    }

    FD_SET(m_socket, &m_readfds);
    m_maxfd = m_socket;

    m_isConnected = true;
    m_rcvTh = new std::thread(&Client::_receiveThread, this);
    // m_rcvTh->detach();

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

    FD_CLR(m_socket, &m_readfds); 
    shutdown(m_socket, SHUT_RDWR);
    close(m_socket);

    return result;
}

unsigned long long Client::sendTo(unsigned char *data, unsigned long long size)
{
    unsigned long long sendSize = 0;
    Packet *packet = (Packet*)new unsigned char[__BUFFER_SIZE];

    packet->totalSize = size;
    packet->accumulatedSize = 0;
    packet->currentSize = 0;
    m_sendingMutex.lock();
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
    m_sendingMutex.unlock();
    delete[] packet;
    return sendSize;
}

Client::~Client()
{
    if(m_isConnected)
        disconn();
    _init();
}