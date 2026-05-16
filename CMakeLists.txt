cmake_minimum_required(VERSION 3.16)

project(ChatRoom)

# 使用 C++17 编译整个项目，并要求编译器必须支持该标准。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 客户端和测试程序使用线程库，例如读写线程并行处理网络输入输出。
find_package(Threads REQUIRED)

# 聊天服务器：负责监听客户端连接、管理在线用户、处理命令并保存消息到 MySQL。
add_executable(server
    src/common/config.cpp
    src/server/main.cpp
    src/server/chat_server.cpp
    src/server/client_manager.cpp
    src/server/event_handler.cpp
    src/server/command_handler.cpp
    src/storage/mysql_storage.cpp
)

# 聊天客户端：负责连接服务器，并在本地处理用户输入和服务器消息。
add_executable(client
    src/common/config.cpp
    src/client/main.cpp
    src/client/chat_client.cpp
)

# 协议测试客户端：用于手动或自动验证消息拆包、粘包等协议边界。
add_executable(protocol_test_client
    src/common/config.cpp
    tests/protocol_test_client.cpp
)

# 所有需要访问项目头文件的目标都加入 include 目录。
target_include_directories(protocol_test_client PRIVATE include)
target_link_libraries(protocol_test_client PRIVATE Threads::Threads)

target_include_directories(server PRIVATE include)
target_include_directories(client PRIVATE include)

target_link_libraries(client PRIVATE Threads::Threads)

# 服务器除了线程库，还需要链接 MySQL C 客户端库以写入聊天记录。
target_link_libraries(server PRIVATE Threads::Threads mysqlclient)
