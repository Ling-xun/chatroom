#pragma once

#include <mysql/mysql.h>

#include <string>

// 封装聊天室消息的 MySQL 持久化逻辑。
//
// 该类只负责建立数据库连接和保存聊天消息，不负责业务格式化、消息广播或客户端管理。
class MySQLStorage {
public:
    MySQLStorage();
    ~MySQLStorage();

    // 建立到 MySQL 的连接，成功返回 true，失败时打印错误并返回 false。
    bool connect();

    // 保存一条聊天消息。
    //
    // sender 表示发送者昵称，content 表示消息正文，message_type 用于区分消息类型。
    // 当前调用方主要传入 "chat"，后续可以扩展为 system、private 等类型。
    bool save_message(const std::string& sender,
                      const std::string& content,
                      const std::string& message_type);

private:
    // MySQL C API 的连接句柄；connect() 成功后持有，析构时释放。
    MYSQL* conn_;
};

// 全局存储实例，供服务器各模块统一写入聊天消息。
extern MySQLStorage g_mysql_storage;
