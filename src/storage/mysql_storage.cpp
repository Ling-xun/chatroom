#include "storage/mysql_storage.h"

#include <iostream>

MySQLStorage g_mysql_storage;

namespace {

std::string escape_string(MYSQL* conn, const std::string& input) {
    std::string output;
    output.resize(input.size() * 2 + 1);

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

MySQLStorage::MySQLStorage() : conn_(nullptr) {}

MySQLStorage::~MySQLStorage() {
    if (conn_ != nullptr) {
        mysql_close(conn_);
    }
}

bool MySQLStorage::connect() {
    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr) {
        std::cerr << "mysql_init failed" << std::endl;
        return false;
    }

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

    mysql_set_character_set(conn_, "utf8mb4");

    std::cout << "mysql connected" << std::endl;
    return true;
}

bool MySQLStorage::save_message(const std::string& sender,
                                const std::string& content,
                                const std::string& message_type) {
    if (conn_ == nullptr) {
        return false;
    }
std::string safe_sender = escape_string(conn_, sender);
std::string safe_content = escape_string(conn_, content);
std::string safe_type = escape_string(conn_, message_type);

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