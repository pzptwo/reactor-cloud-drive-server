#pragma once 
#include "Channel.h"
#include "Epoll.h"
#include <functional>
#include <cstddef>
#include <memory>
#include <sys/types.h>
#include <thread>
#include <queue>
#include <mutex>
#include <map>
#include <atomic>
#include "Connection.h"
#include <sys/eventfd.h>
#include <sys/timerfd.h>      // 定时器需要包含这个头文件。

class Channel;
class Epoll;
class Connection;
using spConnection=std::shared_ptr<Connection>;
class EventLoop
{
    private:
        //闹钟时间间隔和超时参数化
        int timeval_;
        int timeout_;
        std::unique_ptr<Epoll> ep_;
        std::function<void (EventLoop *)> epolltimeoutcb_;
        pid_t threadspid_;

        //增加在WORK线程时候绑上的任务队列
        std::queue<std::function<void ()>> taskqueue_;
        std::mutex mutex_;

        //使用eventfd
        int wakefd_;

        //这里并没有把eventfd挂到红黑树上
        std::unique_ptr<Channel> wakechannel_;
        //这里c++提供了一种定时器的类，所以可以喝epoll使用,这里的用法查就可以，（还有类似的信号集机制）
        int timerfd_;

        //将fd_对应的channel挂到epoll树上
        std::unique_ptr<Channel> timerfdchannel_;

        //不同的线程超时的处理方法不同
        bool mainloop_;
        //Map容器->要设定一个往map里面添加Connection的方法
        std::map<int, spConnection> conns_;

        //设置回调容器
        std::function<void(int )> timerout_;    //删除TcpServer中超时的Connection

        std::mutex mmutex_;

        //标志位说明事件循环停止
        std::atomic_bool stop_;
        
    public:
        EventLoop(bool mainloop,int timeval=30,int timeout=80);
        ~EventLoop();

        void run();
        Epoll *ep();

        void setepolltimeoutcb(std::function<void (EventLoop *)> fn);
        void updatechannel(Channel *ch);

        void removechannel(Channel *ch);
        //这里很妙，因为run就是在IO线程循环里面的
        bool isinloop();

        //设置添加队列的函数
        void setinqueue(std::function<void ()> fn);
        //唤醒事件循环（这里杯阻塞在epoll_wait）
        void wakeIO();

        //唤醒事件循环后，就要执行任务了（消费任务）
        void handlewakeIO ();

        //定时器到了之后，就要执行的函数,由于要操作conn,为了提高性能，则在这里map
        void handletime();
        //往map里面添加Connection的方法
        void addnewConnection(spConnection conn);
        //设置回调(打包进入容器)
        void settimerout(std::function<void(int )> fn);

        //停止从事件循环
        void stop();
};