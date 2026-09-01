#include "protocol.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

PDU *mkPDU(uint uiMsgLen)
{
    uint uiPDULen=uiMsgLen+sizeof(PDU);
    //由于是弹性数组，用new不太好，用malloc,而且Malloc申请的空间，没有类型
    PDU* pdu=(PDU *)malloc(uiPDULen);   //得到pdu了
    //防止前面申请的空间有脏数据
    memset(pdu,0,uiPDULen);
    //进行初始化
    pdu->uiMsgLen_=uiMsgLen;
    pdu->uiPDULen_=uiPDULen;
    return pdu;
}

//进行ui设计


