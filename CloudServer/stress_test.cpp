// stress_test.cpp — 高并发压测程序（与 stress_test.py 功能一致）
// 每个连接：建连 -> 发 FLUSH_DIR(type=23) 请求 -> 收响应(type=24) -> 统计速率
// 运行：./stress_test [连接数] [每连接请求数] [host] [port] [超时秒]
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static const int PDU_HEADER = 76;              // PDU 头长度，与 protocol.h 一致
static const uint32_t TYPE_FLUSH_DIR_REQ = 23; // 请求类型
static const uint32_t TYPE_FLUSH_DIR_RSP = 24; // 响应类型

struct Result { bool connErr = false; int ok = 0; };

// 构造 FLUSH_DIR 请求包：[总长][消息长][类型][caData 64][caMsg]
static std::string buildFlushPdu() {
    std::string path("./test1\0", 8);          // './test1' + '\0'，与 py 版一致
    uint32_t msglen = (uint32_t)path.size();
    uint32_t total  = PDU_HEADER + msglen;
    char hdr[12];
    memcpy(hdr,     &total,             4);    // 小端写总长（同 struct.pack('<I')）
    memcpy(hdr + 4, &msglen,            4);    // 小端写消息长
    memcpy(hdr + 8, &TYPE_FLUSH_DIR_REQ, 4);   // 小端写类型
    return std::string(hdr, 12) + std::string(64, '\0') + path;
}

// 精确接收 n 字节（循环收，处理粘包/半包）
static size_t recvExact(int fd, char* buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, buf + got, n - got, 0);
        if (r <= 0) break;
        got += (size_t)r;
    }
    return got;
}

// 单个连接的工作：建连 -> repeat 次请求/响应
static void worker(const std::string& host, int port, int repeat, int timeoutSec, Result& r) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { r.connErr = true; return; }
    struct timeval tv = { timeoutSec, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));  // 收包超时
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));  // 发包超时
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) != 0) {    // 阶段1：建连
        close(fd); r.connErr = true; return;
    }
    std::string pdu = buildFlushPdu();
    char buf[256];
    for (int i = 0; i < repeat; ++i) {                          // 阶段2：请求/响应
        if (send(fd, pdu.data(), (int)pdu.size(), 0) <= 0) break;
        if (recvExact(fd, buf, 4) != 4) break;                  // 先读 4 字节总长
        uint32_t total;
        memcpy(&total, buf, 4);
        if (recvExact(fd, buf, total - 4) != total - 4) break;  // 再读剩余包体
        uint32_t msgtype;
        memcpy(&msgtype, buf + 4, 4);                           // body[4:8] = 类型
        if (msgtype == TYPE_FLUSH_DIR_RSP) ++r.ok;              // 收到正确响应
    }
    close(fd);
}

int main(int argc, char** argv) {
    int nconn      = argc > 1 ? atoi(argv[1]) : 100;
    int repeat     = argc > 2 ? atoi(argv[2]) : 1;
    std::string host = argc > 3 ? argv[3] : "127.0.0.1";
    int port       = argc > 4 ? atoi(argv[4]) : 8888;
    int timeoutSec = argc > 5 ? atoi(argv[5]) : 10;

    std::cout << "[开始] " << nconn << " 连接 x " << repeat << " 请求 -> "
              << host << ":" << port << " (超时 " << timeoutSec << "s)\n";
    std::vector<Result> results(nconn);
    std::vector<std::thread> threads;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < nconn; ++i)                             // 并发开 N 个连接
        threads.emplace_back(worker, host, port, repeat, timeoutSec, std::ref(results[i]));
    for (auto& t : threads) t.join();
    double dur = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    int connOk = 0, okReq = 0;
    for (auto& r : results) { if (!r.connErr) ++connOk; okReq += r.ok; }
    int failReq = nconn * repeat - okReq;
    std::cout << "[结果] 连接成功=" << connOk << "/" << nconn << "\n";
    std::cout << "[结果] 请求成功=" << okReq << "/" << nconn * repeat
              << "  请求失败=" << failReq << "\n";
    std::cout << "[结果] 总耗时=" << dur << "s  连接速率≈"
              << (dur > 0 ? (int)(nconn / dur) : 0) << "/s  请求速率≈"
              << (dur > 0 ? (int)(okReq / dur) : 0) << "/s\n";
    return 0;
}
