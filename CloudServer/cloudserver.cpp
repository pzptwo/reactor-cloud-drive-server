#include "cloudserver.h"
#include "fileutil.h"
#include "Timestamp.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================
// 业务逻辑移植自 TcpServer/mytcpsocket.cpp（Qt 版），保持行为一致：
//   - caData 前 32 字节 = 参数1（如被加好友的用户名），后 32 字节 = 参数2（如登录名）
//   - 响应类型 = 对应请求类型的 RESPONSE
//   - 用户文件目录：./用户名（注册/登录时确保存在）
// ============================================================

CloudServer::CloudServer(const std::string &ip, uint16_t port, int subthreadnum)
    : tcpserver_(ip, port, subthreadnum), db_(DB::getInstance())
{
    tcpserver_.setnewConnectioncb(std::bind(&CloudServer::handleNewConnection, this, std::placeholders::_1));
    tcpserver_.setclosecb(std::bind(&CloudServer::handleClose, this, std::placeholders::_1));
    tcpserver_.seterrorcb(std::bind(&CloudServer::handleError, this, std::placeholders::_1));
    tcpserver_.setslovemessagecb(std::bind(&CloudServer::handleMessage, this, std::placeholders::_1, std::placeholders::_2));
    tcpserver_.setsendCompletecb(std::bind(&CloudServer::handleSendComplete, this, std::placeholders::_1));
    tcpserver_.setepolltimeoutcb(std::bind(&CloudServer::handleTimeout, this, std::placeholders::_1));
}

CloudServer::~CloudServer()
{
}

void CloudServer::start()
{
    tcpserver_.start();
}

void CloudServer::stop()
{
    tcpserver_.stop();
}

// ---------------- 连接管理 ----------------

void CloudServer::handleNewConnection(spConnection conn)
{
    std::lock_guard<std::mutex> lock(mmutex_);
    conns_[conn->fd()] = conn;
    printf("%s 新连接 fd=%d ip=%s port=%d | 当前连接数=%zu\n",
           Timestamp::now().toString().c_str(), conn->fd(),
           conn->ip().c_str(), conn->port(), conns_.size());
}

void CloudServer::handleClose(spConnection conn)
{
    int fd = conn->fd();
    std::string name;
    {
        std::lock_guard<std::mutex> lock(mmutex_);
        for (auto it = usernames_.begin(); it != usernames_.end(); ++it)
            if (it->second == fd) { name = it->first; usernames_.erase(it); break; }
        conns_.erase(fd);
        recvbuf_.erase(fd);
        printf("%s 连接关闭 fd=%d | 当前连接数=%zu%s%s\n",
               Timestamp::now().toString().c_str(), fd, conns_.size(),
               name.empty() ? "" : " | 下线用户: ",
               name.empty() ? "" : name.c_str());
    }
    if (!name.empty()) db_.handleoffline(name.c_str());
}

void CloudServer::handleError(spConnection conn)
{
    handleClose(conn);
}

// ---------------- PDU 分包与分发 ----------------

void CloudServer::handleMessage(spConnection conn, std::string &raw)
{
    int fd = conn->fd();

    // 上传模式：收到的原始数据直接写文件（不走 PDU 分包）
    auto uit = uploads_.find(fd);
    if (uit != uploads_.end() && uit->second.active) {
        writeUploadData(conn, raw);
        return;
    }

    std::string &buf = recvbuf_[fd];
    buf += raw;

    while (buf.size() >= 4)
    {
        uint len = 0;
        memcpy(&len, buf.data(), 4);
        if (len < sizeof(PDU)) { buf.clear(); break; }
        if (buf.size() < len) break;

        PDU *pdu = (PDU *)malloc(len);
        memcpy(pdu, buf.data(), len);
        buf.erase(0, len);
        handlePdu(conn, pdu);
        free(pdu);

        // 若刚进入上传模式（收到 UPDATE_FILE_RESPEST），缓冲剩余数据就是文件内容，
        // 直接交给写文件逻辑，避免粘包时文件数据残留在分包缓冲中
        auto uit = uploads_.find(fd);
        if (uit != uploads_.end() && uit->second.active && !buf.empty()) {
            std::string remain = buf;
            buf.clear();
            writeUploadData(conn, remain);
        }
    }
}

void CloudServer::handlePdu(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    printf("收到 PDU: fd=%d type=%u pdulen=%u msglen=%u\n",
           fd, pdu->uiMsgType_, pdu->uiPDULen_, pdu->uiMsgLen_);

    switch (pdu->uiMsgType_)
    {
    case ENUM_MSG_TYPE_REGISTER_RESPEST:    handleRegister(conn, pdu); break;
    case ENUM_MSG_TYPE_LOGIN_RESPEST:       handleLogin(conn, pdu); break;
    case ENUM_MSG_TYPE_ALL_ONLINE_RESPEST:  handleAllOnline(conn, pdu); break;
    case ENUM_MSG_TYPE_SEARCH_USER_RESPEST: handleSearchUser(conn, pdu); break;
    case ENUM_MSG_TYPE_ADD_USER_RESPEST:    handleAddUserReq(conn, pdu); break;
    case ENUM_MSG_TYPE_ADD_USER_AGREED:     handleAddUserAgreed(conn, pdu); break;
    case ENUM_MSG_TYPE_ADD_USER_REFUSE:     handleAddUserRefuse(conn, pdu); break;
    case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPEST: handleFlushFriend(conn, pdu); break;
    case ENUM_MSG_TYPE_DEL_FRIEND_RESPEST:  handleDelFriend(conn, pdu); break;
    case ENUM_MSG_TYPE_PRIVATE_CHAT_RESPEST: handlePrivateChat(conn, pdu); break;
    case ENUM_MSG_TYPE_GROUP_CHAT_RESPEST:  handleGroupChat(conn, pdu); break;
    case ENUM_MSG_TYPE_CREATE_DIR_RESPEST:  handleCreateDir(conn, pdu); break;
    case ENUM_MSG_TYPE_FLUSH_DIR_RESPEST:   handleFlushDir(conn, pdu); break;
    case ENUM_MSG_TYPE_DEL_DIR_RESPEST:     handleDelDir(conn, pdu); break;
    case ENUM_MSG_TYPE_DEL_FILE_RESPEST:    handleDelFile(conn, pdu); break;
    case ENUM_MSG_TYPE_RENAME_FILE_RESPEST: handleRenameFile(conn, pdu); break;
    case ENUM_MSG_TYPE_ENTRY_DIR_RESPEST:   handleEntryDir(conn, pdu); break;
    case ENUM_MSG_TYPE_MOVE_FILE_RESPEST:   handleMoveFile(conn, pdu); break;
    case ENUM_MSG_TYPE_UPDATE_FILE_RESPEST:   handleUpdateFile(conn, pdu); break;
    case ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPEST: handleDownloadFile(conn, pdu); break;
    case ENUM_MSG_TYPE_SHARE_FILE_RESPEST:    handleShareFile(conn, pdu); break;
    case ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPONSE: handleShareNoteResponse(conn, pdu); break;
    case ENUM_MSG_TYPE_PING: handlePing(conn, pdu); break;
    default:
        printf("未处理的消息类型: %u\n", pdu->uiMsgType_);
        break;
    }
}

// ---------------- 业务处理 ----------------

void CloudServer::handleRegister(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char caName[32] = {0};
    char caPwd[32] = {0};
    memcpy(caName, pdu->caData, 32);
    memcpy(caPwd, pdu->caData + 32, 32);
    caName[31] = '\0';
    caPwd[31] = '\0';

    bool ret = db_.handleregister(caName, caPwd);
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_REGISTER_RESPONSE;
    if (ret)
    {
        strcpy(resp->caData, REGISTER_OK);
        // 注册成功：创建该用户的根目录
        fileutil::createDir(std::string("./") + caName);
    }
    else
    {
        strcpy(resp->caData, REGISTER_FALSE);
    }
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleLogin(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char caName[32] = {0};
    char caPwd[32] = {0};
    memcpy(caName, pdu->caData, 32);
    memcpy(caPwd, pdu->caData + 32, 32);
    caName[31] = '\0';
    caPwd[31] = '\0';

    bool ret = db_.handlelogin(caName, caPwd);
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_LOGIN_RESPONSE;
    if (ret)
    {
        strcpy(resp->caData, LOGIN_OK);
        // 登录成功：登记用户名 -> fd（供 resend 转发），并确保用户目录存在
        {
            std::lock_guard<std::mutex> lock(mmutex_);
            usernames_[caName] = fd;
        }
        fileutil::createDir(std::string("./") + caName);
    }
    else
    {
        strcpy(resp->caData, LOGIN_FALSE);
    }
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleAllOnline(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    std::vector<std::string> list = db_.handleallonline();
    uint uiMsgLen = (uint)list.size() * 32;   // 每个用户名固定 32 字节
    PDU *resp = mkPDU(uiMsgLen);
    resp->uiMsgType_ = ENUM_MSG_TYPE_ALL_ONLINE_RESPONSE;
    for (size_t i = 0; i < list.size(); i++)
        memcpy((char *)resp->caMsg + i * 32, list[i].c_str(), list[i].size());
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleSearchUser(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char caName[32] = {0};
    memcpy(caName, pdu->caData, 32);
    caName[31] = '\0';

    int ret = db_.handleSearchUser(caName);
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_SEARCH_USER_RESPONSE;
    if (ret == 1)       strcpy(resp->caData, SEARCH_ONLINE);
    else if (ret == 0)  strcpy(resp->caData, SEARCH_OFFLINE);
    else                strcpy(resp->caData, SEARCH_NOPERSON);
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleAddUserReq(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char caAddUserName[32] = {0};   // 被加的人（前 32 字节）
    char caLoginName[32] = {0};     // 发起加好友的人（后 32 字节）
    memcpy(caAddUserName, pdu->caData, 32);
    memcpy(caLoginName, pdu->caData + 32, 32);
    caAddUserName[31] = '\0';
    caLoginName[31] = '\0';

    int ret = db_.handleAddUserCheak(caLoginName, caAddUserName);
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_ADD_USER_RESPONSE;
    if (ret == -1)         strcpy(resp->caData, UNKNOWN_ERROR);
    else if (ret == 0)     strcpy(resp->caData, EXITED_FRIEND);
    else if (ret == 1)
    {
        // 对方在线：把原请求转发给对方，提示"有人加你"
        strcpy(resp->caData, SEND_ADD_FRIEND);
        resend(caAddUserName, pdu);
    }
    else if (ret == 2)     strcpy(resp->caData, ADD_FRIEND_OFFLINE);
    else if (ret == 3)     strcpy(resp->caData, NOT_EXISTED);
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleAddUserAgreed(spConnection conn, PDU *pdu)
{
    char caAddUserName[32] = {0};
    char caLoginName[32] = {0};
    memcpy(caAddUserName, pdu->caData, 32);
    memcpy(caLoginName, pdu->caData + 32, 32);
    caAddUserName[31] = '\0';
    caLoginName[31] = '\0';
    // 对方同意：写入好友关系（双向），并把同意结果转发给发起者
    db_.handleAddUser(caLoginName, caAddUserName);
    resend(caLoginName, pdu);
}

void CloudServer::handleAddUserRefuse(spConnection conn, PDU *pdu)
{
    // 拒绝：原样转发给发起者即可
    char caLoginName[32] = {0};
    memcpy(caLoginName, pdu->caData + 32, 32);
    caLoginName[31] = '\0';
    resend(caLoginName, pdu);
}

void CloudServer::handleFlushFriend(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    // caData = 当前登录用户名
    char caLoginName[32] = {0};
    memcpy(caLoginName, pdu->caData, 32);
    caLoginName[31] = '\0';

    std::vector<std::string> list = db_.handleFlushFriend(caLoginName);
    uint uiMsgLen = (uint)list.size() * 32;
    PDU *resp = mkPDU(uiMsgLen);
    resp->uiMsgType_ = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPONSE;
    for (size_t i = 0; i < list.size(); i++)
        memcpy((char *)resp->caMsg + i * 32, list[i].c_str(), list[i].size());
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleDelFriend(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char caLoginName[32] = {0};
    char caFrinedName[32] = {0};
    memcpy(caLoginName, pdu->caData, 32);        // 登录名（发起者）
    memcpy(caFrinedName, pdu->caData + 32, 32);  // 被删的好友
    caLoginName[31] = '\0';
    caFrinedName[31] = '\0';

    if (db_.handleDelFriend(caLoginName, caFrinedName))
    {
        PDU *resp = mkPDU(0);
        resp->uiMsgType_ = ENUM_MSG_TYPE_DEL_FRIEND_RESPONSE;
        memcpy(resp->caData, DEL_FRIEND_OK, strlen(DEL_FRIEND_OK) + 1);   // 按实际长度拷贝，避免越界读
        sendTo(fd, resp);
        free(resp);
    }
    // 通知被删的人"你被删了"（转发原请求）
    resend(caFrinedName, pdu);
}

void CloudServer::handlePrivateChat(spConnection conn, PDU *pdu)
{
    // 私聊：caData 后 32 字节是接收方用户名，原样转发
    char caRecvName[32] = {0};
    memcpy(caRecvName, pdu->caData + 32, 32);
    caRecvName[31] = '\0';
    resend(caRecvName, pdu);
}

void CloudServer::handleGroupChat(spConnection conn, PDU *pdu)
{
    // 群聊：转发给所有在线好友（caData = 发起者用户名）
    char caLoginName[32] = {0};
    memcpy(caLoginName, pdu->caData, 32);
    caLoginName[31] = '\0';

    std::vector<std::string> list = db_.handleGroupChat(caLoginName);
    for (auto &name : list)
        resend(name.c_str(), pdu);
}

// ---------------- 文件管理业务（移植自 mytcpsocket.cpp）----------------

// 从 PDU caMsg 提取路径（去掉可能的尾部 '\0'）
static std::string pathOf(PDU *pdu)
{
    if (pdu->uiMsgLen_ <= 0) return "";
    std::string s((char *)pdu->caMsg, pdu->uiMsgLen_);
    size_t end = s.find('\0');
    if (end != std::string::npos) s = s.substr(0, end);
    return s;
}

void CloudServer::handleCreateDir(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    std::string cur = pathOf(pdu);               // caMsg = 当前路径
    char strName[32] = {0};
    memcpy(strName, pdu->caData + 32, 32);       // caData+32 = 新文件夹名
    strName[31] = '\0';

    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_CREATE_DIR_RESPONSE;
    if (fileutil::exists(cur)) {
        std::string newPath = fileutil::join(cur, strName);
        if (fileutil::exists(newPath))
            strcpy(resp->caData, FILE_EXIST);        // 重名
        else {
            strcpy(resp->caData, FILE_CREATE_OK);
            fileutil::createDir(newPath);
        }
    } else {
        strcpy(resp->caData, DIR_NOT_EXISTED);       // 根目录不存在
    }
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleFlushDir(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    std::string path = pathOf(pdu);                  // caMsg = 要刷新的路径
    std::vector<FileInfo> list = fileutil::listDir(path);
    uint uiMsgLen = (uint)(sizeof(FileInfo) * list.size());
    PDU *resp = mkPDU(uiMsgLen);
    resp->uiMsgType_ = ENUM_MSG_TYPE_FLUSH_DIR_RESPONSE;
    for (size_t i = 0; i < list.size(); i++)
        memcpy(((FileInfo *)resp->caMsg) + i, &list[i], sizeof(FileInfo));
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleDelDir(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char strName[32] = {0};
    memcpy(strName, pdu->caData, 32);                // caData = 目录名
    strName[31] = '\0';
    std::string path = fileutil::join(pathOf(pdu), strName);

    bool ret = false;
    if (fileutil::isDir(path))                       // 只有目录才删
        ret = fileutil::deleteDirRecursive(path);
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_DEL_DIR_RESPONSE;
    if (ret) strcpy(resp->caData, DEL_DIR_OK);
    else     strcpy(resp->caData, DEL_DIR_FLASE);
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleDelFile(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char strName[32] = {0};
    memcpy(strName, pdu->caData, 32);                // caData = 文件名
    strName[31] = '\0';
    std::string path = fileutil::join(pathOf(pdu), strName);

    bool ret = false;
    if (fileutil::isDir(path))
        ret = false;                                 // 目录走 DEL_DIR
    else if (fileutil::exists(path))
        ret = fileutil::deleteFile(path);
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_DEL_FILE_RESPONSE;
    if (ret) strcpy(resp->caData, DEL_FILE_OK);
    else     strcpy(resp->caData, DEL_FILE_FLASE);
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleRenameFile(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char strOldName[32] = {0};
    char strNewName[32] = {0};
    memcpy(strOldName, pdu->caData, 32);             // caData = 旧名
    memcpy(strNewName, pdu->caData + 32, 32);        // caData+32 = 新名
    strOldName[31] = '\0';
    strNewName[31] = '\0';
    std::string base = pathOf(pdu);

    bool ret = fileutil::renameFile(fileutil::join(base, strOldName),
                                    fileutil::join(base, strNewName));
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_RENAME_FILE_RESPONSE;
    if (ret) strcpy(resp->caData, RENAME_OK);
    else     strcpy(resp->caData, RENAME_FLASE);
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleEntryDir(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char strName[32] = {0};
    memcpy(strName, pdu->caData, 32);                // caData = 目录名
    strName[31] = '\0';
    std::string path = fileutil::join(pathOf(pdu), strName);

    if (fileutil::isDir(path)) {
        // 进入成功：返回目录列表（类型用 FLUSH_DIR_RESPONSE，与 Qt 版一致，客户端用同函数刷新）
        std::vector<FileInfo> list = fileutil::listDir(path);
        uint uiMsgLen = (uint)(sizeof(FileInfo) * list.size());
        PDU *resp = mkPDU(uiMsgLen);
        resp->uiMsgType_ = ENUM_MSG_TYPE_FLUSH_DIR_RESPONSE;
        for (size_t i = 0; i < list.size(); i++)
            memcpy(((FileInfo *)resp->caMsg) + i, &list[i], sizeof(FileInfo));
        sendTo(fd, resp);
        free(resp);
    } else {
        // 不是目录（文件或不存在）：进入失败
        PDU *resp = mkPDU(0);
        resp->uiMsgType_ = ENUM_MSG_TYPE_ENTRY_DIR_RESPONSE;
        strcpy(resp->caData, ENTRY_DIR_FLASE);
        sendTo(fd, resp);
        free(resp);
    }
}

void CloudServer::handleMoveFile(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    // caData = "%d %d %s"（源路径长、目标路径长、文件名）；caMsg = srcPath + desPath
    int srcLen = 0, desLen = 0;
    char moveFileName[32] = {0};
    sscanf(pdu->caData, "%d %d %s", &srcLen, &desLen, moveFileName);

    std::string srcPath((char *)pdu->caMsg, srcLen);
    // 客户端布局：srcPath(0..srcLen-1) + '\0'分隔 + desPath(srcLen+1..)
    // 与 book.cpp selectMoveDir 的写入偏移 (srcLen+1) 严格对应
    std::string desPath((char *)pdu->caMsg + srcLen + 1, desLen);

    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_MOVE_FILE_RESPONSE;
    if (fileutil::isDir(desPath)) {
        // 目标路径是目录：把源移动为 目标目录/文件名
        // 注：Qt 版此处判断 isFile（会导致"移到文件夹"失败），这里修正为 isDir
        std::string newPath = fileutil::join(desPath, moveFileName);
        if (fileutil::renameFile(srcPath, newPath))
            strcpy(resp->caData, MOVE_DIR_OK);
        else
            strcpy(resp->caData, COMMEN_ERR);
    } else {
        strcpy(resp->caData, MOVE_DIR_FLASE);
    }
    sendTo(fd, resp);
    free(resp);
}

// ---------------- 文件传输业务（上传/下载/共享）----------------

void CloudServer::writeUploadData(spConnection conn, const std::string &raw)
{
    int fd = conn->fd();
    auto it = uploads_.find(fd);
    if (it == uploads_.end() || !it->second.fp) return;
    UploadState &st = it->second;

    size_t written = fwrite(raw.data(), 1, raw.size(), st.fp);
    st.received += (long long)written;

    if (st.received >= st.total) {
        fclose(st.fp);
        uploads_.erase(fd);
        // 上传完成：回 UPDATE_FILE_RESPONSE UPDATE_OK
        PDU *resp = mkPDU(0);
        resp->uiMsgType_ = ENUM_MSG_TYPE_UPDATE_FILE_RESPONSE;
        strcpy(resp->caData, UPDATE_OK);
        sendTo(fd, resp);
        free(resp);
        printf("上传完成 fd=%d 共 %lld 字节\n", fd, st.total);
    }
}

void CloudServer::handleUpdateFile(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    // caData = "文件名 文件大小"；caMsg = 目标路径
    char strFileName[32] = {0};
    long long fileSize = 0;
    sscanf(pdu->caData, "%s %lld", strFileName, &fileSize);
    strFileName[31] = '\0';
    std::string path = fileutil::join(pathOf(pdu), strFileName);

    UploadState st;
    st.fp = fopen(path.c_str(), "wb");
    if (st.fp) {
        st.active = true;
        st.total = fileSize;
        st.received = 0;
        uploads_[fd] = st;
        printf("开始上传 fd=%d 文件=%s 大小=%lld\n", fd, path.c_str(), fileSize);
    }
    // Qt 版：打开失败不反馈；上传完成后统一回 UPDATE_FILE_RESPONSE
}

void CloudServer::handleDownloadFile(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    char strFileName[32] = {0};
    memcpy(strFileName, pdu->caData, 32);
    strFileName[31] = '\0';
    std::string path = fileutil::join(pathOf(pdu), strFileName);

    long long fileSize = fileutil::getFileSize(path);
    // 先回包：文件名 + 大小（客户端据此进入"下载写入模式"）
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPONSE;
    sprintf(resp->caData, "%s %lld", strFileName, fileSize);
    sendTo(fd, resp);
    free(resp);

    if (fileSize <= 0) return;

    // 分块推送文件内容（4096 一块；第 1 版同步发送，大文件流控留待任务 10 优化）
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return;
    char buf[4096];
    long long remaining = fileSize;
    while (remaining > 0) {
        size_t want = sizeof(buf) < (size_t)remaining ? sizeof(buf) : (size_t)remaining;
        size_t n = fread(buf, 1, want, fp);
        if (n <= 0) break;
        {
            std::lock_guard<std::mutex> lock(mmutex_);   // 保护 conns_ 查找
            auto it = conns_.find(fd);
            if (it == conns_.end()) break;
            it->second->sendto_ob(buf, n);
        }
        remaining -= (long long)n;
    }
    fclose(fp);
    printf("下载完成 fd=%d 文件=%s 大小=%lld\n", fd, path.c_str(), fileSize);
}

void CloudServer::handleShareFile(spConnection conn, PDU *pdu)
{
    int fd = conn->fd();
    // caData = "发送者名 接收者数 num"；caMsg = 接收者名*32 + 文件路径
    int num = 0;
    char caSendName[32] = {0};
    sscanf(pdu->caData, "%s %d", caSendName, &num);
    caSendName[31] = '\0';

    // 构造 SHARE_FILE_NOTE：caData=发送者名，caMsg=文件路径（跳过 num*32 接收者名）
    uint pathLen = pdu->uiMsgLen_ - (uint)num * 32;
    PDU *note = mkPDU(pathLen);
    note->uiMsgType_ = ENUM_MSG_TYPE_SHARE_FILE_NOTE;
    strcpy(note->caData, caSendName);
    if (pathLen > 0)
        memcpy(note->caMsg, (char *)pdu->caMsg + (size_t)num * 32, pathLen);

    // 转发给每个接收者
    char caRecvName[32] = {0};
    for (int i = 0; i < num; i++) {
        memset(caRecvName, 0, sizeof(caRecvName));
        memcpy(caRecvName, (char *)pdu->caMsg + (size_t)i * 32, 32);
        caRecvName[31] = '\0';
        resend(caRecvName, note);
    }
    free(note);

    // 回 SHARE_FILE_RESPONSE
    PDU *resp = mkPDU(0);
    resp->uiMsgType_ = ENUM_MSG_TYPE_SHARE_FILE_RESPONSE;
    strcpy(resp->caData, "share file ok");
    sendTo(fd, resp);
    free(resp);
}

void CloudServer::handleShareNoteResponse(spConnection conn, PDU *pdu)
{
    // caData = 接收者名；caMsg = 共享文件路径
    char caRecvName[32] = {0};
    memcpy(caRecvName, pdu->caData, 32);
    caRecvName[31] = '\0';
    std::string sharePath = pathOf(pdu);
    if (sharePath.empty()) return;

    // 拷贝到 ./接收者/文件名（文件用 copyFile，文件夹用 copyDirRecursive）
    std::string recvPath = std::string("./") + caRecvName;
    fileutil::createDir(recvPath);
    size_t pos = sharePath.rfind('/');
    std::string fileName = (pos == std::string::npos) ? sharePath : sharePath.substr(pos + 1);
    recvPath = fileutil::join(recvPath, fileName);

    if (fileutil::isDir(sharePath))
        fileutil::copyDirRecursive(sharePath, recvPath);
    else if (fileutil::exists(sharePath))
        fileutil::copyFile(sharePath, recvPath);
    printf("共享文件 %s -> %s\n", sharePath.c_str(), recvPath.c_str());
}

void CloudServer::handlePing(spConnection conn, PDU *pdu)
{
    // 压测基准消息：把类型翻成 PONG 原样回给客户端，纯内存操作（不碰磁盘/数据库）
    // 用于隔离测框架的真实网络吞吐（对比扫盘类业务消息）
    pdu->uiMsgType_ = ENUM_MSG_TYPE_PONG;
    sendTo(conn->fd(), pdu);
}

// ---------------- 发送 ----------------

void CloudServer::handleSendComplete(spConnection conn)
{
}

void CloudServer::handleTimeout(EventLoop *loop)
{
}

void CloudServer::sendTo(int fd, PDU *pdu)
{
    std::lock_guard<std::mutex> lock(mmutex_);
    auto it = conns_.find(fd);
    if (it == conns_.end()) return;
    it->second->sendto_ob((char *)pdu, pdu->uiPDULen_);
}

void CloudServer::resend(const char *name, PDU *pdu)
{
    std::lock_guard<std::mutex> lock(mmutex_);
    auto it = usernames_.find(name);
    if (it == usernames_.end()) return;
    auto cit = conns_.find(it->second);
    if (cit == conns_.end()) return;
    cit->second->sendto_ob((char *)pdu, pdu->uiPDULen_);
}
