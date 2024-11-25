cmake_minimum_required(VERSION 3.25)
project(TicTacToe)

set(CMAKE_CXX_STANDARD 20)


SET(CMAKE_CXX_COMPILER g++)


add_executable(
        server
        lib/db.cpp
        server.cpp)

add_executable(
        client
        lib/game.cpp
        client.cpp
)
