# 文件管理业务联调测试
# 覆盖：创建文件夹 -> 重复创建 -> 刷新目录 -> 重命名 -> 进入目录 -> 删除目录
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

# 解析 FileInfo 数组：每个 68 字节 = caFileName[64] + iFileType(int)
# 注意：传入的 body 是 data[12:]（含 64 字节 caData），caMsg 从偏移 64 开始，必须跳过
def parse_files(body):
    files = []
    msg = body[64:]                       # 跳过 caData
    for i in range(len(msg) // 68):
        chunk = msg[i*68:(i+1)*68]
        name = chunk[:64].rstrip(b'\x00').decode()
        ftype = struct.unpack('=i', chunk[64:68])[0]   # 0 目录 / 1 文件
        files.append((name, ftype))
    return files

s = socket.create_connection(('127.0.0.1', 8888))
send_pdu(s, 1, name32('jack') + name32('123'))   # 注册（已存在则失败）
recv_pdu(s)
send_pdu(s, 3, name32('jack') + name32('123'))   # 登录
r = recv_pdu(s); print('登录:', r[3].rstrip(b'\x00'))

path = b'./jack'                                  # 用户根目录

# 1. 创建文件夹 docs（caMsg=路径，caData+32=文件夹名）
send_pdu(s, 21, b'\x00'*32 + name32('docs'), path)
r = recv_pdu(s); print('1 创建 docs:', r[3].rstrip(b'\x00'))          # file create ok

# 2. 重复创建
send_pdu(s, 21, b'\x00'*32 + name32('docs'), path)
r = recv_pdu(s); print('2 重复创建:', r[3].rstrip(b'\x00'))           # file exist

# 3. 刷新 ./jack
send_pdu(s, 23, b'\x00'*64, path)
r = recv_pdu(s); print('3 刷新 ./jack:', parse_files(r[3]))           # [('docs', 0)]

# 4. 重命名 docs -> renamed
send_pdu(s, 27, name32('docs') + name32('renamed'), path)
r = recv_pdu(s); print('4 重命名:', r[3].rstrip(b'\x00'))             # rename ok

# 5. 进入 ./jack/renamed（成功应返回 FLUSH_DIR_RESPONSE=24 + 空列表）
send_pdu(s, 29, name32('renamed') + b'\x00'*32, path)
r = recv_pdu(s); print('5 进入 renamed 返回类型:', r[2], '内容:', parse_files(r[3]))

# 6. 删除目录 ./jack/renamed
send_pdu(s, 25, name32('renamed') + b'\x00'*32, path)
r = recv_pdu(s); print('6 删除 renamed:', r[3].rstrip(b'\x00'))       # del ok

# 7. 刷新确认空
send_pdu(s, 23, b'\x00'*64, path)
r = recv_pdu(s); print('7 刷新 ./jack:', parse_files(r[3]))           # []

s.close()
print('文件管理联调完成')
