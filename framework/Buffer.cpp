#include "Buffer.h"

Buffer::Buffer(uint16_t seq):seq_(seq)
{

}
Buffer::~Buffer()
{

}

//这里是使用了stl的string
void Buffer::append(const char *data,size_t size)
{
    buf_.append(data,size);
}
//

void Buffer::appendwithseq(const char *data,size_t size)
{
    //根据不通的分隔符，有不同的追加方式
    if(seq_==0)
    {
        buf_.append(data,size);
    }
    else if(seq_==1)
    {
        buf_.append((char *)&size,4);
        buf_.append(data,size);
    }
    else if(seq_==2)
    {
        //这里可以完善
    }
    
}

bool Buffer::pickmessage(std::string &pickmessage)
{
    if(buf_.size()==0) return false;
    //这个相当于pickmessage需要分隔buf_后，将pickmeassger发,所以pickmessage的类型
    if(seq_==0)
    {
        pickmessage=buf_;
        buf_.clear();
    }
    else if(seq_==1)
    {
        int len;
        // 上面已经接受了，现在是拷贝
        memcpy(&len, buf_.data(), sizeof(len));
        if (buf_.size() < len + 4)
            return false;
        //这里注意容器的用法
        pickmessage=buf_.substr(4, len);
        buf_.erase(0, len + 4);
    }

    return true;
}

const char* Buffer::data()
{
    return buf_.data();
}

size_t Buffer::size()
{
    return buf_.size();
}

void Buffer::clear()
{
    return buf_.clear();
}

void Buffer::erase(size_t pos,size_t n)
{
    buf_.erase(pos,n);
}