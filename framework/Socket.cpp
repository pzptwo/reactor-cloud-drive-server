#include "Socket.h"
#include <unistd.h>


//创建一个非阻塞的socket;
int createnonblocking()
{
    int listenfd=socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0);
    if(listenfd==-1)
    {
        printf("socket() failed");
        exit(1);
    }
    return listenfd;
}
//构造函数，传入一个准备好的fd
Socket::Socket(int fd):fd_(fd)
{

}
//在析构函数中关闭fd
Socket::~Socket()
{
    ::close(fd_);
}
int Socket::fd() const
{
    return fd_;
}
//返回ip_
std::string Socket::ip()
{
    return ip_;
}

uint16_t Socket::port()
{
    return port_;
}
void Socket::setreuseaddr(bool on)
{
    int opt=on?1:0;
    ::setsockopt(fd_,SOL_SOCKET,SO_REUSEADDR,&opt,static_cast<socklen_t>(sizeof opt));
}
void Socket::setreuseport(bool on)
{
    int opt=on?1:0;
    ::setsockopt(fd_,SOL_SOCKET,SO_REUSEPORT,&opt,static_cast<socklen_t>(sizeof opt));
}
void Socket::settcpnodelay(bool on)
{
    int opt=on?1:0;
    ::setsockopt(fd_,SOL_SOCKET,TCP_NODELAY,&opt,static_cast<socklen_t>(sizeof opt));
}

void Socket::setkeepalive(bool on)
{
    int opt=on?1:0;
    ::setsockopt(fd_,SOL_SOCKET,SO_KEEPALIVE,&opt,static_cast<socklen_t>(sizeof opt));
}
//服务端的socket将调用此函数
void Socket::bind(const InetAddress &servaddr)
{
    if((::bind(fd_,servaddr.addr(),sizeof(sockaddr)))==-1)
    {
        printf("bind fail\n");
        close(fd_);
        exit(-1);
    }
    
    setipport(servaddr.ip(),servaddr.port());
} 

void Socket::listen(int nn)
{
    if((::listen(fd_,nn))==-1)
    {
        perror("listen fail\n");
        close(fd_);
        exit(-1);
    }
}
//服务端的socket将调用此函数
int Socket::accept(InetAddress &clientaddr)
{
    struct sockaddr_in perraddr;
    socklen_t len=sizeof(perraddr);
    //两个函数的区别accept与accept4，匹配
    int clientfd=::accept4(fd_,(struct sockaddr *)&perraddr,&len,SOCK_NONBLOCK);
    //打印一下日志
    //InetAddress clientaddr(perraddr);//为啥不能，int Socket::accept(InetAddress &clientaddr（参数？？？）)
    clientaddr.setaddr(perraddr);
    return clientfd;
}

//相当于设置接口，外面可以调用我的Socket里面的成员变量
void Socket::setipport(const std::string &ip,uint16_t port)
{
    ip_=ip;
    port_=port;
}