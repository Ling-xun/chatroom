#pragma once

#include <string>

// config.h
//
// 集中声明聊天室运行配置。配置读取优先使用环境变量，环境变量不存在或非法时使用默认值。

// 服务端监听配置。
//
// port 控制 TCP 监听端口；backlog 是 listen 队列长度，用于限制内核可暂存的待 accept 连接数。
struct ServerConfig {
    int port = 8080;
    int backlog = 5;
};

// 客户端连接配置。
//
// 客户端默认连接本机 8080 端口，测试时可以通过环境变量指向其他服务端地址。
struct ClientConfig {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;
};

// MySQL 持久化配置。
//
// 服务端启动时读取这组配置并尝试连接数据库；连接失败不会阻止聊天室继续运行。
struct MySQLConfig {
    std::string host = "localhost";
    std::string user = "chatuser";
    std::string password = "123456";
    std::string database = "chatroom";
    unsigned int port = 3306;
};

// 从环境变量读取服务端配置：
// CHAT_SERVER_PORT、CHAT_SERVER_BACKLOG。
ServerConfig load_server_config();

// 从环境变量读取客户端配置：
// CHAT_CLIENT_SERVER_IP、CHAT_CLIENT_SERVER_PORT。
ClientConfig load_client_config();

// 从环境变量读取 MySQL 配置：
// CHAT_MYSQL_HOST、CHAT_MYSQL_USER、CHAT_MYSQL_PASSWORD、CHAT_MYSQL_DATABASE、CHAT_MYSQL_PORT。
MySQLConfig load_mysql_config();
