#include "common/config.h"

#include <cerrno>
#include <cstdlib>

// config.cpp
//
// 该文件集中实现运行配置读取逻辑。所有配置都遵循同一个约定：
// - 环境变量存在且合法时使用环境变量。
// - 环境变量缺失、为空或格式非法时保留结构体中的默认值。
// 这样本地开发可以零配置启动，测试和部署时也能通过环境变量覆盖关键参数。

namespace {

// 读取字符串环境变量。
//
// 空字符串按未配置处理，避免调用方拿到没有实际意义的配置值。
std::string get_env_string(const char* name, const std::string& default_value) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return default_value;
    }

    return value;
}

// 读取整数环境变量，并校验它落在调用方指定的闭区间内。
//
// strtol 可以区分“没有数字”“有多余字符”和“转换溢出”等情况；任一异常都回退默认值。
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

// 读取无符号整数配置。
//
// 当前端口号类型使用 unsigned int，但环境变量解析仍复用 get_env_int 的范围校验逻辑。
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

// 加载服务端监听配置。
//
// 端口范围限制为 1-65535，backlog 允许 1-4096，避免把明显无效的环境变量传给 socket API。
ServerConfig load_server_config() {
    ServerConfig config;
    config.port = get_env_int("CHAT_SERVER_PORT", config.port, 1, 65535);
    config.backlog = get_env_int("CHAT_SERVER_BACKLOG", config.backlog, 1, 4096);
    return config;
}

// 加载客户端连接配置。
//
// server_ip 不做格式校验，后续由 inet_addr/connect 负责判定地址是否可用。
ClientConfig load_client_config() {
    ClientConfig config;
    config.server_ip = get_env_string("CHAT_CLIENT_SERVER_IP", config.server_ip);
    config.server_port = get_env_int("CHAT_CLIENT_SERVER_PORT", config.server_port, 1, 65535);
    return config;
}

// 加载 MySQL 连接配置。
//
// 只有端口需要数值校验；主机、用户名、密码和数据库名保持字符串原样传给 MySQL C API。
MySQLConfig load_mysql_config() {
    MySQLConfig config;
    config.host = get_env_string("CHAT_MYSQL_HOST", config.host);
    config.user = get_env_string("CHAT_MYSQL_USER", config.user);
    config.password = get_env_string("CHAT_MYSQL_PASSWORD", config.password);
    config.database = get_env_string("CHAT_MYSQL_DATABASE", config.database);
    config.port = get_env_uint("CHAT_MYSQL_PORT", config.port, 1, 65535);
    return config;
}
