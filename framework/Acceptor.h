#pragma once 
#include "EventLoop.h"
#include "Socket.h"
#include "Channel.h"
#include <memory>
#include <functional>

//监听的功能
class Acceptor  
{
    private:
        EventLoop *loop_;   //Acceptor对应的事件循环，在构造函数中传入，一个对应一个
        Socket servsock_;  //服务端用于监听的socket,在构建函数中创建
        Channel acceptchannel_;    //  Acceptor对应的Channel，在构造函数中创建。

        //设置回调
        std::function<void(std::unique_ptr<Socket>)> newConnectioncb_; //设置
    public:
        Acceptor(EventLoop *loop,const std::string ip,const uint16_t port); //  
        ~Acceptor();
        //这里的优化是由于newconnection是由acceptor产生的，不用在下层类Channel中写了
        void newConnection();   //这里有成团变量不用参数
        void setnewConnectioncb(std::function<void(std::unique_ptr<Socket>)> fn);   //
};