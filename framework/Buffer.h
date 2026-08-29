#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <string.h>

class Buffer
{
    private:
        //定义一个容器，这里面回调用stl里面
        std::string buf_;

        uint16_t seq_;  //分隔符，0---无，1----四字节，2-----http(\n\r\r....)还有------
    public:
        Buffer(uint16_t seq=1);
        ~Buffer();
        void append(const char *data,size_t size);
        void appendwithseq(const char *data,size_t size);  //增加报头+内容
        const char* data();//
        size_t size();
        void erase(size_t pos,size_t n);
        void clear();

        //完善buffer的功能，就是在连接之后的报文的处理应该在buffer类完成
        bool pickmessage(std::string &pickmessage);
};