#include <iostream>
#include <fstream>
#include <sys/unistd.h>
#include "tcp.h"

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
        std::cout << "Client Receive" << std::endl;

    file.close();
    return;
}

void onDisconn(char *ip, void *userData)
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
    server.setPort(2935);
    server.start(onReceive, onDisconn, &srv);

    getchar();

    int cli = 456;
    Client client;
    // client.conn("127.0.0.1", 2932, onReceive, onDisconn, &cli);

    //* 파일전송 완료
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
    //     client.sendTo(pf, size);
    // }

    //? 클라이언트 연결/끊기 무한 반복
    for(int i=0; i<1000000; i++)
    {
        getchar();
        client.conn("127.0.0.1", 2935, onReceive, onDisconn, &cli);
        client.disconn();
    }


    //? 같은 스레드에서 락 두 번 걸면 털림?
    // 정상 동작하는데? 데드락걸림
    
    //? 같은 스레드에서 락 없이 언락하면 털림?
    // 안털림
    
    //? cond wait에서 락걸고 진행중인데 또 락걸면 털림?
    // 안털림
    
    //* VC++ 에서 이중 락 체크해 주는 것이었음
    return 0;
}