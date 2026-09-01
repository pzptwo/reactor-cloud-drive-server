# CloudDrive —— 基于自研 Reactor 高并发框架的多用户私有云网盘

基于**自研 C++ Reactor 高并发网络框架**（主从 Reactor + 线程池 + epoll）的多用户私有云网盘系统：服务端为 Linux 原生 C++（g++14），前端为 Windows Qt 5.15 客户端，二者通过自定义 **PDU 二进制协议**（字节级一致）通信。

## 第一部分 项目简介

### 1.1 项目背景

本项目前身是「myCloudDrive Qt 云盘系统」——Windows 上基于单线程 `QTcpServer` + SQLite 的 C/S 云盘。单线程模型在高并发下成为瓶颈（一个连接卡住会影响全部连接），且无法发挥多核性能。

升级方案：**服务端用自研 Reactor 高并发框架以纯 C++ 重写并迁移到 Linux**，客户端与 PDU 协议**完全复用、零改动**，只改 IP/端口配置即可连接新服务端。

### 1.2 项目目标

- **服务端**：基于自研 Reactor 框架（主从 Reactor + 线程池 + epoll），在 1000-2000 并发连接下稳定运行、无崩溃、无 CPU 空转、无连接泄漏
- **客户端**：完全复用现有 Qt 客户端（UI 与协议零改动）
- **协议**：复用 PDU 二进制协议，服务端/客户端字节级一致，天然支持粘包/半包

### 1.3 技术栈

| 端 | 技术 |
|---|---|
| 框架层 | C++14 + epoll（主从 Reactor、线程池、eventfd 唤醒、timerfd 定时） |
| 业务层 | CloudServer：17 类消息分发、MySQL（C API，由 SQLite 迁移） |
| 前端 | Qt 5.15.2（MinGW、C++17）：登录/好友/私聊/群聊/文件管理/共享 |
| 压测 | C++ `stress_test` + `test.sh` 一键压测；Python 联调脚本 |

### 1.4 仓库结构

```
cloud-drive/
├── framework/         # 自研 Reactor 高并发框架（与业务解耦，可复用）
├── CloudServer/       # 服务端业务层（协议分发、MySQL、文件工具、压测）
└── web/               # 前端 Qt 客户端（原独立仓库，完整历史并入）
```

### 1.5 开发历程（git 提交脉络）

| 阶段 | 提交 | 内容 |
|---|---|---|
| 框架 | init | 自研 Reactor 框架（主从 Reactor + 线程池 + epoll） |
| 业务 | feat | CloudServer 业务层：17 类消息分发、数据库、文件工具 |
| 联调 | fix×2 | 任务9：timerfd 空转、移动路径偏移差 1、强杀后在线残留自愈 |
| 压测 | fix | 任务10：修复 6 个框架级缺陷（EMFILE 越界崩溃、重复 remove 杀进程、EINTR、CPU 空转、backlog） |
| 存储 | feat | SQLite → MySQL（C API）迁移，新增 PING/PONG 纯内存压测基准 |
| 整合 | merge | 前端仓库并入 `web/`，前后端一体化，全部历史保留 |

## 第二部分 功能特性

### 2.1 功能总览

| 模块 | 功能 | 对应消息类型 |
|---|---|---|
| 用户系统 | 注册、登录、在线状态管理 | `REGISTER` / `LOGIN` / `ALL_ONLINE` |
| 好友系统 | 搜索用户、添加好友（同意/拒绝）、删除好友、刷新好友列表 | `SEARCH_USER` / `ADD_USER` / `ADD_USER_AGREED` / `ADD_USER_REFUSE` / `DEL_FRIEND` / `FLUSH_FRIEND` |
| 即时通讯 | 私聊、群聊 | `PRIVATE_CHAT` / `GROUP_CHAT` |
| 文件夹管理 | 创建/删除/进入/返回/重命名/移动文件夹、刷新目录 | `CREATE_DIR` / `DEL_DIR` / `ENTRY_DIR` / `RENAME_FILE` / `MOVE_FILE` / `FLUSH_DIR` |
| 文件传输 | 上传、下载、删除文件、共享给好友 | `UPDATE_FILE` / `DOWNLOAD_FILE` / `DEL_FILE` / `SHARE_FILE` / `SHARE_FILE_NOTE` |
| 压测基准 | PING/PONG 纯内存基准消息（无磁盘/DB，仅测吞吐） | `PING` / `PONG` |

### 2.2 各模块细节

**用户系统**
- 注册、登录（用户名 32 字节 + 密码 32 字节，存于 `caData`）
- 在线状态管理：服务端维护「用户名 → fd」映射；客户端异常断开/强杀后，服务端检测到连接关闭即清除在线状态，下次可正常重登（断线自愈）

**好友系统**
- 搜索用户、发起添加好友请求、对方同意/拒绝双向确认
- 在线用户列表、好友列表实时刷新、删除好友

**即时通讯**
- 私聊：消息走 PDU `caMsg` 变长区
- 群聊：群发给在线相关用户

**文件夹管理**
- 创建/删除/进入/返回/重命名/移动文件夹

**文件传输**
- 上传：先发 PDU（文件名+大小），服务端进入上传模式后原始字节直接写文件
- 下载：服务端回 PDU（文件名+大小），客户端进入下载模式收字节写文件
- 共享：将文件拷贝到好友目录，对方收到共享通知

### 2.3 高并发特性

- 1000-2000 并发连接稳定，连接速率约 **1700-1900/s**，请求速率约 **2000 req/s**
- 空闲 CPU **0.2%**（无空转）、连接数可归 0（无泄漏）、RSS ~7-8MB
- 修复 6 个框架级缺陷后的稳定性保障（详见第六部分）

## 第三部分 系统架构

### 3.1 总体架构：三层解耦

```
┌─────────────────────────────────────────────┐
│  业务层  CloudServer/                        │
│  用户/好友/聊天/文件管理/文件传输            │
├─────────────────────────────────────────────┤
│  协议层  protocol.h/cpp                     │
│  PDU 定义、打包、粘包/半包处理              │
├─────────────────────────────────────────────┤
│  框架层  framework/                         │
│  主从 Reactor + 线程池 + epoll              │
└─────────────────────────────────────────────┘
```

- **框架层**：通用高并发网络模型，不感知业务（连接管理、事件循环、收发）
- **协议层**：定义"线上字节格式"（PDU），服务端与 Qt 客户端**字节级一致**
- **业务层**：处理具体消息，与框架通过**回调**解耦（`onmessage` 回调触发业务分发）

### 3.2 Reactor 模型：主从 Reactor

```
                        ┌─────────────────┐
  客户端连接 ──────────► │ 主 Reactor      │  主线程/主事件循环
                        │ Acceptor (listen)│  只负责 accept 新连接
                        └────────┬────────┘
                                 │ 按 fd % 3 分配
              ┌──────────────────┼──────────────────┐
              ▼                  ▼                  ▼
        ┌──────────┐      ┌──────────┐      ┌──────────┐
        │ 从 Reactor│      │ 从 Reactor│      │ 从 Reactor│
        │ 线程 0    │      │ 线程 1    │      │ 线程 2    │
        │ 连接 fd%3=0│      │ 连接 fd%3=1│      │ 连接 fd%3=2│
        └──────────┘      └──────────┘      └──────────┘
```

- **主 Reactor（1 个）**：只负责 `accept` 新连接，把连接按 `fd % 线程数` 分配给从 Reactor
- **从 Reactor（3 个）**：各自跑一个事件循环，负责所属连接的读/写/关闭
- **好处**：accept 与业务 I/O 分离；连接分摊到多线程，天然支持多核

### 3.3 线程模型

| 线程 | 数量 | 职责 |
|---|---|---|
| 主线程 | 1 | 主事件循环：accept、定时器、把新连接投递给从循环 |
| IO 线程（线程池） | 3 | 各跑一个从事件循环：连接读写、业务回调、连接清理 |

**跨线程通信**：主线程把「连接/任务」投递给从线程，通过 `EventLoop::setinqueue()`：
1. 加锁入队 `taskqueue_`
2. `write(wakefd_)` 唤醒目标事件循环
3. 目标循环处理 wakefd → 读走计数 → 逐个执行队列任务

> **为什么用 wakefd（eventfd）**：`epoll_wait` 阻塞时，跨线程投递任务必须能打断阻塞 → 用一个可写的 eventfd，写入即触发读事件。

### 3.4 事件循环（EventLoop）核心

```
while (!stop) {
    channels = epoll_wait(10s)          // 阻塞等待事件
    if (channels 为空)  epolltimeoutcb_  // 超时回调（定时清理）
    else 逐个 channels[i]->handleevent()
}
```

每个事件循环管理三类 fd：

| fd | 作用 | 处理要求 |
|---|---|---|
| 监听 fd（仅主循环） | accept 新连接 | 循环取完到 EAGAIN |
| wakefd（eventfd） | 跨线程唤醒 | 读走计数（否则水平触发空转） |
| timerfd | 定时器 | 读走计数、重设、检查连接超时 |
| 连接 fd | 读写 | 读到 EAGAIN；可写时发数据 |

**水平触发铁律（踩坑 2 次的总结）**：水平触发下事件处理完**必须消费**——
- 可读 → 读到 EAGAIN
- 定时器/wakefd → 读走计数
- 错误事件（EPOLLERR/HUP）→ 摘除 fd（无数据可读，只能 DEL）
- 不消费 = `epoll_wait` 永远立即返回 = 100% CPU 空转

### 3.5 连接生命周期

```
accept（主循环）
  → 分配 subloop_[fd%3]，创建 Connection（存业务表 + 子循环表）
  → 投递 enablereading 到子循环（规避 shared_from_this 竞态）
  → 子循环处理读事件 → 业务回调
  → 客户端断开/错误 → closecallback
      → 从业务连接表删除（立即）
      → 连接对象仍被子循环持有，空闲超时（10s）后定时器清理销毁
```

**设计点**：
- 读事件注册延迟到连接完全建立后投递，规避 `shared_from_this` 的 `bad_weak_ptr` 竞态
- 连接销毁用"定时器兜底清理"，保证断开连接最终一定被回收（代价是 fd 多占 10s）

## 第四部分 协议设计

### 4.1 PDU 结构（与 Qt 客户端字节级一致）

```cpp
typedef struct PDU {
    uint   uiPDULen_;    // PDU 总长度（含自身与 caMsg）
    uint   uiMsgLen_;    // caMsg 长度
    uint   uiMsgType_;   // 消息类型（业务分发用）
    char   caData[64];   // 附加数据（前 32 参数1 / 后 32 参数2）
    int    caMsg[];      // 弹性数组（变长消息体）
} PDU;                   // sizeof(PDU) = 76
```

### 4.2 关键语义

- **长度头含自身**：`uiPDULen_ = sizeof(PDU) + uiMsgLen_`。客户端先读 4 字节得到总长，再读完整包，天然支持分包
- **分隔符约定**：服务端接收用原始字节（sep=0），业务层自己按 PDU 头分包——因为客户端发的长度头含自身，与框架 Buffer 默认的 sep=1（长度不含头）语义不同
- **消息类型**：注册/登录/好友/聊天/文件管理/文件传输等 17 类消息，`handlePdu` 按 `uiMsgType_` 分发

### 4.3 粘包/半包处理（服务端）

```
recvbuf_[fd] += raw;                     // 粘包：先攒着
while (buf.size() >= 4) {
    len = 前4字节;
    if (len < sizeof(PDU)) 清空丢弃;      // 脏数据
    if (buf.size() < len) break;          // 半包：等下一批
    取出完整 PDU → handlePdu;             // 处理
}
```

### 4.4 双模式协议（文件传输）

同一 socket 上协议包与原始文件字节混用，用"状态机"区分：

- **上传**：先发 PDU（文件名+大小）→ 服务端进入"上传模式"（`uploads_[fd]`）→ 之后收到的原始字节直接写文件，收满 size 回包
- **下载**：服务端先回 PDU（文件名+大小）→ 客户端进入"下载模式"（`bDownlaod_`）→ 之后收到的字节写文件，收满判成功
- **模式切换瞬间的粘包必须处理**：客户端在"收到下载响应、切换模式"后，当场消费同批剩余字节，不能等下一次 readyRead

### 4.5 与协议相关的关键决策

| 决策 | 原因 |
|---|---|
| 接收用 sep=0 原始字节 + 业务分包 | 客户端 PDU 长度头含自身 |
| 读事件注册延迟投递 | 避免 bad_weak_ptr 竞态 |
| 服务端 Buffer 长度头语义与客户端对齐 | 收发字节级一致 |
| `setProxy(NoProxy)` | 本机代理工具干扰局域网直连 |
| 客户端拼路径后清空 strEntryName | 防止路径重复拼接 |
| 下载切模式后当场消费剩余字节 | TCP 粘包竞态 |
| accept 判负 + fd<0 防御 | EMFILE 越界崩溃 |
| epoll_ctl/epoll_wait 错误容错重试 | 良性错误不能 exit |
| 错误事件分支摘除 fd | 水平触发空转 |
| backlog 4096 + accept 循环取完 | 高并发丢 SYN |
| DB 启动复位 online | 强杀后残留导致无法重登 |

## 第五部分 模块详解

### 5.1 framework/ —— 自研 Reactor 高并发框架（与业务解耦）

| 模块 | 职责 |
|---|---|
| `EventLoop.*` | 事件循环核心：epoll 封装循环、timerfd 定时器、wakefd 跨线程唤醒、任务队列 |
| `Epoll.*` | epoll 系统调用封装（add/del/mod、事件获取） |
| `Channel.*` | fd 与回调的绑定（读/写/错误/关闭回调），事件分发入口 |
| `Acceptor.*` | 监听 socket 封装：listen、accept、新连接分发 |
| `Connection.*` | 连接对象：读/写缓冲、超时管理、关闭回调 |
| `TcpServer.*` | 主从 Reactor 整合：连接表、子循环分配（`fd % 3`） |
| `Buffer.*` | 收发缓冲区（本项目配置 sep=0，业务层自行按 PDU 分包） |
| `Socket.*` | socket 封装（创建、绑定、非阻塞设置等） |
| `ThreadPool.*` | IO 线程池（3 个从事件循环线程） |
| `Timestamp.*` | 时间戳/超时计算 |
| `InetAddress.*` | IP/端口封装 |

> 框架不感知任何业务，可通过回调接入任意协议/业务，框架内自带 `EchoServer` 可运行示例。

### 5.2 CloudServer/ —— 服务端业务层（Linux，g++14）

| 模块 | 职责 |
|---|---|
| `cloudserver.*` | 业务分发器 + 全部 handler：17 类消息的 `handlePdu` 分支 |
| `protocol.*` | PDU 协议定义与编解码（与 Qt 客户端字节级一致） |
| `db.*` | MySQL 数据存储（C API，由 SQLite 迁移）：用户表/好友表 |
| `fileutil.*` | 文件系统工具（POSIX：opendir/readdir/stat/rename 等） |
| `main.cpp` | 入口：解析 IP/端口、启动 TcpServer、注册业务回调 |
| `makefile` | 编译目标：cloudserver、test_protocol、test_db、test_fileutil、stress_test |
| `stress_test.cpp` + `test.sh` | C++ 高并发压测程序 + 一键压测脚本 |
| `test_db / test_protocol / test_fileutil` | 各层单元测试 |
| `test_cloud*.py` / `stress_test.py` | 联调脚本 / Python 压测脚本 |

**业务分发机制**：框架 `onmessage` 回调收到完整 PDU → `cloudserver` 按 `uiMsgType_` 分发到对应 handler（注册/登录/好友/聊天/文件...），handler 内做数据库 + 文件操作，回包走连接发送。

### 5.3 web/ —— 前端 Qt 客户端（Windows，Qt 5.15.2）

| 模块 | 职责 |
|---|---|
| `tcpclient.*` | 主窗口：登录/注册、连接管理 |
| `opewidget.*` | 主操作面板（好友/文件标签切换） |
| `friendlw.*` | 好友列表页面 |
| `online.*` | 在线用户弹窗 |
| `privatechat.*` | 私聊窗口 |
| `book.*` | 文件管理页面（网盘目录） |
| `sharefile.*` | 共享文件选择窗口 |
| `protocol.*` | PDU 协议（与服务端一致） |
| `client.config` | 服务端 IP:端口配置 |

> 客户端完全复用旧版 Qt 云盘客户端，与服务端仅通过 PDU 协议交互；连 Linux 服务端只需改 `client.config` 的 IP/端口。

## 第六部分 编译运行与压测

### 6.1 环境要求

| 端 | 依赖 |
|---|---|
| 服务端 | Linux（Ubuntu 虚拟机）、g++14、MySQL |
| 前端 | Windows、Qt 5.15.2（MinGW 64-bit） |
| 压测 | `ulimit -n 20000`（高并发 fd 上限） |

### 6.2 编译与运行

**服务端（Linux）**

```bash
cd CloudServer
make -B                  # 编译 cloudserver + 各层测试 + stress_test
./cloudserver 0.0.0.0 8888
```

- 监听 `0.0.0.0:8888`，`listen(4096)` + SO_REUSEPORT
- 重启前先 `pkill -9 cloudserver`（SO_REUSEPORT 会让残留进程静默占端口）

**前端（Windows）**

Qt Creator 打开 `web/TcpClient.pro` 构建运行；`client.config` 第一行为服务端 IP、第二行为端口。

**单元测试**

```bash
cd CloudServer
./test_protocol    # 协议编解码
./test_db          # 数据库
./test_fileutil    # 文件工具
```

### 6.3 压测方法

每个连接执行完整链路：**建连 → 发请求 → 收响应 → 断开**。

- 请求类型：`FLUSH_DIR`（type=23，服务端必回 type=24 响应）
- 选它的原因：无需登录鉴权，任意连接可发、服务端必回包，可大批量压测且不污染用户数据库
- 一键压测（C++ 版，编译 + 运行）：

```bash
cd CloudServer
./test.sh [连接数] [每连接请求数] [host] [port]   # 一键编译并压测
./test.sh 1000 1 127.0.0.1 8888
```

### 6.4 压测结果（修复后最终数据）

| 场景 | 连接成功 | 请求成功 | 耗时 | 吞吐 |
|---|---|---|---|---|
| 100 并发 × 1 请求 | 100/100 | 100/100 | 0.07s | ~1400/s |
| 300 并发 × 1 请求 | 300/300 | 300/300 | 0.20s | ~1500/s |
| 1000 并发 × 1 请求 | 1000/1000 | 1000/1000 | ~0.5s | ~1600-1900/s |
| 2000 并发 × 1 请求 | 2000/2000 | 2000/2000 | 1.18s | ~1700/s |
| 500 连接 × 10 请求 | 500/500 | 5000/5000 | 2.48s | ~2000 req/s |

**资源占用**：压测中 CPU 有负载；空闲 CPU **0.2%**（不空转）；内存 RSS ~7-8MB 稳定无泄漏；压测后连接数归 0。

### 6.5 压测暴露并修复的 6 个框架级缺陷

| 缺陷 | 修复前 | 修复后 |
|---|---|---|
| EMFILE 崩溃 | 1000 并发时 `accept` 拿 -1 建连接，`subloop_[-1]` 越界段错误 | accept 判负 + 防御，服务端稳定 |
| 重复 remove 杀进程 | 大量断开时 `EPOLL_CTL_DEL` 报 ENOENT 直接 `exit(-1)` | 去重 remove + `setinepoll` 复位 + 容错 |
| updatechannel 杀进程 | 竞态下 MOD/ADD 报错直接 `exit(-1)` | 容错继续 |
| epoll_wait EINTR | 信号打断报 EINTR 直接 `exit(-1)` | EINTR 重试 |
| 主循环 99% 空转 | 错误事件（EPOLLERR/HUP）水平触发未消费，CPU 打满 | 错误分支摘除 fd，空闲 0.2% |
| backlog 丢连接 | 1000 并发丢 SYN，偶发连接超时 | `listen(4096)` + accept 循环取完 |

**优化前典型失败**：1000 连接，746/1000 成功，服务端崩溃（EMFILE 越界）。
**优化后**：1000-2000 并发 100% 成功，无崩溃、无空转、连接数归 0。

### 6.6 结论

1. 框架在高并发连接 + 快速断开场景下表现合格：1000-2000 并发稳定，约 1700-1900 连接/s，约 2000 req/s
2. 修复 6 个框架缺陷后：无崩溃、无 CPU 空转、无连接泄漏、空闲 CPU 0.2%
3. 核心病根两类，均已根治：
   - **良性错误被当致命错误处理**（EMFILE/ENOENT/EINTR 直接 exit）→ 改为容错/重试
   - **水平触发事件未消费**（timerfd 不读计数、错误事件不摘除）→ 已消费化
