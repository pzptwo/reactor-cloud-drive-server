#ifndef CLOUDSERVER_H
#define CLOUDSERVER_H

// ============================================================
// 云盘服务端业务层（基于自研 Reactor 框架 TcpServer）
// 职责：
//   1. 连接管理：conns_（fd->连接）、usernames_（用户名->fd），均加锁
//   2. PDU 分包：接收 sep=0 原始字节，按 PDU 头（4字节总长含自身）拆包
//   3. 消息分发：handlePdu switch 分发到各业务处理
//   4. 转发：resend(name, pdu) 按用户名转发
// ============================================================
#include "TcpServer.h"
#include "Connection.h"
#include "protocol.h"
#include "db.h"
#include <map>
#include <string>
#include <mutex>
#include <cstdio>   // FILE（上传/下载文件句柄）

class CloudServer
{
public:
    CloudServer(const std::string &ip, uint16_t port, int subthreadnum);
    ~CloudServer();

    void start();
    void stop();

    // 按用户名转发整个 PDU（跨连接：私聊/群聊/好友请求/共享文件）
    void resend(const char *name, PDU *pdu);
    // 按 fd 发送整个 PDU（给指定连接回包）
    void sendTo(int fd, PDU *pdu);

private:
    // ---- 框架回调 ----
    void handleNewConnection(spConnection conn);
    void handleClose(spConnection conn);
    void handleError(spConnection conn);
    void handleMessage(spConnection conn, std::string &raw);
    void handleSendComplete(spConnection conn);
    void handleTimeout(EventLoop *loop);

    // ---- PDU 分发 ----
    void handlePdu(spConnection conn, PDU *pdu);

    // ---- 业务处理：用户 / 好友 / 聊天（移植自 mytcpsocket.cpp）----
    void handleRegister(spConnection conn, PDU *pdu);
    void handleLogin(spConnection conn, PDU *pdu);
    void handleAllOnline(spConnection conn, PDU *pdu);
    void handleSearchUser(spConnection conn, PDU *pdu);
    void handleAddUserReq(spConnection conn, PDU *pdu);
    void handleAddUserAgreed(spConnection conn, PDU *pdu);
    void handleAddUserRefuse(spConnection conn, PDU *pdu);
    void handleFlushFriend(spConnection conn, PDU *pdu);
    void handleDelFriend(spConnection conn, PDU *pdu);
    void handlePrivateChat(spConnection conn, PDU *pdu);
    void handleGroupChat(spConnection conn, PDU *pdu);

    // ---- 业务处理：文件管理（移植自 mytcpsocket.cpp）----
    void handleCreateDir(spConnection conn, PDU *pdu);
    void handleFlushDir(spConnection conn, PDU *pdu);
    void handleDelDir(spConnection conn, PDU *pdu);
    void handleDelFile(spConnection conn, PDU *pdu);
    void handleRenameFile(spConnection conn, PDU *pdu);
    void handleEntryDir(spConnection conn, PDU *pdu);
    void handleMoveFile(spConnection conn, PDU *pdu);

    // ---- 业务处理：文件传输（上传/下载/共享）----
    void handleUpdateFile(spConnection conn, PDU *pdu);        // 上传请求
    void writeUploadData(spConnection conn, const std::string &raw);  // 上传模式写文件
    void handleDownloadFile(spConnection conn, PDU *pdu);     // 下载请求
    void handleShareFile(spConnection conn, PDU *pdu);        // 共享请求
    void handleShareNoteResponse(spConnection conn, PDU *pdu); // 接收者确认共享
    void handlePing(spConnection conn, PDU *pdu);              // 压测基准：PING->PONG（纯内存）

    // 上传状态机：连接在上传模式时，收到的原始数据直接写文件（不走 PDU 分包）
    struct UploadState {
        bool active = false;
        FILE *fp = nullptr;
        long long total = 0;
        long long received = 0;
    };
    std::map<int, UploadState> uploads_;

    TcpServer tcpserver_;
    DB &db_;

    std::mutex mmutex_;
    std::map<int, spConnection> conns_;        // fd -> 连接（业务层连接表）
    std::map<std::string, int> usernames_;     // 用户名 -> fd（登录成功后登记）
    std::map<int, std::string> recvbuf_;       // fd -> 接收缓冲（粘包/半包）
};

#endif // CLOUDSERVER_H
