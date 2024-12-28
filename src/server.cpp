#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>
#ifdef __linux__
#include <sys/epoll.h>
#elif __APPLE__
#include <sys/event.h>
#endif
#include "http_utils.hpp"
#include "socket_utils.hpp"
#include "spdlog/spdlog.h"

std::vector<std::thread> pool;

int main() {
    address_resolver resolver;
    auto entry = resolver.resolve("localhost", "8080");
    int listen_fd = entry.create_socket_and_bind();
    spdlog::info("Listening on port 8080");
    spdlog::info("listen_fd: {}, SOMAXCONN: {}", listen_fd, SOMAXCONN);
    CHECK_CALL(listen, listen_fd, SOMAXCONN);
    socket_address_storage addr;
    while (true) {
        int connid = CHECK_CALL(accept, listen_fd, &addr.m_addr, &addr.m_addrlen);
        spdlog::info("Accepted connection from {}", inet_ntoa(((sockaddr_in*)&addr.m_addr)->sin_addr));
        pool.emplace_back([connid] {  // Separate thread for each connection
            char buffer[1024];
            http_request_parser<http11_header_parser> req_parser;
            do {
                spdlog::info("Reading from connection {}", connid);
                size_t sz = CHECK_CALL(read, connid, buffer, sizeof(buffer));
                spdlog::info("Received {} bytes", sz);
                req_parser.push_chunk(std::string(buffer, sz));
            } while (req_parser.request_finished() == false);
            auto header = req_parser.header();
            auto body = req_parser.body();
            spdlog::info("Received request header: {}", header);
            spdlog::info("Received request body: {}", body);

            std::string reply = "HTTP/1.1 200 OK\r\nContent-Length: ";
            reply += std::to_string(body.size());
            reply += "\r\n\r\n";
            reply += body;

            spdlog::info("Replying with: {}", reply);
            CHECK_CALL(write, connid, reply.data(), reply.size());
            CHECK_CALL(close, connid);
        });
    }
    for (auto& thread : pool) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    return 0;
}