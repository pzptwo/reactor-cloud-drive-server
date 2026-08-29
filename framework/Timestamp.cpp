#include "Timestamp.h"

Timestamp::Timestamp()
{
    secsinceepoch_=time(0);
}
//用整形赋值个    
Timestamp::Timestamp(int64_t secsinceepoch):secsinceepoch_(secsinceepoch)
{

}

//为了获得返回当前时间的对象
Timestamp Timestamp::now()
{
    //其更新的作用
    return Timestamp();
}
//不同的表示返回时间的方式，time_t ----int64_t
time_t Timestamp::toint() const
{
    return secsinceepoch_;
}

std::string Timestamp::toString() const
{
    char buf[32] = {0};
    tm *tm_time = localtime(&secsinceepoch_);
    snprintf(buf, 20, "%4d-%02d-%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}