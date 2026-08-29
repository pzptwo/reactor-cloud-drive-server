#include "ThreadPool.h"
#include <mutex>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>


ThreadPool::ThreadPool(size_t threadNum,const std::string &threadType):stop_(false),threadType_(threadType)
{
    //主线程后，产生子线程
    for(size_t i=0;i<threadNum;i++)
    {
        thread_.emplace_back([this]
        {
            printf("create %s thread id:%ld\n",threadType_.c_str(),syscall(SYS_gettid));    //记得只能是对象里面的标量
            //这里说明子线程
            while(stop_==false)
            {
                //这边属于要拿走任务队列里面的人物，需要一个盘子
                std::function<void ()> task;
                {
                    std::unique_lock<std::mutex> lock(this->mutex_);
                    //排除cpu空等，无任务该wait，就要利用条件变量condition,这里的this是lambda传进来的
                    //这里有堵塞队列（内核）
                    this->condition_.wait(lock,[this]{
                        return (this->stop_==true||this->taskqueue_.empty()==false);
                    });
                    //wait一开始解锁，后面加锁
                    //这里筛选关闭子线程的判断
                    if(this->stop_==true&&this->taskqueue_.empty()==true)
                        return ;

                    //如果不是的话，需要把任务队列,队列的相关知识
                    //使用移动语义，性能更高
                    task=std::move(taskqueue_.front());
                    this->taskqueue_.pop();
                }
                //printf("%s thread id :%ld execute task\n",threadType_.c_str(),syscall(SYS_gettid));
                task();//开始运行
            }
        });
    }
}
//和上面对应，因为对于c11，可以打包很多
void ThreadPool::addtask(std::function<void ()> task)
{
    {
     std::lock_guard<std::mutex> lock(this->mutex_);
     this->taskqueue_.push(task);
    }
    this->condition_.notify_one();//唤醒一个堵塞队列的线程
}

void ThreadPool::stop()
{
    //这里的防止重复停止啥意思
    if(stop_) return ;
    this->stop_=true;
    //一定要唤醒！！！
    this->condition_.notify_all();
    for(std::thread &th:thread_)
    {
        th.join();//这里会阻塞！！！
    }
}

ThreadPool::~ThreadPool()
{
    stop();
}
/*
//测试
void way1(){
    printf("飞起来；\n");
}
void way2(){
    printf("wo飞起来\n");
}
void way3(){
    printf("ni飞起来\n");
}

int main()
{
    ThreadPool threadpool(3);
    threadpool.addtask(std::bind(way1));
    sleep(1);
    threadpool.addtask(std::bind(way2));
    sleep(1);
    threadpool.addtask(std::bind(way3));
    sleep(1);
}
*/
/*
//测试,只能指针处理

#include <memory>
class AA
{
    public:
        void show()
        {
            printf("aa\n");
        }
    ~AA()
    {
        printf("调用了析构函数\n");
    }
};

//我想测试的数野指针
void func(std::shared_ptr<AA>aa)
{
    sleep(5);
    aa->show();
}

int main()
{
    ThreadPool tp(2,"Text");
    {
        std::shared_ptr<AA> aa(new AA);
        tp.addtask(std::bind(&func,aa));
        //需要手动释放
    }
    sleep(10);

    return 0;
}
*/

size_t ThreadPool::threadSize()
{
    return thread_.size();
}