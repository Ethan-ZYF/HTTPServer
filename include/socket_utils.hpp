#ifndef SOCKET_UTILS_HPP
#define SOCKET_UTILS_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdexcept>
#include <string>
#include "spdlog/spdlog.h"

inline size_t check_error(const char* msg, ssize_t res) {
    if (res == -1) {
        spdlog::error("{}: {}", msg, strerror(errno));
        throw std::runtime_error(msg);
    }
    return res;
}

#define CHECK_CALL(func, ...) check_error(#func, func(__VA_ARGS__))

struct socket_address_fatptr {
    sockaddr* m_addr;
    socklen_t m_addrlen;
};

struct socket_address_storage {
    union {
        sockaddr m_addr;
        sockaddr_storage m_addr_storage;
    };

    socklen_t m_addrlen = sizeof(m_addr_storage);

    operator socket_address_fatptr() {
        return {&m_addr, m_addrlen};
    }
};

struct address_resolved_entry {
    addrinfo* m_curr = nullptr;

    socket_address_fatptr get_address() {
        return {m_curr->ai_addr, m_curr->ai_addrlen};
    }

    int create_socket() const {
        int socket_fd = CHECK_CALL(socket, m_curr->ai_family, m_curr->ai_socktype, m_curr->ai_protocol);
        return socket_fd;
    }

    int create_socket_and_bind() {
        int socket_fd = create_socket();
        socket_address_fatptr address = get_address();
        CHECK_CALL(bind, socket_fd, address.m_addr, address.m_addrlen);
        return socket_fd;
    }

    [[nodiscard]] bool next_entry() {
        m_curr = m_curr->ai_next;
        return m_curr != nullptr;
    }
};

struct address_resolver {
    addrinfo* m_head = nullptr;

    address_resolver() = default;

    address_resolver(address_resolver&& other) {
        m_head = other.m_head;
        other.m_head = nullptr;
    }

    ~address_resolver() {
        if (m_head) {
            freeaddrinfo(m_head);
        }
    }

    address_resolved_entry resolve(const std::string& hostname, const std::string& port) {
        addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        int res = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &m_head);
        if (res != 0) {
            spdlog::error("getaddrinfo: {}", gai_strerror(res));
            throw std::runtime_error("getaddrinfo");
        }
        return {m_head};
    }
};

#endif  // SOCKET_UTILS_HPP