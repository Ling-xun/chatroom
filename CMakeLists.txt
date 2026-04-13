cmake_minimum_required(VERSION 3.16)

project(ChatRoom)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(server
    src/main.cpp
    src/chat_server.cpp
)

add_executable(client
    src/client.cpp
)

target_include_directories(server PRIVATE include)
target_include_directories(client PRIVATE include)

target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client PRIVATE Threads::Threads)