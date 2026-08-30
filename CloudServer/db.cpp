#include "db.h"
#include <cstdio>
#include <cstring>

// 执行一条 SQL（无结果集），打印错误
static bool execSql(sqlite3 *db, const char *sql)
{
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL 执行失败: %s\n  SQL: %s\n", errmsg ? errmsg : "unknown", sql);
        if (errmsg) sqlite3_free(errmsg);
        return false;
    }
    return true;
}

// 查询第一行第一列的 int（找不到返回 def）
static int queryInt(sqlite3 *db, const char *sql, int def = 0)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return def;
    int val = def;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        val = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return val;
}

// 查询 name 列填充到 out；dedup=true 时去重
static void queryNames(sqlite3 *db, const char *sql, std::vector<std::string> &out, bool dedup = false)
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        if (!name) continue;
        std::string s((const char *)name);
        if (dedup) {
            bool dup = false;
            for (auto &x : out)
                if (x == s) { dup = true; break; }
            if (dup) continue;
        }
        out.push_back(s);
    }
    sqlite3_finalize(stmt);
}

DB &DB::getInstance()
{
    static DB instance;
    return instance;
}

DB::~DB()
{
    if (db_) sqlite3_close(db_);
}

bool DB::init()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (sqlite3_open("cloudDrive.db", &db_) != SQLITE_OK) {
        fprintf(stderr, "打开数据库失败: %s\n", db_ ? sqlite3_errmsg(db_) : "unknown");
        return false;
    }
    // 建表（与 Qt 版表结构一致；IF NOT EXISTS 兼容已有库文件）
    const char *sql =
        "CREATE TABLE IF NOT EXISTS usrInfo ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT UNIQUE,"
        " pwd TEXT,"
        " online INTEGER DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS friend ("
        " id INTEGER,"
        " friendid INTEGER);";
    if (!execSql(db_, sql)) return false;

    // 崩溃自愈：上次进程若被强杀(SIGKILL)或异常退出，online 可能残留 1
    // 登录查询要求 online=0，残留会导致"已在线"无法重登，启动时统一复位
    execSql(db_, "update usrInfo set online=0");

    // 打印现有用户（对应 Qt init 的日志）
    printf("=== 当前用户列表 ===\n");
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "select id,name,pwd,online from usrInfo", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  id=%d name=%s pwd=%s online=%d\n",
                   sqlite3_column_int(stmt, 0),
                   sqlite3_column_text(stmt, 1),
                   sqlite3_column_text(stmt, 2),
                   sqlite3_column_int(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }
    printf("====================\n");
    return true;
}

bool DB::handleregister(const char *caName, const char *caPwd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!caName || !caPwd) return false;
    char sql[512];
    snprintf(sql, sizeof(sql), "insert into usrInfo (name,pwd) values ('%s','%s')", caName, caPwd);
    return execSql(db_, sql);
}

bool DB::handlelogin(const char *caName, const char *caPwd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!caName || !caPwd) return false;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "select * from usrInfo where name='%s' and pwd='%s' and online=0", caName, caPwd);
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    if (found) {
        snprintf(sql, sizeof(sql),
                 "update usrInfo set online=1 where name='%s' and pwd='%s'", caName, caPwd);
        execSql(db_, sql);
    }
    return found;
}

void DB::handleoffline(const char *caName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!caName) return;
    char sql[256];
    snprintf(sql, sizeof(sql), "update usrInfo set online=0 where name='%s'", caName);
    execSql(db_, sql);
}

std::vector<std::string> DB::handleallonline()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ret;
    queryNames(db_, "select name from usrInfo where online=1", ret);
    return ret;
}

int DB::handleSearchUser(const char *caName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    char sql[256];
    snprintf(sql, sizeof(sql), "select online from usrInfo where name='%s'", caName);
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int ret = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return ret;   // 1 在线 / 0 离线
    }
    sqlite3_finalize(stmt);
    return -1;   // 不存在
}

int DB::handleAddUserCheak(const char *caLoginName, const char *caAddUserName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!caLoginName || !caAddUserName) return -1;

    // 1) 是否已是好友（双向）
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT * FROM friend "
             "WHERE (id = (SELECT id FROM usrInfo WHERE name='%1$s') AND friendid = (SELECT id FROM usrInfo WHERE name='%2$s')) "
             "OR (id = (SELECT id FROM usrInfo WHERE name='%2$s') AND friendid = (SELECT id FROM usrInfo WHERE name='%1$s'))",
             caLoginName, caAddUserName);
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    bool isFriend = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    if (isFriend) return 0;   // 已是好友

    // 2) 检查对方在线状态
    snprintf(sql, sizeof(sql), "select online from usrInfo where name='%s'", caAddUserName);
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int online = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return online == 1 ? 1 : 2;   // 1 在线 / 2 离线
    }
    sqlite3_finalize(stmt);
    return 3;   // 对方不存在
}

void DB::handleAddUser(const char *caLoginName, const char *caAddUserName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!caLoginName || !caAddUserName) return;
    char sql[256];
    snprintf(sql, sizeof(sql), "select id from usrInfo where name='%s'", caLoginName);
    int selfid = queryInt(db_, sql, -1);
    snprintf(sql, sizeof(sql), "select id from usrInfo where name='%s'", caAddUserName);
    int friendid = queryInt(db_, sql, -1);
    if (selfid < 0 || friendid < 0) return;
    snprintf(sql, sizeof(sql),
             "insert into friend (id,friendid) values (%d,%d),(%d,%d)",
             selfid, friendid, friendid, selfid);
    execSql(db_, sql);
}

std::vector<std::string> DB::handleFlushFriend(const char *caLoginName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> list;
    char sql[512];
    // 好友在对方列表（我作为 friendid）
    snprintf(sql, sizeof(sql),
             "select name from usrInfo where id in "
             "(select id from friend where friendid=(select id from usrInfo where name='%s')) and online=1",
             caLoginName);
    queryNames(db_, sql, list);
    // 我在对方列表（我作为 id），去重
    snprintf(sql, sizeof(sql),
             "select name from usrInfo where id in "
             "(select friendid from friend where id=(select id from usrInfo where name='%s')) and online=1",
             caLoginName);
    queryNames(db_, sql, list, true);
    return list;
}

bool DB::handleDelFriend(const char *caLoginName, const char *caAddUserName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!caLoginName || !caAddUserName) return false;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "delete from friend where id=(select id from usrInfo where name='%s') "
             "and friendid=(select id from usrInfo where name='%s')",
             caLoginName, caAddUserName);
    execSql(db_, sql);
    snprintf(sql, sizeof(sql),
             "delete from friend where id=(select id from usrInfo where name='%s') "
             "and friendid=(select id from usrInfo where name='%s')",
             caAddUserName, caLoginName);
    execSql(db_, sql);
    return true;
}

std::vector<std::string> DB::handleGroupChat(const char *caLoginName)
{
    return handleFlushFriend(caLoginName);
}

bool DB::removeuser(const char *caName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!caName) return false;
    char sql[256];
    // 删除好友关系（双向）
    snprintf(sql, sizeof(sql),
             "delete from friend where id in (select id from usrInfo where name='%s') "
             "or friendid in (select id from usrInfo where name='%s')",
             caName, caName);
    execSql(db_, sql);
    // 删除用户
    snprintf(sql, sizeof(sql), "delete from usrInfo where name='%s'", caName);
    return execSql(db_, sql);
}
