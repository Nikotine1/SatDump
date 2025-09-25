#include "graphite.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>

namespace {
    // Global config
    std::string g_server;
    int g_port = 0;
    bool g_debug = false;

    // Networking
    int g_sockfd = -1;

    // Threading
    std::mutex g_mutex;
    std::condition_variable g_cv;
    std::queue<std::string> g_queue;
    std::thread g_worker;
    std::atomic<bool> g_running{false};

    // Internal helpers
    void log_debug(const char *fmt, ...) {
        if (!g_debug) return;
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }

    int connect_graphite_unlocked() {
        if (g_server.empty() || g_port <= 0) return -1;

        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("graphite: socket()");
            return -1;
        }

        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port   = htons(g_port);
        if (::inet_pton(AF_INET, g_server.c_str(), &sa.sin_addr) <= 0) {
            fprintf(stderr, "graphite: invalid address %s\n", g_server.c_str());
            ::close(fd);
            return -1;
        }

        if (::connect(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) < 0) {
            perror("graphite: connect()");
            ::close(fd);
            return -1;
        }

        int opt = 1;
        (void)::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

        g_sockfd = fd;
        log_debug("graphite: connected to %s:%d\n", g_server.c_str(), g_port);
        return g_sockfd;
    }

    void close_graphite_unlocked() {
        if (g_sockfd >= 0) {
            ::close(g_sockfd);
            g_sockfd = -1;
        }
    }

    void worker_thread() {
        std::unique_lock<std::mutex> lock(g_mutex);

        while (g_running.load()) {
            if (g_queue.empty()) {
                g_cv.wait(lock); // wait for new messages or stop
                continue;
            }

            auto message = std::move(g_queue.front());
            g_queue.pop();

            // release lock while doing I/O
            lock.unlock();

            if (g_sockfd < 0 && connect_graphite_unlocked() < 0) {
                // couldn't connect, drop message silently
            } else {
                ssize_t ret = ::send(g_sockfd, message.c_str(), message.size(), 0);
                if (ret <= 0) {
                    if (errno == EPIPE || errno == ECONNRESET) {
                        close_graphite_unlocked();
                        if (connect_graphite_unlocked() >= 0) {
                            (void)::send(g_sockfd, message.c_str(), message.size(), 0);
                        }
                    } else {
                        perror("graphite: send()");
                    }
                }
            }

            lock.lock();
        }

        // cleanup
        close_graphite_unlocked();
    }
} // namespace

// Public API
void set_graphite_server(const char *server, int port) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_server = server ? server : "";
    g_port   = port;
}

void set_graphite_debug(bool enable) {
    g_debug = enable;
}

void graphite_start() {
    bool expected = false;
    if (g_running.compare_exchange_strong(expected, true)) {
        g_worker = std::thread(worker_thread);
    }
}

void graphite_stop() {
    bool expected = true;
    if (g_running.compare_exchange_strong(expected, false)) {
        g_cv.notify_all();
        if (g_worker.joinable())
            g_worker.join();
    }
}

void send_to_graphite(const std::string &message) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_running) return; // ignore if not started
        g_queue.push(message);
    }
    g_cv.notify_one();
}
