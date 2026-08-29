// 协议层单元测试：
// 1. 验证 PDU 内存布局与 Qt 客户端完全一致（关键！字节级一致才能通信）
// 2. 验证 mkPDU 分配与字段初始化正确
#include "protocol.h"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>

int main()
{
    // ---------- 1. 内存布局断言（与 Qt 客户端必须完全一致） ----------
    // PDU: uint*3(12) + caData[64] = 76，弹性数组 caMsg 偏移 76
    assert(sizeof(PDU) == 76);
    assert(offsetof(PDU, uiPDULen_) == 0);
    assert(offsetof(PDU, uiMsgLen_) == 4);
    assert(offsetof(PDU, uiMsgType_) == 8);
    assert(offsetof(PDU, caData) == 12);
    assert(offsetof(PDU, caMsg) == 76);   // 弹性数组紧跟在 caData 后
    assert(sizeof(FileInfo) == 68);       // 64 + 4

    // ---------- 2. mkPDU 分配正确性 ----------
    PDU *pdu = mkPDU(100);
    assert(pdu != nullptr);
    assert(pdu->uiMsgLen_ == 100);
    assert(pdu->uiPDULen_ == 76 + 100);
    // 清零验证：caMsg 区域可用且已清零
    for (int i = 0; i < 100; i++)
        assert(((char*)pdu->caMsg)[i] == 0);
    // 使用 caData 与 caMsg
    memset(pdu->caData, 'A', 64);
    memcpy(pdu->caMsg, "hello", 5);
    assert(strncmp((char*)pdu->caMsg, "hello", 5) == 0);
    free(pdu);

    // ---------- 3. 模拟真实消息：注册请求（caData 前32字节用户名，后32字节密码） ----------
    PDU *reg = mkPDU(0);
    reg->uiMsgType_ = ENUM_MSG_TYPE_REGISTER_RESPEST;
    strcpy(reg->caData, "jack");
    strcpy(reg->caData + 32, "123456");
    assert(reg->uiPDULen_ == 76);
    assert(strcmp(reg->caData, "jack") == 0);
    assert(strcmp(reg->caData + 32, "123456") == 0);
    assert(reg->uiMsgType_ == ENUM_MSG_TYPE_REGISTER_RESPEST);
    free(reg);

    // ---------- 4. 模拟真实消息：登录请求 ----------
    PDU *login = mkPDU(0);
    login->uiMsgType_ = ENUM_MSG_TYPE_LOGIN_RESPEST;
    strcpy(login->caData, "lucy");
    strcpy(login->caData + 32, "pass");
    assert(strcmp(login->caData + 32, "pass") == 0);
    free(login);

    printf("协议层测试全部通过: sizeof(PDU)=%zu sizeof(FileInfo)=%zu\n",
           sizeof(PDU), sizeof(FileInfo));
    return 0;
}
