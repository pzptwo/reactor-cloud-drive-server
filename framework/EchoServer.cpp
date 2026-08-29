#include "EchoServer.h"
#include "Connection.h"
#include "TcpServer.h"
#include "ThreadPool.h"
#include "Timestamp.h"
#include <functional>
#include <iostream>
#include <ratio>
#include <sys/syscall.h>

using namespace std;

EchoServer::EchoServer(const std::string ip,const uint16_t port,int subthreadNum,int workthreadNum):tcpserver_(ip,port,subthreadNum),
                        threadpool_(workthreadNum,"WORKS")
{
    tcpserver_.setnewConnectioncb(std::bind(&EchoServer::HandleNewConnection,this,std::placeholders::_1));
    tcpserver_.setclosecb(std::bind(&EchoServer::HandleClose,this,std::placeholders::_1));
    tcpserver_.seterrorcb(std::bind(&EchoServer::HandleError,this,std::placeholders::_1));
    tcpserver_.setslovemessagecb(std::bind(&EchoServer::HandleSlovemessage,this,std::placeholders::_1,std::placeholders::_2));
    tcpserver_.setsendCompletecb(std::bind(&EchoServer::HandleSendComplete,this,std::placeholders::_1));
    //tcpserver_.setepolltimeoutcb(std::bind(&EchoServer::HandleEpolltimeout,this,std::placeholders::_1));
}

EchoServer::~EchoServer()
{

}
//调用loop的run,相当于*loop_的接口，裕兴事件循环
void EchoServer::start()
{
    tcpserver_.start();
} 

void EchoServer::stop()
{
    //停止自己的管的work线程(线程池)
    threadpool_.stop();
    //停止tcpserver管的IO
    tcpserver_.stop();
}
 //处理新连接上来的
void EchoServer::HandleNewConnection(spConnection conn)
{ 
    printf("%s new connection (fd=%d,ip=%s,port=%d)ok \n",Timestamp::now().toString().c_str(),conn->fd(),conn->ip().c_str(),conn->port());
   //printf("HandleNewConnection thread is %ld\n",syscall(SYS_gettid));
    //cout<<"New Connection Come in "<<endl;
}
//我感觉取名有点问题
void EchoServer::HandleClose(spConnection conn)
{
    printf("%s connection closed (fd=%d,ip=%s,port=%d)ok \n",Timestamp::now().toString().c_str(),conn->fd(),conn->ip().c_str(),conn->port());
    //cout<<"EchoServer conn close"<<endl;
} 

void EchoServer::HandleError(spConnection conn)
{
    //cout<<"EchoServer conn error"<<endl;

}
void EchoServer::HandleSlovemessage(spConnection conn,std::string & message)
{
    //我的目的是IO线程（事件循环处理数据），threadpool_.threadSize()==0没有工作线程
    if(threadpool_.threadSize()==0)
    {
        onworkmessage(conn, message);
    }
    //这里就是工作线程的开始。!!!!
    //printf("HandleSlovemessage thread is %ld\n",syscall(SYS_gettid));
    //把任务函数放入线程池里面的任务队列,由于我一开始的打包是bind.....<void ()>,无参，该函数有参，打包，有参的话，std::placeholders占位符
    else
    {
        threadpool_.addtask(std::bind(&EchoServer::onworkmessage,this,conn,message));    
    } 
}


void EchoServer::onworkmessage(spConnection conn,std::string & message)
{
    //printf("%s recv from client(fd=%d)%s:\n",Timestamp::now().toString().c_str(),conn->fd(),message.c_str());
    message="reply"+message;
    //这里演示相当于回显业务work需要很长时间，但是io线程不需要很长时间，（conn在io被释放）
    //printf("业务处理完之后，需要调用connection对象");
    
    conn->sendto_ob(message.data(),message.size()); 
}
//对于最上层，要知道，数据已经发送完毕
void EchoServer::HandleSendComplete(spConnection conn)
{
    //cout<<"Message Send cpmplete"<<endl;
}

//对于最上层，要知道，是否超时,当Channel为空时，而且因为在eventloop里面，所以要说明是哪一个loop
// void EchoServer::HandleEpolltimeout(EventLoop *loop)
// {
//     cout<<"EchoServer timeout"<<endl;
// }