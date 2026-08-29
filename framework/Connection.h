#pragma once 
#include "EventLoop.h"
#include "Socket.h"
#include "Channel.h"
#include <cstddef>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include "Buffer.h"
#include <memory>
#include <atomic>
#include <memory>
#include <sys/syscall.h>
#include "Timestamp.h"

class EventLoop;
//利用只能指针解决conn析构问题
class Connection;
using spConnection=std::shared_ptr<Connection>;
//this报错是因为this穿的是普通指针
//监听的功能
class Connection:public std::enable_shared_from_this<Connection>
{
    private:
        EventLoop *loop_;   //Acceptor对应的事件循环，在构造函数中传入，一个对应一个
        std::unique_ptr<Socket> clientsock_;  //服务端用于监听的socket,在构建函数中创建
        std::unique_ptr<Channel> clientchannel_;    //  Acceptor对应的Channel，在构造函数中创建。
        std::function<void (spConnection)> closecallback_;
        std::function<void (spConnection)> errorcallback_;
        std::function<void(spConnection,std::string &)> slovemessagecallback_;
        //取消完写事件，就可以是发送完毕
        std::function<void (spConnection)> sendCompletecb_;
        
        std::atomic<bool> disconnect_;
        // 云盘协议对接：接收/发送缓冲都用 sep=0（原始字节）
        // PDU 是 [4字节总长(含自身)]+内容，与 sep=1 的"长度不含头"不兼容；
        // 原始字节交给业务层(CloudServer)自己按 PDU 头分包，发送直接发整个 PDU 内存
        Buffer inputbuffer_{0};
        Buffer outputbuffer_{0};
        Timestamp lasttime_;
    public:
        Connection(EventLoop *loop,std::unique_ptr<Socket> clientsock); //  
        ~Connection();

        int fd() const;
        std::string ip(); //返回ip_
        uint16_t port();    //返回port_

        void closecallback();   //我感觉取名有点问题
        void errorcallback();

        void setcloseback(std::function<void (spConnection)>fn);
        void seterrorback(std::function<void (spConnection)>fn);
        void setslovecb(std::function<void(spConnection,std::string &)>fn);
        void setsendCompletecb(std::function<void (spConnection)>fn);
        void onMessage();

        //可以是把数据先放到outbuffer,然后注册写事件，发送线程，不管是什么线程都是调用这个发送数据到
        void sendto_ob(const char* data,size_t size);
        //发送数据，如果当前线程是IO线程，直接调用此函数，如是工作线程，把此函数传给IO线程
        void sendinloop(const char* data,size_t size);
        void writecallback();

        //检测Connection是否超时
        bool timeout(time_t now,int val);

        // 返回客户端对应的 Channel（供 TcpServer 在连接建立后投递注册读事件）
        Channel* clientchannel() { return clientchannel_.get();}


};