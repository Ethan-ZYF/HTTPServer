#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include "spdlog/spdlog.h"

int main() {
    struct addrinfo* addrinfo;
    int res = getaddrinfo("localhost", "80", nullptr, &addrinfo);
    if (res != 0) {
        perror("getaddrinfo");
        return 1;
    }

    spdlog::info("addrinfo->ai_family  : {}", addrinfo->ai_family);
    spdlog::info("addrinfo->ai_socktype: {}", addrinfo->ai_socktype);
    spdlog::info("addrinfo->ai_protocol: {}", addrinfo->ai_protocol);

    int sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        return 1;
    }

    return 0;
}