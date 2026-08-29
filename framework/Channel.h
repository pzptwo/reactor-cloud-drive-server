#pragma once
#include <cstdint>
#include <sys/epoll.h>
#include <functional>

#include "EventLoop.h"
#include "InetAddress.h"
#include "Socket.h"
#include <iostream>

class InetAddress;
class Socket;
class Epoll;
class EventLoop;
//这个Channel的作用不再是底层，而是更细致的“状态管理”,
// 与Epoll是协作的关系，一个Channel对应一个Epoll,一个Epoll对应多个Channel

class Channel
{
    private:
        int fd_;
        //Epoll *ep_; //说明channel 是属于那个红黑树,更加封装，
        EventLoop *loop_;   //说明channel 是属于那个红黑树
        bool inepoll_=false;    //Channel是否已经添加到Epoll树上
        //需要分为event_ revent_ 是由于epoll_ctl（）是传入参数，epoll_wait（）是传出参数
        uint32_t events_;    //fd_需要监听的事件listenfd和clientfd需要监听EPOLLIN ,但是clientfd还需要监听EPOLLOUT
        uint32_t revents_;   //fd_已发生的事件

        //bool islisten_=false;    //listenfd取值为true，客户端连上为false

        //与c的回调不同，这里要用容器（因为结构体的this）
        std::function<void ()>readback_;    //这里的参数是调用的时候没有参数，与bind不同
        //因为两种类型，所以分为两种回调函数
        std::function<void ()>closeback_; 
        std::function<void ()>errorback_; 
        std::function<void ()>writeback_;
    public:
        Channel(EventLoop *loop,int fd);    //Channel是Acceptor和Connection的下层类
        ~Channel();
        //这个可以从Channel对应(管理)一个fd，fd相关的状态
        int fd();
        void useet();   //采用边缘触发
        void enablereading();//让epoll_wait监听fd_的事件（这里是读）
        void disablereading();
        void enablewriting();
        void disablewriting();

        //对于删，就是改fd关闭，没有任何事件了
        void disableall();  //取消所有事件
        
        void setinepoll(bool inepoll);  //把inepoll_成员设置为true;
        void setrevent(uint32_t ev);   //外面的将uint32_t ev传进来，给revent_赋值
        bool inepoll(); //返回inepoll_成员
        uint32_t events();
        uint32_t revents();

        void handleevent(); //事件处理函数，epoll_wait()返回的时候，执行它,因为缺少servsock，所以这里传参进去
        //继续修改，由于不能定制功能，采用回调,定义接口
        void setreadback(std::function<void ()>fn); //
        void setwriteback(std::function<void ()>fn); //
        void onMessage();   //这里是已连接的fd，客户端
        //void newConnection(Socket *servsock);   //这里是新的连接请求

        void setcloseback(std::function<void ()>fn); 
        void seterrorback(std::function<void ()>fn); 

        void remove();  //从事件循环删除Channel,因为fd关了，先取消事件，再删除，就更好。

};