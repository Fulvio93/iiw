cmake_minimum_required(VERSION 3.3)
project(iiw)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

set(CLIENT client.c)
set(SERVER server.c)
add_executable(server ${SERVER})
add_executable(client ${CLIENT})
