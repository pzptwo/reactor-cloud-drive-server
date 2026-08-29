#pragma once 
#include <ctime>
#include <time.h>
#include <cstdint>
#include <string>
class Timestamp
{
    private:
        time_t secsinceepoch_;  //表示当前时间（从1970）
    public:
        Timestamp();    
        Timestamp(int64_t secsinceepoch);   //用整形赋值个

        //为了获得返回当前时间的对象
        static Timestamp now();
        //不同的表示返回时间的方式，time_t ----int64_t
        time_t toint() const;
        std::string toString() const;
};