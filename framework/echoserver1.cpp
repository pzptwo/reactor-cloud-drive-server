#include "EchoServer.h"
#include <csignal>
using namespace std;
#include <signal.h> //用信号结束，由于是异步，所以作用域

EchoServer *echoserver;
//如果不是accept4的话
//设置非阻塞的IO,????
void setnonblocking(int fd)
{
    fcntl(fd, F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);
}
void Stop(int sig)
{
    printf("sig is %d\n",sig);
    //要停止服务，所以要在最上层类创建一个EchoServer::stop
    echoserver->stop();
    printf("echoserver stop\n");
    delete echoserver;
    printf("delete echoserver\n");
    exit(0);
}

int main(int argc,char *argv[])
{
    if(argc!=3)
    {
        printf("usage :ip port \n ");//argv顺序存
        printf("./echoserver 192.168.152.128 5000\n\n");
        return -1;
    }
    //注册信号处理
    signal(SIGTERM,Stop);   //信号15系统kill或killall命令默认发送的信号
    signal(SIGINT, Stop);   //信号2，按Ctrl+c发送的信号
    echoserver=new EchoServer(argv[1],atoi(argv[2]),3,2);  //这里的后两个参数一个是IO线程的数量，一个是WORK的
    echoserver->start();
    printf("已更改，名字\n");
    return 0;
}


//c++ struct 可以省略
