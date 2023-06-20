# socket-core-cpp

## Requirement

* [fmt](https://github.com/fmtlib/fmt)

## To used

1. install fmt library
2. add "tcp.h" header
3. create object "Server" or "Client"
    1. Server
        1. setPort
        2. start (receive data)
        3. sendTo/sendAll
    2. Client
        1. conn (receive data)
        2. sendTo

detail is referenced to main.cpp.