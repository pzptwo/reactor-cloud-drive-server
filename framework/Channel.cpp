#include "Channel.h"
#include "EventLoop.h"
#include "Epoll.h"
#include <sys/epoll.h>

using namespace std;


Channel::Channel(EventLoop *loop,int fd)
    : loop_(loop), fd_(fd)
    // 4 个回调默认设为空操作，防止"事件触发但回调未设置"时调用空 std::function 崩溃
    // （例如 Acceptor/EventLoop 的 fd 只注册了读事件，EPOLLOUT/错误事件触发时会走这些回调）
    , readback_([]{ }), closeback_([]{ }), errorback_([]{ }), writeback_([]{ })
{

}

Channel::~Channel()
{

}
//这个可以从Channel对应(管理)一个fd，fd相关的状态
int Channel::fd()
{
    return fd_;
}
//采用边缘触发
void Channel::useet()
{
    //注意因为是宏，所以用位运算
    events_|=EPOLLET;
}
//让epoll_wait监听fd_的事件（这里是读）
void Channel::enablereading()
{
    events_=events_|EPOLLIN;
    loop_->updatechannel(this);
}

void Channel::disablereading()
{
    events_=events_&~EPOLLIN;
    loop_->updatechannel(this);
}

void Channel::enablewriting()
{
    events_=events_|EPOLLOUT;
    loop_->updatechannel(this);
}

void Channel::disablewriting()
{
    events_=events_&~EPOLLOUT;
    loop_->updatechannel(this);
}
void Channel::disableall()
{
    events_=0;
    loop_->updatechannel(this);
}

void Channel::remove()
{
    disableall();
    loop_->removechannel(this);
}
//把inepoll_成员设置为true;
void Channel::setinepoll(bool inepoll)
{
    inepoll_=inepoll;
} 
//外面的将uint32_t ev传进来，给revent_赋值
void Channel::setrevent(uint32_t ev)
{
    revents_=ev;
}  
//返回inepoll_成员
bool Channel::inepoll()
{
   return inepoll_;
}

uint32_t Channel::events()
{
    return events_;
}
uint32_t Channel::revents()
{
    return revents_;
}
//事件处理函数，epoll_wait()返回的时候，执行它
void Channel::handleevent()
{
    if(revents_&EPOLLRDHUP)
    {
        //这里表示对方已关闭
        remove();   //从事件删除Channel；
        closeback_();
    }
    else if(revents_&(EPOLLIN|EPOLLPRI))
    {
        //服务器这边要分为两种
        /*
        if(islisten_==true)//这里就要开始accept了,这里就会有新的fd了,//v6改的时候，看看我的这里并没有分两种fd
        {
            newConnection(servsock);
        }
        else 
        {
            onMessage();
        }
        */
        readback_();
    }
    //后面的events类型
    else if(revents_&EPOLLOUT) // 有数据需要写，暂时没有代码，以后再说。
    {
        //要用回调写
        //printf("EPOLLOUT\n");
        writeback_();
    }
    else 
    {
        //remove();   //从事件删除Channel；
        errorback_();
    }
}
//

//这里是已连接的fd，客户端
/*

void Channel::onMessage()
{
    char buffer[1024];
    while(true)
    {
        bzero(buffer,sizeof(buffer));//这个函数与memset的区别
        ssize_t nread=read(fd_,buffer,sizeof(buffer));//这个函数的赋值？？？

        if(nread>0)
        {
            cout << "recv from client(eventfd=" <<fd_ << "): " << buffer << endl;
            //这里服务器在接收到数据，后的处理方式
            send(fd_,buffer,strlen(buffer),0);
        }
        //错误有好几种，有些需要排除
        else if(nread==-1&&errno==EINTR)     // 读取数据的时候被信号中断，继续读取。
        {
            continue;
        }
        else if(nread==-1&&((errno == EAGAIN) || (errno == EWOULDBLOCK)))
        {
            break;
        }
        else if(nread==0)   // 客户端连接已断开，和上面的重复了
        {
            closeback_();            // 关闭客户端的fd。
            break;
        }
    }
} 
    */

void Channel::setreadback(std::function<void ()>fn)
{
    readback_=fn;
}

void Channel::setcloseback(std::function<void ()>fn)
{
    closeback_=fn;
}
void Channel::seterrorback(std::function<void ()>fn)
{
    errorback_=fn;
}

void Channel::setwriteback(std::function<void ()>fn)
{
    writeback_=fn;
}


