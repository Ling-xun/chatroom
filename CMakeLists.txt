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

add_executable(protocol_test_client
    tests/protocol_test_client.cpp
)

target_include_directories(protocol_test_client PRIVATE include)
target_link_libraries(protocol_test_client PRIVATE Threads::Threads)

target_include_directories(server PRIVATE include)
target_include_directories(client PRIVATE include)

target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client PRIVATE Threads::Threads)