# 双客户端联调：验证 resend 转发链路（加好友流程 + 私聊）
# 客户端 A = jack（已注册），客户端 B = lucy（新注册）
import socket, struct

def send_pdu(s, msgtype, caData, caMsg=b''):
    s.sendall(struct.pack('=III', 76 + len(caMsg), len(caMsg), msgtype) + caData + caMsg)

def recv_pdu(s, timeout=3):
    s.settimeout(timeout)
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
    return n, m, t, data[12:]

def name32(s):
    return s.encode() + b'\x00' * (32 - len(s))

A = socket.create_connection(('127.0.0.1', 8888))
send_pdu(A, 1, name32('jack') + name32('123'))   # 注册 jack（已存在则注册失败，无妨）
r = recv_pdu(A)
send_pdu(A, 3, name32('jack') + name32('123'))   # 登录 jack
r = recv_pdu(A); print('A(jack) 登录:', r[3].rstrip(b'\x00'))

B = socket.create_connection(('127.0.0.1', 8888))
send_pdu(B, 1, name32('lucy') + name32('456'))
r = recv_pdu(B); print('B(lucy) 注册:', r[3].rstrip(b'\x00'))
send_pdu(B, 3, name32('lucy') + name32('456'))
r = recv_pdu(B); print('B(lucy) 登录:', r[3].rstrip(b'\x00'))

# ---- 加好友：jack 请求加 lucy（前32=lucy要加的人，后32=jack发起者）----
send_pdu(A, 9, name32('lucy') + name32('jack'))
r = recv_pdu(A); print('1 A 加好友响应:', r[3].rstrip(b'\x00'))      # 应 SEND_ADD_FRIEND
r = recv_pdu(B); print('2 B 收到加好友请求 type:', r[2])            # 应 9 (ADD_USER_RESPEST)

# ---- lucy 同意（前32=lucy，后32=jack）----
send_pdu(B, 11, name32('lucy') + name32('jack'))
r = recv_pdu(A); print('3 A 收到同意 type:', r[2])                  # 应 11 (ADD_USER_AGREED)

# ---- jack 刷新好友：应含 lucy ----
send_pdu(A, 13, name32('jack') + b'\x00' * 32)   # caData 必须 64 字节
r = recv_pdu(A)
friends = [r[3][i:i+32].rstrip(b'\x00').decode() for i in range(0, len(r[3]), 32) if r[3][i:i+32].rstrip(b'\x00')]
print('4 A 好友列表:', friends)

# ---- 私聊：jack 发消息给 lucy（前32=发送方jack，后32=接收方lucy，消息在 caMsg）----
send_pdu(A, 17, name32('jack') + name32('lucy'), '你好 lucy，测试私聊'.encode())
r = recv_pdu(B); print('5 B 收到私聊 type:', r[2], '发送方:', r[3][:32].rstrip(b'\x00').decode(), '消息:', r[3][64:64+r[1]].decode())

A.close()
B.close()
print('双客户端联调完成')
