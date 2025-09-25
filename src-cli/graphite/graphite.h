#pragma once

#include <cstddef>   // for size_t
#include <string>    // for string

// Declarations (interface)
int connect_graphite(void);
void send_to_graphite(const std::string &message);

// You might also want to expose setters for server/port
void set_graphite_server(const char *server, int port);
void set_graphite_debug(bool enable);