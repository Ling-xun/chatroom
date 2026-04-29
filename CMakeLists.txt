cmake_minimum_required(VERSION 3.16)

project(ChatRoom)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(server
    src/server/main.cpp
    src/server/chat_server.cpp
    src/server/client_manager.cpp
    src/server/event_handler.cpp
    src/server/command_handler.cpp
)

add_executable(client
    src/client/main.cpp
    src/client/chat_client.cpp
)

target_include_directories(server PRIVATE include)
target_include_directories(client PRIVATE include)

target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client PRIVATE Threads::Threads)