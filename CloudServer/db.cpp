#include "db.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// MySQL 连接参数（对应 VM 上建好的库/用户）
static const char *DB_HOST = "localhost";
static const char *DB_USER = "cloud";
static const char *DB_PASS = "123456";
static const char *DB_NAME = "clouddrive";

// 执行一条 SQL（无结果集），打印错误
static bool execSql(MYSQL *db, const char *sql)
{
    if (mysql_query(db, sql) != 0) {
        fprintf(stderr, "SQL 执行失败: %s\n  SQL: %s\n", mysql_error(db), sql);
        return false;
    }
    return true;
}

// 查询第一行第一列的 int（找不到返回 def）
static int queryInt(MYSQL *db, const char *sql, int def = 0)
{
    if (mysql_query(db, sql) != 0) return def;
    MYSQL_RES *res = mysql_store_result(db);
    if (!res) return def;
    int val = def;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && row[0]) val = atoi(row[0]);
    mysql_free_result(res);
    return val;
}

// 查询 name 列填充到 out；dedup=true 时去重
static void queryNames(MYSQL *db, const char *sql, std::vector<std::string> &out, bool dedup = false)
{
    if (mysql_query(db, sql) != 0) return;
    MYSQL_RES *res = mysql_store_result(db);
    if (!res) return;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (!row[0]) continue;
        std::string s(row[0]);
        if (dedup) {
            bool dup = false;
            for (auto &x : out)
                if (x == s) { dup = true; break; }
            if (dup) continue;
        }
        out.push_back(s);
    }
    mysql_free_result(res);
}

DB &DB::getInstance()
{
    static DB instance;
    return instance;
}

DB::~DB()
{
    if (db_) mysql_close(db_);
}

bool DB::init()
{
    std::lock_guard<std::mutex> lock(mutex_);
    db_ = mysql_init(nullptr);
    if (!db_) {
        fprintf(stderr, "mysql_init 失败\n");
        return false;
    }
    // 断线自动重连 + utf8mb4（支持中文）
    bool reconnect = true;
    mysql_options(db_, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_set_character_set(db_, "utf8mb4");

    if (!mysql_real_connect(db_, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, nullptr, 0)) {
        fprintf(stderr, "连接 MySQL 失败: %s\n", mysql_error(db_));
        return false;
    }

    // 建表（IF NOT EXISTS 兼容已存在的库）
    execSql(db_,
        "CREATE TABLE IF NOT EXISTS usrInfo ("
        " id INT AUTO_INCREMENT PRIMARY KEY,"
        " name VARCHAR(64) UNIQUE,"
        " pwd VARCHAR(64),"
        " online INT DEFAULT 0)");
    execSql(db_,
        "CREATE TABLE IF NOT EXISTS friend ("
        " id INT,"
        " friendid INT)");

    // 崩溃自愈：上次进程若被强杀(SIGKILL)或异常退出，online 可能残留 1
    // 登录查询要求 online=0，残留会导致"已在线"无法重登，启动时统一复位
    execSql(db_, "update usrInfo set online=0");

    // 打印现有用户（对应 Qt init 的日志）
    printf("=== 当前用户列表 ===\n");
    if (mysql_query(db_, "select id,name,pwd,online from usrInfo") == 0) {
        MYSQL_RES *res = mysql_store_result(db_);
        if (res) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)))
                printf("  id=%s name=%s pwd=%s online=%s\n", row[0], row[1], row[2], row[3]);
            mysql_free_result(res);
        }
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
    // 找一条"名字+密码对 + 不在线"的记录，存在才允许登录
    snprintf(sql, sizeof(sql),
             "select id from usrInfo where name='%s' and pwd='%s' and online=0", caName, caPwd);
    int id = queryInt(db_, sql, -1);
    if (id < 0) return false;
    snprintf(sql, sizeof(sql),
             "update usrInfo set online=1 where name='%s' and pwd='%s'", caName, caPwd);
    execSql(db_, sql);
    return true;
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
    return queryInt(db_, sql, -1);   // 1 在线 / 0 离线 / -1 不存在
}

int DB::handleAddUserCheak(const char *caLoginName, const char *caAddUserName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!caLoginName || !caAddUserName) return -1;

    // 1) 是否已是好友（双向）
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT count(*) FROM friend "
             "WHERE (id = (SELECT id FROM usrInfo WHERE name='%1$s') AND friendid = (SELECT id FROM usrInfo WHERE name='%2$s')) "
             "OR (id = (SELECT id FROM usrInfo WHERE name='%2$s') AND friendid = (SELECT id FROM usrInfo WHERE name='%1$s'))",
             caLoginName, caAddUserName);
    if (queryInt(db_, sql, 0) > 0) return 0;   // 已是好友

    // 2) 检查对方在线状态
    snprintf(sql, sizeof(sql), "select online from usrInfo where name='%s'", caAddUserName);
    int online = queryInt(db_, sql, -1);
    if (online < 0) return 3;   // 对方不存在
    return online == 1 ? 1 : 2; // 1 在线 / 2 离线
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
