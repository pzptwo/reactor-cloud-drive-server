#include "Connection.h"
#include "Channel.h"
#include "Timestamp.h"
#include <ctime>
#include <utility>

Connection::Connection(EventLoop *loop, std::unique_ptr<Socket>clientsock)
    : loop_(loop), clientsock_(std::move(clientsock)),clientchannel_(new Channel(loop_, clientsock_->fd()))
    , disconnect_(false)
{
  //clientchannel_ = new Channel(loop_, clientsock_->fd());
  clientchannel_->setreadback(std::bind(&Connection::onMessage, this));
  clientchannel_->setcloseback(
  std::bind(&Connection::closecallback, this)); // 这里的是在Tcpserver回调
  clientchannel_->seterrorback(std::bind(&Connection::errorcallback, this));
  clientchannel_->setwriteback(std::bind(&Connection::writecallback, this));
  //clientchannel_->useet();
  //clientchannel_->enablereading();  // 移到连接完全建立后由 TcpServer 投递，避免 shared_from_this 竞态
  // clientchannel.updatechannel(clientchannel);
}

Connection::~Connection() 
{
  //delete clientchannel_;
  //delete clientsock_; // 这里相当于我拿走了new的全部
}

int Connection::fd() const { return clientsock_->fd(); }
// 返回ip_
std::string Connection::ip() { return clientsock_->ip(); }
// 返回port_
uint16_t Connection::port() { return clientsock_->port(); }

void Connection::closecallback() 
{
  disconnect_ = true;
  clientchannel_->remove();
  closecallback_(shared_from_this()); 
}

void Connection::errorcallback() 
{
  disconnect_ = true;
  clientchannel_->remove();
  errorcallback_(shared_from_this()); 
}

void Connection::setcloseback(std::function<void(spConnection)> fn) {
  closecallback_ = fn;
}

void Connection::seterrorback(std::function<void(spConnection)> fn) {
  errorcallback_ = fn;
}

void Connection::setslovecb(
    std::function<void(spConnection, std::string &)> fn) {
  slovemessagecallback_ = fn;
}

void Connection::setsendCompletecb(std::function<void(spConnection)> fn) {
  sendCompletecb_ = fn;
}

using namespace std;
void Connection::onMessage() {
  char buffer[1024];
  while (true) {
    bzero(buffer, sizeof(buffer)); // 这个函数与memset的区别
    ssize_t nread = read(fd(), buffer, sizeof(buffer)); // 这个函数的赋值？？？

    if (nread > 0) {
      // 这里看看要不要清空，inputbuffer_
      inputbuffer_.append(buffer, nread); // 现在的buffer没有数据
    }
    // 错误有好几种，有些需要排除
    else if (nread == -1 &&
             errno == EINTR) // 读取数据的时候被信号中断，继续读取。
    {
      continue;
    } else if (nread == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) 
    {
      std::string message;  
      // 这里开始读,逻辑是将客户端发过来进行读取
      while (true) {
        //////////////下面代码可以封装到buffer，还可以支持固定长度，指定报文长度，和分隔符等多种格式
        if(inputbuffer_.pickmessage(message)==false) break;;  //这里是bool
        /////////////////////////////////
        // 服务器对信息进行处理,数据的计算
        /*
        message="reply"+message;
        //发送模式为头加内容
        len=message.size();
        //这里用string，char都行
        //相当于进行增加报头，但是是拷贝构造？？？
        std::string tmpbuf((char *)&len,sizeof(len));
        tmpbuf.append(message);
        send(fd(),tmpbuf.data(),tmpbuf.size(),0);
        */
        //服务器接受数据的最新时间
        lasttime_=Timestamp::now();
        slovemessagecallback_(shared_from_this(), message);
      }
      break;
    } else if (nread == 0) // 客户端连接已断开，和上面的重复了
    {
      //clientchannel_->remove();
      closecallback(); // 关闭客户端的fd。
      break;
    }
  }
}


void Connection::sendto_ob(const char *data, size_t size) 
{
  if(disconnect_==true) {printf("客服端已断开了，send()直接返回。\n"); return ;}
  //IO线程
  if(loop_->isinloop())
  {
    //printf("send()在事件循环中\n");
    sendinloop(data,size);
  }
  //work 线程
  else 
  {
    //printf("send()不在事件循环中\n");
    // 修复：先把数据拷贝成 std::string 再投递到 IO 线程，
    // 避免调用方局部变量析构后 data 指针悬空（use-after-free）
    loop_->setinqueue([this, s = std::string(data, size)]() { sendinloop(s.data(), s.size()); });
  }
}

//发送数据，如果当前线程是IO线程，直接调用此函数，如是工作线程，把此函数传给IO线程
void Connection::sendinloop(const char* data,size_t size)
{
  // 这里不一样了，这里进缓冲区的是报文长度+内容
  outputbuffer_.appendwithseq(data, size);
  // 注册写事件
  clientchannel_->enablewriting();
}

void Connection::writecallback() {
  // 把outbuffer的数据发送出去
  int writen = ::send(fd(), outputbuffer_.data(), outputbuffer_.size(), 0);
  if (writen > 0)
    outputbuffer_.erase(0, writen);

  // 说明没有数据了，发送成功，取消写事件
  if (outputbuffer_.size() == 0)
    clientchannel_->disablewriting();

  sendCompletecb_(shared_from_this());
}

bool Connection::timeout(time_t now ,int val)
{
  return now-lasttime_.toint()>val;
}
