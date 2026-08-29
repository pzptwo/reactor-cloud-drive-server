#pragma once
#include <cstddef>
#include <mutex>
#include <functional>
#include <atomic>
#include <vector>
#include <thread>
#include <queue>
#include <condition_variable>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>

class ThreadPool
{
    private:
        std::vector<std::thread> thread_;   //线程数组
        std::queue<std::function<void ()> >taskqueue_;   //因为需要先进先出，队列；
        std::mutex mutex_;  //锁，前面是类型
        std::condition_variable condition_; //条件变量，可以使同步（消费）
        std::atomic_bool stop_;

        //这里要区分是啥线程
        std::string threadType_;
    public:
        ThreadPool(size_t threadNum,const std::string &threadType);
        void addtask(std::function<void ()> task); //和上面对应，因为对于c11，可以打包很多
        ~ThreadPool();

        size_t threadSize();

        //增加停止线程（因为是异步，所以不同于析构）
        void stop();
};