
#include <cstdio>
#include <sys/epoll.h>
#include "Channel.h"
#include "Epoll.h"
using namespace std;
 //在构造函数里面创建了epollfd_
Epoll::Epoll()
{
    epollfd_=epoll_create(1);
    if(epollfd_==-1)
    {
        printf("epoll_create fail\n");
        exit(-1);
    }
}
//关闭epollfd_
Epoll::~Epoll()
{
    close(epollfd_);
}


//思考epoll的三个函数功能，进行封装
//把fd和它需要监视的事件添加到红黑树上面。
/*
void Epoll::addfd(int fd,uint32_t op)
{
    struct epoll_event ev;//这里是epoll的数据结构
    ev.data.fd=fd;  //指定事件的自定义数据，会随着epoll_wait()返回的事件一并返回？？？？
    ev.events=op;

    int addnum=epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&ev);
    if(addnum==-1)
    {
        printf("epoll_ctl() failed(%d).\n",errno);
        exit(-1);
    }
}
*/
//channel添加或者更新到红黑树上，channel中有fd，也有需要监视的事件
void Epoll::updatechannel(Channel *ch)
{
    //因为这里是epoll_ctl，需要声明事件的数据结构
    epoll_event ev;
    ev.data.ptr=ch; //指定channel
    ev.events=ch->events();//指定事件

    if(ch->inepoll())
    {
        if((epoll_ctl(epollfd_,EPOLL_CTL_MOD,ch->fd(),&ev))==-1)
        {
            perror("epoll_ctl failed \n");
            exit(-1);   //注意噶
        }
    }
    else
    {
        if((epoll_ctl(epollfd_,EPOLL_CTL_ADD,ch->fd(),&ev))==-1)
        {
            perror("epoll_ctl failed \n");
            exit(-1);
        }
        //这里已经挂书上来更新值
        ch->setinepoll(true);
    }
}   

void Epoll::removechannel(Channel *ch)
{
    if(ch->inepoll())
    {
        printf("removechannel()\n");
        if((epoll_ctl(epollfd_,EPOLL_CTL_DEL,ch->fd(),0))==-1)
        {
            perror("epoll_ctl failed \n");
            exit(-1);
        }
        //ch->setinepoll(false);
    }
}
//这里很妙使用了stl里面的vector去存放epoll_wait的evs;
//还有一个参数timeout
/*
//运行epoll_wait(),等待事件的发生，已发生的事件用vector返回
std::vector<epoll_event>Epoll::loop(int timeout)
{
    std::vector<epoll_event> evs;   //这个的作用是相当于对于private的属性的接口吗？？？？？
    bzero(events_,sizeof(events_));
    int fdnums=epoll_wait(epollfd_,events_,Maxevents,timeout);//这个函数仔细解释,等待监视事件fd又是件发生
        if(fdnums==-1)
        {
            perror("epoll_wait fail\n");
            //循环---》break
            exit(-1) ;
        }
        else if(fdnums==0)
        {
            cout<<"超时"<<endl;
            return evs;
        }
        //这里是没问题的，默认大于0的文件描述符
        for(int i=0;i<fdnums;i++)
        {
            evs.push_back(events_[i]);
        }
        return evs; //重要噶
}

*/

//运行epoll_wait(),等待事件的发生，已发生的事件用vector返回
std::vector<Channel *>Epoll::loop(int timeout)
{
    std::vector<Channel *> channels;   //这个的作用是相当于对于private的属性的接口,存放epoll_wait()返回的事件
    bzero(events_,sizeof(events_));
    int fdnums=epoll_wait(epollfd_,events_,Maxevents,timeout);//这个函数仔细解释,等待监视事件fd又是件发生
        if(fdnums==-1)
        {
            perror("epoll_wait fail\n");
            //循环---》break
            exit(-1) ;
        }
        else if(fdnums==0)
        {
            //cout<<"超时"<<endl;
            return channels;
        }
        //这里是没问题的，默认大于0的文件描述符
        for(int i=0;i<fdnums;i++)
        {
            //evs.push_back(events_[i]);
            //这里要取出已发生事件的channel，首先要声明，因为Channel是一个一个的来,从events_[i]取出来，这里的数据结果非常重要
            Channel *ch=(Channel *)events_[i].data.ptr; //已发生事件的channel,所以是revent
            ch->setrevent(events_[i].events);
            channels.push_back(ch);
            
        }
        return channels; //重要噶
}