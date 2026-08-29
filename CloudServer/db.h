#ifndef DB_H
#define DB_H

// ============================================================
// 数据库模块（Linux 版，替代 Qt 的 opedb）
// 基于 sqlite3 C API，表结构与 Qt 版完全一致：
//   usrInfo(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, pwd TEXT, online INTEGER)
//   friend (id INTEGER, friendid INTEGER)
// 线程安全：所有方法内部加全局互斥锁，串行化访问
// ============================================================
#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>

class DB
{
public:
    static DB &getInstance();

    bool init();   // 打开 cloudDrive.db + 建表（IF NOT EXISTS）+ 打印现有用户

    // ---- 用户 ----
    bool handleregister(const char *caName, const char *caPwd);
    bool handlelogin(const char *caName, const char *caPwd);
    void handleoffline(const char *caName);
    std::vector<std::string> handleallonline();
    int handleSearchUser(const char *caName);   // 1在线 / 0离线 / -1不存在

    // ---- 好友 ----
    int handleAddUserCheak(const char *caLoginName, const char *caAddUserName);
        // -1参数错 / 0已是好友 / 1对方在线 / 2对方离线 / 3对方不存在
    void handleAddUser(const char *caLoginName, const char *caAddUserName);
    std::vector<std::string> handleFlushFriend(const char *caLoginName);
    bool handleDelFriend(const char *caLoginName, const char *caAddUserName);
    std::vector<std::string> handleGroupChat(const char *caLoginName);

    // ---- 测试/注销辅助：删除用户及其好友关系 ----
    bool removeuser(const char *caName);

private:
    DB() = default;
    ~DB();
    DB(const DB &) = delete;
    DB &operator=(const DB &) = delete;

    sqlite3 *db_ = nullptr;
    std::mutex mutex_;   // 全局互斥，串行化所有数据库操作
};

#endif // DB_H
