#pragma once 
#include "Connection.h"
#include "TcpServer.h"
#include "ThreadPool.h"

//这个类就是上层类，可以有自己的特点了，业务类（回显业务）
class EchoServer
{
    private:
        TcpServer tcpserver_;   //
        ThreadPool threadpool_; //这里为啥不用堆内存
        int workthreadNum_;

    public:
        EchoServer(const std::string ip,const uint16_t port,int subthreadNum=3,int workthreadNum=5);    //subthreadNum->oithread
        ~EchoServer();

        void start();   //调用loop的run,相当于*loop_的接口，裕兴事件循环


        //接口！！！！
        void HandleNewConnection(spConnection conn);   //处理新连接上来的,Socket *clientsock这里不行，类之间的关系而且这样直接跳过来outbuffer
        void HandleClose(spConnection conn);   //我感觉取名有点问题
        void HandleError(spConnection conn);
        void HandleSlovemessage(spConnection conn,std::string & message);

        //对于最上层，要知道，数据已经发送完毕
        void HandleSendComplete(spConnection conn);

        void onworkmessage(spConnection conn,std::string & message); //  这里是建立workthread的工作任务
        //对于最上层，要知道，是否超时,当Channel为空时，而且因为在eventloop里面，所以要说明是哪一个loop
        //void HandleEpolltimeout(EventLoop *loop);

        //设置停止work线程
        void stop();
};