#pragma once

#include <sys/epoll.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <vector>
#include <cstdint>
#include <unistd.h>
#include <iostream>

class Channel;
class Epoll
{
    private:
        static const int Maxevents=100; //思考这里为啥要static???
        int epollfd_=-1;    //红黑树的句柄
        struct epoll_event events_[Maxevents];
    public:
        Epoll();    //在构造函数里面创建了epollfd_
        ~Epoll();   //关闭epollfd_

        //思考epoll的三个函数功能，进行封装
        //void addfd(int fd,uint32_t op); //把fd和它需要监视的事件添加到红黑树上面。更新户
        void updatechannel(Channel *ch);    //channel添加或者更新到红黑树上，channel中有fd，也有需要监视的事件

        void removechannel(Channel *ch);    //从红黑树上删除channel

        //这里很妙使用了stl里面的vector去存放epoll_wait的evs;
        //还有一个参数timeout
        //std::vector<epoll_event>loop(int timeout=-1);   //运行epoll_wait(),等待事件的发生，已发生的事件用vector返回
        std::vector<Channel *>loop(int timeout=-1);   //运行epoll_wait(),等待事件的发生，已发生的事件用vector返回
};