#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
任务 10：高并发压测脚本
每个连接：建连 -> 发 FLUSH_DIR(type=23) 请求 -> 收响应(type=24) -> 断开
请求无需登录鉴权，服务端必回包，适合大批量压测，不污染数据库。

用法:
  python3 stress_test.py [连接数] [每连接请求数] [host] [port]
示例:
  python3 stress_test.py 100            # 100 并发连接，每连接 1 个请求
  python3 stress_test.py 300 20         # 300 连接，每连接连发 20 个请求
  python3 stress_test.py 1000 1 127.0.0.1 8888
"""
import socket
import struct
import threading
import time
import sys

PDU_HEADER = 76          # sizeof(PDU)
TYPE_FLUSH_DIR_REQ = 23  # ENUM_MSG_TYPE_FLUSH_DIR_RESPEST
TYPE_FLUSH_DIR_RSP = 24  # ENUM_MSG_TYPE_FLUSH_DIR_RESPONSE


def recv_exact(s, n):
    buf = b''
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            break
        buf += chunk
    return buf


def build_flush_pdu(path=b'./test1\x00'):
    msglen = len(path)
    total = PDU_HEADER + msglen
    # 布局: [uiPDULen_][uiMsgLen_][uiMsgType_][caData 64][caMsg]
    return struct.pack('<III', total, msglen, TYPE_FLUSH_DIR_REQ) + b'\x00' * 64 + path


def worker(idx, host, port, repeat, timeout, results):
    ok = 0
    fail = 0
    # 阶段1：连接（区分 connect 超时 —— 服务端 backlog/accept 问题）
    try:
        s = socket.create_connection((host, port), timeout=timeout)
    except Exception as e:
        results[idx] = ('conn-err', 1, 0, str(e)[:80])
        return
    s.settimeout(timeout)
    # 阶段2：请求/响应（区分 recv 超时 —— 连接被接受但请求没被处理）
    try:
        for _ in range(repeat):
            s.sendall(build_flush_pdu())
            hdr = recv_exact(s, 4)
            if len(hdr) != 4:
                fail += 1
                break
            (total,) = struct.unpack('<I', hdr)
            body = recv_exact(s, total - 4)
            if len(body) != total - 4:
                fail += 1
                break
            msgtype = struct.unpack('<I', body[4:8])[0]  # 响应包偏移: body[0:4]=msglen, body[4:8]=type
            if msgtype == TYPE_FLUSH_DIR_RSP:
                ok += 1
            else:
                fail += 1
        s.close()
    except Exception as e:
        results[idx] = ('recv-err', 1, ok, str(e)[:80])
        return
    results[idx] = ('ok', fail, ok, '')


def main():
    nconn = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    repeat = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    host = sys.argv[3] if len(sys.argv) > 3 else '127.0.0.1'
    port = int(sys.argv[4]) if len(sys.argv) > 4 else 8888
    timeout = float(sys.argv[5]) if len(sys.argv) > 5 else 10.0

    print(f"[开始] {nconn} 连接 x {repeat} 请求 -> {host}:{port} (超时 {timeout}s)")
    results = [None] * nconn
    threads = []
    t0 = time.time()
    for i in range(nconn):
        t = threading.Thread(target=worker, args=(i, host, port, repeat, timeout, results))
        threads.append(t)
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    dur = time.time() - t0

    conn_ok = sum(1 for r in results if r and r[0] == 'ok')
    errs = [r for r in results if r and r[0] != 'ok']
    ok_req = sum(r[2] for r in results if r)
    fail_req = nconn * repeat - ok_req

    print(f"[结果] 连接成功={conn_ok}/{nconn}")
    print(f"[结果] 请求成功={ok_req}/{nconn * repeat}  请求失败={fail_req}")
    print(f"[结果] 总耗时={dur:.2f}s  连接速率≈{nconn / dur:.0f}/s  请求速率≈{ok_req / dur:.0f}/s")
    if errs:
        print("[错误样例]")
        for r in errs[:5]:
            print("   ", r)


if __name__ == '__main__':
    main()
