#pragma once

#include <cstddef>   // for size_t
#include <string>    // for string

int connect_graphite(void);
void send_to_graphite(const std::string &message);

// Setters for server/port
void set_graphite_server(const char *server, int port);
void set_graphite_debug(bool enable);