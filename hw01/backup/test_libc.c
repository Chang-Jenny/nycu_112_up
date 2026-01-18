#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s hostname\n", argv[0]);
        return 1;
    }

    const char *hostname = argv[1];

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int status;

    // 清零并初始化 hints 结构
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    // 任何可用的协议
    hints.ai_socktype = SOCK_STREAM; // 使用流式套接字

    // 调用 getaddrinfo 函数解析主机名
    status = getaddrinfo(hostname, NULL, &hints, &result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // 遍历结果并打印 IP 地址
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        void *addr;
        char ipstr[INET6_ADDRSTRLEN];

        // 将地址转换为字符串格式
        if (rp->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
            addr = &(ipv4->sin_addr);
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        // 将地址转换为可读格式
        inet_ntop(rp->ai_family, addr, ipstr, sizeof(ipstr));
        printf("IP address: %s\n", ipstr);
    }

    // 释放结果链表
    freeaddrinfo(result);

    return 0;
}