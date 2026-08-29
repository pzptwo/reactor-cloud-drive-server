#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>//struct sockaddr_in servaddr;
#include <iostream>

using namespace std;

int main (int argc,char * argv[])
{
    if(argc!=3)
    {
        printf("usage :ip port\n ");//argv顺序存
        printf("./client 192.168.152.128 5000\n");
        return -1;
    }
    int sockfd;

    if((sockfd=(socket(AF_INET,SOCK_STREAM,0)))==-1)
    {   
        printf("socket fail\n");
        // close (sockfd);
        exit(1);//??
    }
    //bind 
    
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_addr.s_addr=inet_addr(argv[1]);
    servaddr.sin_port=htons(atoi(argv[2]));
    servaddr.sin_family=AF_INET;

    if((connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)))==-1)
    {
        printf("connect(%s:%s) failed.\n",argv[1],argv[2]); close(sockfd);  return -1;
        
    }
    
    cout<<"connect tcpepoll"<<endl;
    printf("开始时间：%ld\n",time(0));
    char buf[1024];
    
//发送与接收
    for(int i=0;i<10000;i++)
    {
        memset(buf,0,sizeof(buf));
        sprintf(buf,"第%d次",i);
        int len=strlen(buf);
        char tmpbuf[1024+sizeof(len)];  //1028
        memset(tmpbuf,0,sizeof(tmpbuf));
        //分两次接受 报文1 报文2 ，后面根据报文一拿到报文2
        memcpy(tmpbuf,&len,sizeof(len));    //先发头部
        memcpy(tmpbuf+sizeof(len),buf,sizeof(buf));
        //int tmplen=sizeof(tmpbuf);,非常错误，这不是动态数组

        send(sockfd,tmpbuf,len+sizeof(len),0);
        //这里就是io
        //这里是读取
        recv(sockfd,&len,sizeof(len),0);    //读取头部
        memset(buf,0,sizeof(buf));
        recv(sockfd,buf,len,0);     //读取报文，从fd里面读取
        //cout<<" recv"<<buf<<endl;
    }
    printf("结束时间：%ld\n",time(0));
    //return 0;
}