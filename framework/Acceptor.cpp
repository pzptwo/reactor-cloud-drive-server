#include "Acceptor.h"
#include <utility>
#include <cerrno>


//
Acceptor::Acceptor(EventLoop *loop,const std::string ip,const uint16_t port):loop_(loop),servsock_(createnonblocking()),acceptchannel_(loop_,servsock_.fd())
{
    //servsock_=new Socket(createnonblocking());
    servsock_.setkeepalive(true);
    servsock_.setreuseaddr(true);
    servsock_.setreuseport(true);
    servsock_.settcpnodelay(true);
    InetAddress servaddr(ip,port);
    servsock_.bind(servaddr);
    servsock_.listen(4096);   // backlog 提高到 4096：压测 1000+ 并发连入时避免 SYN 被丢
    //Epoll ep;
    //ep.addfd(servsock.fd(),EPOLLIN);
    //EventLoop loop;
    //acceptchannel_=new Channel(loop_,servsock_->fd());
    acceptchannel_.setreadback(std::bind(&Acceptor::newConnection,this));
    acceptchannel_.enablereading();
}  
Acceptor::~Acceptor()
{
    //delete servsock_;
    //delete acceptchannel_;
}


//这里是新的连接请求,实在servsock 这个管道符里面
void Acceptor::newConnection()
{
    // 一次 EPOLLIN 事件把待 accept 的连接全部取完（循环到 EAGAIN），
    // 避免 backlog 堆积导致新连接 SYN 被内核丢弃（压测 1000 并发时出现超时）
    while (true)
    {
        InetAddress clientaddr;
        int clientfd = servsock_.accept(clientaddr);
        if (clientfd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // 取完了
            if (errno == EINTR) continue;                        // 信号打断，重试
            // fd 耗尽(EMFILE)等：拿 -1 建 Socket 会导致后续 fd%threadNum_ 越界崩溃，直接放弃本次
            break;
        }
        std::unique_ptr<Socket> clientsock(new Socket(clientfd));//(相当于传进来clientfd)
        clientsock->setipport(clientaddr.ip(), clientaddr.port());
        newConnectioncb_(std::move(clientsock));
    }
}

void Acceptor::setnewConnectioncb(std::function<void(std::unique_ptr<Socket>)> fn)
{
    newConnectioncb_=fn;
}