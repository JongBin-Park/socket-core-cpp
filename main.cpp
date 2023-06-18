#include <iostream>
#include <fstream>
#include <sys/unistd.h>
#include "tcp.h"
#include <fmt/format.h>
#define PORT 2341

void onReceive(int size, unsigned char *data, void *userData)
{
    static int index = 0;
    int separator = *(int*)userData;
    std::ofstream file;
    file.open(std::to_string(index++) + ".exe", std::ios::binary);
    if(separator == 123)
    {
        std::cout << "Server Receive size:" << size << std::endl;
        file.write((char*)data, size);
    }
    else
    {
        std::cout << "Client Receive size:" << size << std::endl;
        file.write((char*)data, size);
    }

    file.close();
    return;
}

void onDisconn(const char *ip, void *userData)
{
    int separator = *(int*)userData;

    if(separator == 123)
        std::cout << "Server Disconnect callback discon ip:" << ip << std::endl;
    else
        std::cout << "Client Disconnect callback" << std::endl;

    return;
}

int main()
{
    int srv = 123;
    Server server;
    server.setPort(PORT);
    server.start(onReceive, onDisconn, &srv);

    getchar();

    int cli = 456;
    Client client;

    //// 파일전송 완료
    // client.conn("127.0.0.1", PORT, onReceive, onDisconn, &cli);
    // std::ifstream file;
    // file.open("SSMS-Setup-KOR.exe", std::ios::binary);
    // file.seekg(0, std::ios::end);
    // unsigned long long size = file.tellg();
    // std::cout << size << std::endl;
    // file.seekg(0, std::ios::beg);
    // unsigned char *pf = new unsigned char[size];
    // file.read((char*)pf, size);
    // file.close();
    // while(getchar() != 'x')
    // {
    //     // server.sendToAll(pf, size);
    //     client.sendTo(pf, size);
    // }
    // delete[] pf;

    //// 클라이언트 연결/끊기 무한 반복
    // // while(getchar() != 'x')
    // while(true)
    // {
    //     client.conn("127.0.0.1", PORT, onReceive, onDisconn, &cli);
    //     client.disconn();
    //     usleep(1000);
    // }
    
    return 0;
}