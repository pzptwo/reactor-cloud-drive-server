#include "TcpServer.h"
#include "Acceptor.h"
#include "Connection.h"
#include "Epoll.h"
#include "EventLoop.h"
#include <functional>
#include <mutex>
using namespace std;


TcpServer::TcpServer(const std::string ip,const uint16_t port,int threadNum):threadNum_(threadNum),mainloop_(new EventLoop(true,5,10))
                        ,acceptor_( Acceptor(mainloop_.get(),ip,port)),threadpool_(ThreadPool(threadNum_,"IO"))
{
    //mainloop_=new EventLoop;//把动态内存创建出来，对应析构
    mainloop_->setepolltimeoutcb(std::bind(&TcpServer::epolltimeout,this,std::placeholders::_1));
    //acceptor_=new Acceptor(mainloop_.get(),ip,port);
    //刚刚启动，还没有socket所以不能传clientsock
    acceptor_.setnewConnectioncb(std::bind(&TcpServer::newConnection,this,std::placeholders::_1));

    //完成线程池的创建
    //threadpool_=new ThreadPool(threadNum_,"IO");
    for(int i=0;i<threadNum_;i++)
    {
        //先当于子线程开始运行EventLoop,从容器里面去取
        //为啥要是改为智能指针后智能用emplace
        subloop_.emplace_back(new EventLoop(false,5,10));//这里的写法
        subloop_[i]->setepolltimeoutcb(std::bind(&TcpServer::epolltimeout,this,std::placeholders::_1));
        subloop_[i]->settimerout(std::bind(&TcpServer::removeconnection,this,std::placeholders::_1));
        //还要有相关启动，上面已经创建出来线程了，这里只需要把从事件循环放入到线程池的人物队列里面
        threadpool_.addtask(std::bind(&EventLoop::run,subloop_[i].get()));   //这里打包表明是第几个循环
        //sleep(1);
    }
    
}
TcpServer::~TcpServer()
{
    //delete loop_;
    //delete acceptor_;
    //delete mainloop_;
    /*
    for(auto &aa:conns_)
    {
        delete aa.second;
    }
    */

    //释放从事件循环
    // for(auto &aa:subloop_)
    // {
    //     delete aa;
    // }
    //delete threadpool_;
}

void TcpServer::start()
{
    mainloop_->run();
}

void TcpServer::stop()
{
    //这里为啥要三分种，而且顺序很重要
    //先要停止主事件循环
    mainloop_->stop();
    printf("主事件循环已停止\n");
    //停止从事件循环
    for(int i=0;i<threadNum_;i++)
    {
        subloop_[i]->stop();
    }
    printf("从事件循环已停止\n");
    //停止IO（线程池）
    threadpool_.stop();
    printf("IO线程池环已停止\n");
}


//处理新连接上来的
void TcpServer::newConnection(std::unique_ptr<Socket>clientsock)
{
    //Connection *conn=new Connection(mainloop_,clientsock);//这里也没有释放，为了耦合低
    //让从事件循环运行连接

    //防止move与unique_ptr
    int fd=clientsock->fd();
    //这里Connection里面输普通指针
    spConnection conn(new Connection(subloop_[fd%threadNum_].get(),std::move(clientsock)));
    conn->setcloseback(std::bind(&TcpServer::closecallback,this,std::placeholders::_1));
    conn->seterrorback(std::bind(&TcpServer::errorcallback,this,std::placeholders::_1));
    conn->setslovecb(std::bind(&TcpServer::slovemessage,this,std::placeholders::_1,std::placeholders::_2));
    conn->setsendCompletecb(std::bind(&TcpServer::sendComplete,this,std::placeholders::_1));
    // cout << "new Connection: fd=" << clientsock->fd()
    //         << ", ip=" << clientsock->ip()
    //         << ", port=" <<  clientsock->port()<< endl;
    {
        std::lock_guard<std::mutex>gd(mmtex_);
        conns_[conn->fd()]=conn;
    }
    subloop_[conn->fd()%threadNum_]->addnewConnection(conn);    //这里的移动语义用完后，消失了，注意内存（不能调用一个不存在的）
    // 连接完全建立后，再把读事件注册投递到 IO 线程（避免构造函数里提前 enablereading 引发 shared_from_this 竞态 -> bad_weak_ptr）
    subloop_[conn->fd()%threadNum_]->setinqueue(std::bind(&Channel::enablereading, conn->clientchannel()));
    if(newConnectioncb_)newConnectioncb_(conn); //新建连接的时候，要等连接建立好才开始
}

void TcpServer::closecallback(spConnection conn)
{
    if(closecb_) closecb_(conn); //关闭连接的时候，则是需要先回调
    //printf("client(eventfd=%d) disconnected.\n",conn->fd());
    //close(fd()); 
    {
        std::lock_guard<std::mutex>gd(mmtex_);
        conns_.erase(conn->fd());
    }
    
    //delete conn;
}

void TcpServer::errorcallback(spConnection conn)
{
    if(errorcb_)errorcb_(conn);
    //printf("client(eventfd=%d) error.\n",conn->fd());
    //close(conn->fd());
    {
        std::lock_guard<std::mutex>gd(mmtex_);
        conns_.erase(conn->fd());
    }
    
    //delete conn;
}

void TcpServer::slovemessage(spConnection conn,std::string &message)
{
    if(slovemessagecb_) slovemessagecb_(conn,message);
}

void TcpServer::sendComplete(spConnection conn)
{
    
    //参数一定要conn，表明是这个连接的
    //printf("send complete;\n");
    if(sendCompletecb_) sendCompletecb_(conn);
   
}

void TcpServer::epolltimeout(EventLoop *loop)
{
    
    //printf("epoll timeout\n");
    if(epolltimeoutcb_) epolltimeoutcb_(loop);//可以在这里面增加逻辑
    
}

void TcpServer::setnewConnectioncb(std::function<void (spConnection)>fn)
{
    newConnectioncb_=fn;
}
void TcpServer::setclosecb(std::function<void (spConnection)>fn)
{
    closecb_=fn;
}
void TcpServer::seterrorcb(std::function<void (spConnection)>fn)
{
    errorcb_=fn;
}
void TcpServer::setslovemessagecb(std::function<void (spConnection,std::string &) >fn)
{
    slovemessagecb_=fn;
}
void TcpServer::setsendCompletecb(std::function<void (spConnection)>fn)
{
    sendCompletecb_=fn;
}
void TcpServer::setepolltimeoutcb(std::function<void (EventLoop *)>fn)
{
    epolltimeoutcb_=fn;
}

void TcpServer::removeconnection(int fd)
{
    {
        std::lock_guard<std::mutex>gd(mmtex_);
        conns_.erase(fd);   //map key
    }
    
}