#ifndef DB_H
#define DB_H

// ============================================================
// 数据库模块（Linux 版，替代 Qt 的 opedb）
// 基于 MySQL C API（libmysqlclient），表结构与 Qt 版一致：
//   usrInfo(id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(64) UNIQUE, pwd VARCHAR(64), online INT DEFAULT 0)
//   friend (id INT, friendid INT)
// 线程安全：所有方法内部加全局互斥锁，串行化访问
// ============================================================
#include <string>
#include <vector>
#include <mutex>
#include <mysql/mysql.h>

class DB
{
public:
    static DB &getInstance();

    bool init();   // 连接 MySQL + 建表（IF NOT EXISTS）+ 打印现有用户

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

    MYSQL *db_ = nullptr;
    std::mutex mutex_;   // 全局互斥，串行化所有数据库操作
};

#endif // DB_H
