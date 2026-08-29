#pragma once 

#include <cerrno>
#include <endian.h>
#include <fcntl.h>
#include <iterator>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>          
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>      // TCP_NODELAY需要包含这个头文件。
#include <iostream>
#include "EventLoop.h"
#include "InetAddress.h"
#include "Acceptor.h"
#include "Socket.h"
#include "Connection.h"
#include <map>
#include "ThreadPool.h"
#include <memory>
#include <mutex>

//TCP 网络服务类
class TcpServer
{
    private:
       //一个Tcpserver对应多个eventloop（多线程） ，所以就是TcpServer管理loop
        std::unique_ptr<EventLoop> mainloop_;  //为了好理解，好对应从事件循环，用堆内存。  
        Acceptor acceptor_; //一个Tcpserver对应一个Accept对象
        std::map<int,spConnection> conns_;   //一个Tcpserver对应多个Connection对象，存放在map容器里面
        int threadNum_; //线程池的大小。（还是这里管理）
        ThreadPool threadpool_;    //创建线程池，这里要把线程创建出来
        std::vector<std::unique_ptr<EventLoop>> subloop_;  //创建出来存放的容器。
        

        std::function<void (spConnection)> newConnectioncb_;
        std::function<void (spConnection)> closecb_;
        std::function<void (spConnection)> errorcb_;
        std::function<void (spConnection,std::string &)> slovemessagecb_;
        std::function<void (spConnection)> sendCompletecb_;
        std::function<void (EventLoop *)> epolltimeoutcb_;

        std::mutex mmtex_;
    public:
        //由于要完成bind所以需要传参
        TcpServer(const std::string ip,const uint16_t port,int threadNum=3);  //第三个参数初始化线程池
        ~TcpServer();

        void start();   //调用loop的run,相当于*loop_的接口，裕兴事件循环
        void newConnection(std::unique_ptr<Socket> clientsock);   //处理新连接上来的
        void closecallback(spConnection conn);   //我感觉取名有点问题
        void errorcallback(spConnection conn);
        void slovemessage(spConnection conn,std::string &message );

        //对于最上层，要知道，数据已经发送完毕
        void sendComplete(spConnection conn);

        //对于最上层，要知道，是否超时,当Channel为空时，而且因为在eventloop里面，所以要说明是哪一个loop
        void epolltimeout(EventLoop *loop);

        void setnewConnectioncb(std::function<void (spConnection)>fn);
        void setclosecb(std::function<void (spConnection)>fn);
        void seterrorcb(std::function<void (spConnection)>fn);
        void setslovemessagecb(std::function<void (spConnection,std::string &)>fn);
        void setsendCompletecb(std::function<void (spConnection)>fn);
        void setepolltimeoutcb(std::function<void (EventLoop *)>fn);

        //这里也要删除map,Connection,但是Tcpserver不直接，只能在EventLoop里面进行，采用回调
        void removeconnection(int fd);

        //停止IO（线程池）
        void stop();
};