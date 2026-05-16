#pragma once

#include <string>

// config.h
//
// 集中声明聊天室运行配置。配置读取优先使用环境变量，环境变量不存在或非法时使用默认值。

struct ServerConfig {
    int port = 8080;
    int backlog = 5;
};

struct ClientConfig {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;
};

struct MySQLConfig {
    std::string host = "localhost";
    std::string user = "chatuser";
    std::string password = "123456";
    std::string database = "chatroom";
    unsigned int port = 3306;
};

ServerConfig load_server_config();
ClientConfig load_client_config();
MySQLConfig load_mysql_config();
