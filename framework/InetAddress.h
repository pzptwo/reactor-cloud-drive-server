#pragma once 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>

class InetAddress
{
    private:
        struct sockaddr_in addr_;//#include <arpa/inet.h>,需要这个头文件
    public:
        InetAddress(const std::string &ip,uint16_t port);
        InetAddress(const sockaddr_in addr);
        InetAddress();
        ~InetAddress();

        const char *ip() const;
        uint16_t port() const;
        const sockaddr *addr() const;
        void setaddr(sockaddr_in clientaddr);
};