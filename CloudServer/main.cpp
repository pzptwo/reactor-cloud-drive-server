// 云盘服务端入口
#include "cloudserver.h"
#include "db.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>

CloudServer *g_server = nullptr;

void Stop(int sig)
{
    printf("收到信号 %d，停止服务\n", sig);
    if (g_server) { g_server->stop(); delete g_server; g_server = nullptr; }
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("usage: ./cloudserver <ip> <port>\n");
        printf("example: ./cloudserver 127.0.0.1 8888\n");
        return -1;
    }
    signal(SIGTERM, Stop);
    signal(SIGINT, Stop);

    if (!DB::getInstance().init())
    {
        printf("数据库初始化失败\n");
        return -1;
    }

    g_server = new CloudServer(argv[1], atoi(argv[2]), 3);
    printf("CloudServer 启动，监听 %s:%s\n", argv[1], argv[2]);
    g_server->start();
    return 0;
}
