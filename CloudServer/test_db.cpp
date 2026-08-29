// 数据库模块单元测试：
// 覆盖注册/登录/离线/搜索/在线列表/加好友/刷新好友/删好友/群聊
#include "db.h"
#include <cassert>
#include <cstdio>
#include <string>

static bool contains(const std::vector<std::string> &v, const std::string &s)
{
    for (auto &x : v)
        if (x == s) return true;
    return false;
}

int main()
{
    const char *u1 = "tester_a";
    const char *u2 = "tester_b";
    const char *p1 = "123";
    const char *p2 = "456";

    DB &db = DB::getInstance();
    assert(db.init());

    // 清理历史残留（重复运行也能通过）
    db.removeuser(u1);
    db.removeuser(u2);

    // ---- 注册 ----
    assert(db.handleregister(u1, p1));
    assert(db.handleregister(u2, p2));
    assert(!db.handleregister(u1, p1));   // 重复注册（name 唯一）应失败

    // ---- 登录 ----
    assert(db.handlelogin(u1, p1));
    assert(!db.handlelogin(u1, p1));      // 已在线不能重复登录
    assert(db.handlelogin(u2, p2));

    // ---- 在线列表 ----
    auto online = db.handleallonline();
    assert(contains(online, u1) && contains(online, u2));

    // ---- 搜索用户 ----
    assert(db.handleSearchUser(u1) == 1);              // 在线
    assert(db.handleSearchUser("no_such_user_xyz") == -1);

    // ---- 加好友检查 ----
    assert(db.handleAddUserCheak(u1, u2) == 1);        // 未加好友且对方在线

    // ---- 加好友（双向）----
    db.handleAddUser(u1, u2);
    assert(db.handleAddUserCheak(u1, u2) == 0);        // 已是好友

    // ---- 刷新好友（双向 + 在线过滤）----
    auto friends1 = db.handleFlushFriend(u1);
    assert(contains(friends1, u2));
    auto friends2 = db.handleFlushFriend(u2);
    assert(contains(friends2, u1));

    // ---- 群聊（= 刷新好友）----
    auto group = db.handleGroupChat(u1);
    assert(group.size() >= 1);

    // ---- 删除好友（双向）----
    assert(db.handleDelFriend(u1, u2));
    assert(db.handleAddUserCheak(u1, u2) == 1);        // 删除后回到"非好友，对方在线"

    // ---- 下线 ----
    db.handleoffline(u1);
    db.handleoffline(u2);
    assert(db.handleSearchUser(u1) == 0);              // 离线
    assert(db.handleSearchUser(u2) == 0);

    // 清理测试数据
    db.removeuser(u1);
    db.removeuser(u2);

    printf("数据库测试全部通过\n");
    return 0;
}
