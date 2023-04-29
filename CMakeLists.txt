cmake_minimum_required(VERSION 3.25)
project(OS_Ex3 C)

set(CMAKE_C_STANDARD 99)

add_executable(OS_Ex3 stnc.c tcpUtils.h tcpUtils.c utils.h utils.c udpUtils.h udpUtils.c mmapUtils.h mmapUtils.c pipeUtils.c udsUtils.c)
