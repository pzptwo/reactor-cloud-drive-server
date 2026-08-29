# 云盘服务端业务联调测试（模拟 Qt 客户端协议）
# 覆盖：注册 -> 登录 -> 在线列表 -> 搜索用户
import socket, struct

def send_pdu(s, msgtype, caData):
    pdu = struct.pack('=III', 76, 0, msgtype) + caData
    s.sendall(pdu)

def recv_pdu(s):
    data = b''
    while len(data) < 4:
        chunk = s.recv(4 - len(data))
        if not chunk: return None
        data += chunk
    n = struct.unpack('=I', data)[0]
    while len(data) < n:
        chunk = s.recv(n - len(data))
        if not chunk: return None
        data += chunk
    _, m, t = struct.unpack('=III', data[:12])
    return n, m, t, data[12:n]

s = socket.create_connection(('127.0.0.1', 8888))

# 1. 注册 jack / 123
send_pdu(s, 1, b'jack' + b'\x00' * 28 + b'123' + b'\x00' * 29)
r = recv_pdu(s)
print('1 注册响应 type=%d caData=%r' % (r[2], r[3].rstrip(b'\x00')))

# 2. 登录 jack / 123
send_pdu(s, 3, b'jack' + b'\x00' * 28 + b'123' + b'\x00' * 29)
r = recv_pdu(s)
print('2 登录响应 type=%d caData=%r' % (r[2], r[3].rstrip(b'\x00')))

# 3. 在线列表（含 jack）
send_pdu(s, 5, b'\x00' * 64)
r = recv_pdu(s)
names = [r[3][i:i + 32].rstrip(b'\x00').decode() for i in range(0, len(r[3]), 32) if r[3][i:i + 32].rstrip(b'\x00')]
print('3 在线列表 type=%d 名字=%s' % (r[2], names))

# 4. 搜索 jack（应在线）
send_pdu(s, 7, b'jack' + b'\x00' * 60)
r = recv_pdu(s)
print('4 搜索响应 type=%d caData=%r' % (r[2], r[3].rstrip(b'\x00')))

# 5. 搜索不存在的用户（ghost_user 10字节 + 54个\0 = 64字节）
send_pdu(s, 7, b'ghost_user' + b'\x00' * 54)
r = recv_pdu(s)
print('5 搜索不存在 type=%d caData=%r' % (r[2], r[3].rstrip(b'\x00')))

s.close()
print('联调测试完成')
