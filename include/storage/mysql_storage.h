#pragma once

#include <mysql/mysql.h>

#include <string>


class MySQLStorage {
public:
    MySQLStorage();
    ~MySQLStorage();

    bool connect();
    bool save_message(const std::string& sender,
                      const std::string& content,
                      const std::string& message_type);

private:
    MYSQL* conn_;
};

extern MySQLStorage g_mysql_storage;