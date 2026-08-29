#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include <bits/types/struct_timeval.h>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <sys/syscall.h>
#include <unistd.h>


//对于定时器的创建
int createtimerfd(int sec=30)
{
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
    struct itimerspec timeout;
    memset(&timeout, 0, sizeof(struct itimerspec));
    timeout.it_value.tv_sec = sec;       // 5 秒后第一次触发,这里应该不写死。
    timeout.it_value.tv_nsec = 0;
    timerfd_settime(tfd, 0, &timeout, 0);   //相当于alarm(5)
    return tfd;
}
EventLoop::EventLoop(bool mainloop,int timeval,int timeout):mainloop_(mainloop),timeval_(timeval),timeout_(timeout),stop_(false)
                    ,ep_(new Epoll),wakefd_(eventfd(0,EFD_NONBLOCK)),wakechannel_ (new Channel(this,wakefd_ ))
                    ,timerfd_(createtimerfd(timeout_))
                    ,timerfdchannel_(new Channel(this,timerfd_ ))
{
    wakechannel_->setreadback(std::bind(&EventLoop::handlewakeIO,this));
    wakechannel_->enablereading();

    timerfdchannel_->setreadback(std::bind(&EventLoop::handletime,this));
    timerfdchannel_->enablereading();

}
 EventLoop::~EventLoop()
{
    //delete ep_;
}

void  EventLoop::run()
{
    threadspid_=syscall(SYS_gettid);
    while(stop_==false) //事件循环
    {
        std::vector<Channel *> channels;
          
        //看看这里，可以继续封装，这里都是channel
        channels=ep_->loop(10*1000);  //这个函数的类型为std::vector<epoll_event>锁一前面要定义一个evs来接受
        if(channels.size()==0)
        {
            epolltimeoutcb_(this);
        }
        else
        {
            for(auto &ch:channels)   //熟悉这种遍历。
            {
                ch->handleevent();
        }
        }
    }
}

void EventLoop::stop()
{
    //注意原子操作，给一个标志位
    stop_=true;
    //但是还未停止epoll_wait，只能去唤醒
    wakeIO();   //让他直接是？？（）
}



void EventLoop::setepolltimeoutcb(std::function<void (EventLoop *)> fn)
{
    epolltimeoutcb_=fn;
}

void EventLoop::updatechannel(Channel *ch)
{
    ep_->updatechannel(ch);
}

void EventLoop::removechannel(Channel *ch)
{
    ep_->removechannel(ch);
}

bool EventLoop::isinloop()
{
    return threadspid_==syscall(SYS_gettid);
}

//设置添加队列的函数
void EventLoop::setinqueue(std::function<void ()>fn)
{
    //把任务加入到队列里面需要锁
    {
        std::lock_guard<std::mutex>gd(mutex_);
        taskqueue_.push(fn);
    }

    //有了任务就要唤醒IO事件循环
    wakeIO();
}

//唤醒eventfd
void EventLoop::wakeIO()
{
    uint64_t val=1;
    write(wakefd_,&val,sizeof(val));
}

 //唤醒事件循环后，就要执行任务了（消费任务）
 void EventLoop::handlewakeIO ()
 {
    //printf("handlewakeIO thread is %ld\n",syscall(SYS_gettid));
    uint64_t n;
    //要把eventfd的读出来，否则在水平触发会一直触发
    read(wakefd_,&n,sizeof(n));

    std::function<void()> tmp;  //接收执行任务的变量
    //这里就要开始（执行任务）
    std::lock_guard<std::mutex> gd(mutex_);
    while(taskqueue_.size()>0)
    {
        tmp=std::move(taskqueue_.front());
        taskqueue_.pop();
        tmp();
    }

 }

 void EventLoop::handletime()
 {
    // 重新计时——因为 timerfd 不会自动循环
    struct itimerspec timeout;
    memset(&timeout, 0, sizeof(struct itimerspec));
    timeout.it_value.tv_sec = timeval_;
    timeout.it_value.tv_nsec = 0;
    timerfd_settime(timerfd_, 0, &timeout, 0);

    if(mainloop_==true)
    {
        //printf("主事件循环的闹钟响了\n");,、
        //主事件没有Connection，只有Accepter
    }
    else
    {
        //printf("从事件循环的闹钟响了\n");
        //一个从事件循环对应一个线程编号
        //printf("handletime() thread :%ld fd ",syscall(SYS_gettid));
        //
        //获取当前时间
        time_t now=time(0);
        //打印还有几个Connection->fd(mao 里面一个fd对应一个Connection)
        for (auto it=conns_.begin();it!=conns_.end();)
        
        {
            //(" %d",it->first);
            if (it->second->timeout(now,timeout_)) 
            {
                //printf("EventLoop::handletimer()1  thread is %d.\n",syscall(SYS_gettid)); 
                int fd=it->first;
                {
                    std::lock_guard<std::mutex> gd(mmutex_);
                    it=conns_.erase(it);                  // erase返回下一个有效迭代器。
                }
                timerout_(fd);             // 从TcpServer的map中删除超时的conn。
            }
            else
            {
                ++it;
            }
        }
            //printf("\n");
    }
 }

 void EventLoop::addnewConnection(spConnection conn)
 {
    {
        std::lock_guard<std::mutex> gd(mmutex_);
        conns_[conn->fd()]=conn;    //但是conn都是在Tcpserver里面，要去里面调用这个函数
    }
 }
    

 void EventLoop::settimerout(std::function<void(int )> fn)
 {
    timerout_=fn;
 }

