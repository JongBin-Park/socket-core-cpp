#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include "tcp.h"

std::mutex mutex;

static void dummy()
{
    mutex.lock();

    return;
}

void onReceive(int size, unsigned char *data, void *userData)
{

    return;
}

void onDisconn(int idx, void *userData)
{

    return;
}

int main()
{
    // Server server;
    // server.setPort(2932);
    // server.start(onReceive, onDisconn, nullptr);

    //? 같은 스레드에서 락 두 번 걸면 털림?
    // 정상 동작하는데? 데드락걸림
    std::thread th([]() { mutex.lock(); });
    th.detach();
    mutex.lock();
    getchar();
    return 0;
}