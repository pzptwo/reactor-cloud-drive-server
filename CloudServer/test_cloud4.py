# 文件传输联调测试 v2：上传 -> 刷新 -> 下载 -> 共享 -> 删除 -> 移动
# 修正：所有 caData 补足 64 字节；共享在删除 test.txt 之前执行
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

def parse_files(body):
    files = []
    msg = body[64:]
    for i in range(len(msg) // 68):
        chunk = msg[i*68:(i+1)*68]
        files.append((chunk[:64].rstrip(b'\x00').decode(), struct.unpack('=i', chunk[64:68])[0]))
    return files

def login(sock, name, pwd):
    send_pdu(sock, 1, name32(name) + name32(pwd))
    recv_pdu(sock)
    send_pdu(sock, 3, name32(name) + name32(pwd))
    return recv_pdu(sock)[3].rstrip(b'\x00')

A = socket.create_connection(('127.0.0.1', 8888))
print('A(jack) 登录:', login(A, 'jack', '123'))
B = socket.create_connection(('127.0.0.1', 8888))
print('B(lucy) 登录:', login(B, 'lucy', '456'))
path = b'./jack'

# ===== 1. 上传 test.txt =====
file_data = b'hello cloud disk'          # 16 字节
ca = b'test.txt %d' % len(file_data)
send_pdu(A, 31, ca + b'\x00' * (64 - len(ca)), path)
A.sendall(file_data)
r = recv_pdu(A); print('1 上传完成:', r[2], r[3].rstrip(b'\x00'))

# ===== 2. 刷新目录 =====
send_pdu(A, 23, b'\x00' * 64, path)
r = recv_pdu(A); print('2 刷新 ./jack:', parse_files(r[3]))

# ===== 3. 下载 test.txt =====
send_pdu(A, 35, name32('test.txt') + b'\x00' * 32, path)
r = recv_pdu(A); print('3 下载响应:', r[2], r[3].rstrip(b'\x00'))
fsize = int(r[3].rstrip(b'\x00').split()[1])
data = b''
while len(data) < fsize:
    chunk = A.recv(fsize - len(data))
    if not chunk: break
    data += chunk
print('   下载内容:', data, '| 校验:', data == file_data)

# ===== 4. 共享 test.txt 给 lucy（此时文件存在）=====
ca = b'jack 1'
send_pdu(A, 37, ca + b'\x00' * (64 - len(ca)), name32('lucy') + b'./jack/test.txt')
r = recv_pdu(A); print('4 A 共享响应:', r[2], r[3].rstrip(b'\x00'))
r = recv_pdu(B); print('   B 收到共享请求 type:', r[2], '发送者:', r[3][:32].rstrip(b'\x00').decode(), '路径:', r[3][64:64+r[1]].rstrip(b'\x00').decode())

# B 确认接收（caData 必须 64 字节）
send_pdu(B, 40, name32('lucy') + b'\x00' * 32, b'./jack/test.txt')
send_pdu(B, 23, b'\x00' * 64, b'./lucy')
r = recv_pdu(B); print('   B 刷新 ./lucy:', parse_files(r[3]))       # [('test.txt',1)]

# ===== 5. 删除 test.txt =====
send_pdu(A, 33, name32('test.txt') + b'\x00' * 32, path)
r = recv_pdu(A); print('5 删除文件:', r[3].rstrip(b'\x00'))

# ===== 6. 移动：建 dest + 上传 move.txt + 移动 =====
send_pdu(A, 21, b'\x00' * 32 + name32('dest'), path)
r = recv_pdu(A); print('6 创建 dest:', r[3].rstrip(b'\x00'))
move_data = b'abc'
ca = b'move.txt %d' % len(move_data)
send_pdu(A, 31, ca + b'\x00' * (64 - len(ca)), path)
A.sendall(move_data)
r = recv_pdu(A); print('   上传 move.txt:', r[3].rstrip(b'\x00'))

src = b'./jack/move.txt'; des = b'./jack/dest'
ca = b'%d %d move.txt' % (len(src), len(des))
send_pdu(A, 41, ca + b'\x00' * (64 - len(ca)), src + des)
r = recv_pdu(A); print('7 移动文件:', r[3].rstrip(b'\x00'))
send_pdu(A, 23, b'\x00' * 64, des)
r = recv_pdu(A); print('   刷新 dest:', parse_files(r[3]))

A.close(); B.close()
print('文件传输联调完成')
