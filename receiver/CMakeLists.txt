cmake_minimum_required(VERSION 3.20)
project(psia_reciever_udp)

set(CMAKE_C_STANDARD 99)

add_executable(psia_reciever_udp
        main.c
        packet.c
        packet.h
        receiver.c
        receiver.h
        sender.c
        sender.h
        utils.c
        utils.h
        logger.h
)

# Link Winsock2 and OpenSSL (for SHA-256)
target_link_libraries(psia_reciever_udp ws2_32 libcrypto libssl)
