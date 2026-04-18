#pragma once
#include <string>
void broadcast_msg(int sender_fd, const std::string& msg);
std::string get_current_time();
