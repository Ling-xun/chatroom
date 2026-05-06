#include "storage/mysql_storage.h"

#include <iostream>

// mysql_storage.cpp
//
// 该文件封装聊天室消息写入 MySQL 的细节：
// 1. 建立和释放 MySQL C API 连接。
// 2. 对用户输入进行 SQL 转义。
// 3. 将聊天消息插入 messages 表。
//
// 聊天业务层只需要调用 g_mysql_storage.save_message(...)，不需要关心具体 SQL。

// 全局 MySQL 存储对象，供服务器事件处理逻辑保存聊天消息。
// 当前服务器主循环是单线程事件处理模型，因此这里暂时没有额外加锁。
MySQLStorage g_mysql_storage;

namespace {

// 对写入 SQL 的字符串做转义。
//
// 客户端发来的昵称和消息内容可能包含单引号、反斜杠等特殊字符，如果直接拼接进
// SQL 语句，可能导致语句格式错误，甚至产生 SQL 注入风险。mysql_real_escape_string
// 会基于当前连接的字符集规则生成安全字符串，所以调用前需要保证 conn 是有效连接。
std::string escape_string(MYSQL* conn, const std::string& input) {
    std::string output;

    // MySQL 转义后的最长长度为原字符串长度的 2 倍再加结尾 '\0' 空间。
    output.resize(input.size() * 2 + 1);

    // mysql_real_escape_string 会把 input 中需要转义的字符写入 output，并返回实际长度。
    unsigned long len = mysql_real_escape_string(
        conn,
        output.data(),
        input.c_str(),
        input.size()
    );

    output.resize(len);
    return output;
}

}  // namespace

// 构造时只初始化连接指针，真正的数据库连接在 connect() 中建立。
MySQLStorage::MySQLStorage() : conn_(nullptr) {}

MySQLStorage::~MySQLStorage() {
    // 程序退出或全局对象析构时关闭 MySQL 连接，释放客户端库资源。
    if (conn_ != nullptr) {
        mysql_close(conn_);
    }
}

bool MySQLStorage::connect() {
    // 如果未来支持重连，这里需要先处理已有 conn_；当前程序只在启动时连接一次。
    // 创建 MySQL 连接句柄。此时只是初始化客户端结构，还没有连接到数据库服务。
    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr) {
        std::cerr << "mysql_init failed" << std::endl;
        return false;
    }

    // 连接本机 MySQL 服务，并选择 chatroom 数据库。
    // 参数依次为：连接句柄、主机、用户名、密码、数据库名、端口、Unix socket、客户端标志。
    MYSQL* result = mysql_real_connect(
        conn_,
        "localhost",
        "chatuser",
        "123456",
        "chatroom",
        3306,
        nullptr,
        0
    );

    if (result == nullptr) {
        std::cerr << "mysql_real_connect failed: "
                  << mysql_error(conn_) << std::endl;
        return false;
    }

    // 使用 utf8mb4 保存聊天内容，避免中文、表情等多字节字符写入异常。
    mysql_set_character_set(conn_, "utf8mb4");

    std::cout << "mysql connected" << std::endl;
    return true;
}

bool MySQLStorage::save_message(const std::string& sender,
                                const std::string& content,
                                const std::string& message_type) {
    // 数据库尚未连接时直接返回失败，避免对空连接执行 MySQL API。
    // 上层聊天流程不会因为保存失败而中断在线消息广播。
    if (conn_ == nullptr) {
        return false;
    }

    // 所有外部输入先转义，再拼接到 INSERT 语句中。
    std::string safe_sender = escape_string(conn_, sender);
    std::string safe_content = escape_string(conn_, content);
    std::string safe_type = escape_string(conn_, message_type);

    // 将聊天发送者、消息正文和消息类型保存到 messages 表。
    std::string query =
        "INSERT INTO messages(sender_name, content, message_type) VALUES ('" +
        safe_sender + "', '" + safe_content + "', '" + safe_type + "')";

    if (mysql_query(conn_, query.c_str()) != 0) {
        std::cerr << "mysql insert failed: "
                  << mysql_error(conn_) << std::endl;
        return false;
    }

    return true;
}
