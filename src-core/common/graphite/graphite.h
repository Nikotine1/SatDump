#pragma once
#include <string>

// Configure the Graphite client (must call before use)
void set_graphite_server(const char *server, int port);
void set_graphite_debug(bool enable);

// Start / stop background sender thread
void graphite_start();
void graphite_stop();

// Non-blocking enqueue of a message
void send_to_graphite(const std::string &message);
