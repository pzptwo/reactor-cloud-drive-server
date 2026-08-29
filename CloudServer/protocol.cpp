#include "protocol.h"
#include <stdlib.h>
#include <string.h>

// 创建 PDU：malloc 分配（弹性数组不能用 new），清零并初始化长度字段
PDU *mkPDU(uint uiMsgLen)
{
    uint uiPDULen=uiMsgLen+sizeof(PDU);
    // 由于是弹性数组，用 new 不太好，用 malloc，且 malloc 申请的空间没有类型
    PDU* pdu=(PDU *)malloc(uiPDULen);
    // 防止前面申请的空间有脏数据
    memset(pdu,0,uiPDULen);
    // 初始化
    pdu->uiMsgLen_=uiMsgLen;
    pdu->uiPDULen_=uiPDULen;
    return pdu;
}
