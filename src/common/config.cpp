#include "common/config.h"

#include <cerrno>
#include <cstdlib>

namespace {

std::string get_env_string(const char* name, const std::string& default_value) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return default_value;
    }

    return value;
}

int get_env_int(const char* name, int default_value, int min_value, int max_value) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return default_value;
    }

    errno = 0;
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        return default_value;
    }

    if (parsed < min_value || parsed > max_value) {
        return default_value;
    }

    return static_cast<int>(parsed);
}

unsigned int get_env_uint(const char* name,
                          unsigned int default_value,
                          unsigned int min_value,
                          unsigned int max_value) {
    int value = get_env_int(
        name,
        static_cast<int>(default_value),
        static_cast<int>(min_value),
        static_cast<int>(max_value)
    );

    return static_cast<unsigned int>(value);
}

}  // namespace

ServerConfig load_server_config() {
    ServerConfig config;
    config.port = get_env_int("CHAT_SERVER_PORT", config.port, 1, 65535);
    config.backlog = get_env_int("CHAT_SERVER_BACKLOG", config.backlog, 1, 4096);
    return config;
}

ClientConfig load_client_config() {
    ClientConfig config;
    config.server_ip = get_env_string("CHAT_CLIENT_SERVER_IP", config.server_ip);
    config.server_port = get_env_int("CHAT_CLIENT_SERVER_PORT", config.server_port, 1, 65535);
    return config;
}

MySQLConfig load_mysql_config() {
    MySQLConfig config;
    config.host = get_env_string("CHAT_MYSQL_HOST", config.host);
    config.user = get_env_string("CHAT_MYSQL_USER", config.user);
    config.password = get_env_string("CHAT_MYSQL_PASSWORD", config.password);
    config.database = get_env_string("CHAT_MYSQL_DATABASE", config.database);
    config.port = get_env_uint("CHAT_MYSQL_PORT", config.port, 1, 65535);
    return config;
}
