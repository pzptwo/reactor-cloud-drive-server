#include "friendlw.h"

#include "tcpclient.h"
FriendLW::FriendLW(QWidget *parent)
    : QWidget{parent}
{
    //都是继承QWidget
    pShowMsgTE_=new QTextEdit;
    pFriendListWidget_=new QListWidget;
    pInputMsgLE_=new QLineEdit;

    pDelFriendPB_=new QPushButton("删除好友");
    pFlushFriendPB_=new QPushButton("刷新好友");
    pShowOnlineUsrPB_=new QPushButton("显示在线用户");
    pSearchUsrPB_=new QPushButton("查找用户");
    pMsgSendPB_=new QPushButton("信息发送");
    pPrivateChatPB_=new QPushButton("私聊");

    QVBoxLayout *pRightPBVBL=new QVBoxLayout;
    pRightPBVBL->addWidget(pDelFriendPB_);
    pRightPBVBL->addWidget(pFlushFriendPB_);
    pRightPBVBL->addWidget(pShowOnlineUsrPB_);
    pRightPBVBL->addWidget(pSearchUsrPB_);
    pRightPBVBL->addWidget(pPrivateChatPB_);

    QHBoxLayout *pTopHBL=new QHBoxLayout;
    pTopHBL->addWidget(pShowMsgTE_);
    pTopHBL->addWidget(pFriendListWidget_);
    pTopHBL->addLayout(pRightPBVBL);

    QHBoxLayout *pMsgHBL=new QHBoxLayout;
    pMsgHBL->addWidget(pInputMsgLE_);
    pMsgHBL->addWidget(pMsgSendPB_);

    pOnline_=new online;

    QVBoxLayout *pMain=new QVBoxLayout;
    pMain->addLayout(pTopHBL);
    pMain->addLayout(pMsgHBL);
    pMain->addWidget(pOnline_);
    pOnline_->hide();

    setLayout(pMain);

    connect(pShowOnlineUsrPB_,&QPushButton::clicked,this,&FriendLW::showOnline);
    connect(pSearchUsrPB_,&QPushButton::clicked,this,&FriendLW::serachUser);
    connect(pFlushFriendPB_,&QPushButton::clicked,this,&FriendLW::flushFriend);
    connect(pDelFriendPB_,&QPushButton::clicked,this,&FriendLW::delFriend);
    connect(pPrivateChatPB_,&QPushButton::clicked,this,&FriendLW::privateChat);
    connect(pMsgSendPB_,&QPushButton::clicked,this,&FriendLW::groupchat);
}

void FriendLW::showAllOnline(PDU *pdu)
{
    if(pdu==NULL)
    {
        return ;
    }
    if(pOnline_->isHidden())   // 防止响应问题
        return;
    //这里调用online的方法
    pOnline_->showOnlineUser(pdu);
}

void FriendLW::flushFriendLW(PDU *pdu)
{
    if(pdu==NULL)
    {
        return ;
    }
    //这里是根据caNsg
    uint uiSize=pdu->uiMsgLen_/32;
    char caFriendName[32];
    pFriendListWidget_->clear();
    for(uint i=0;i<uiSize;i++)
    {
        memcpy(caFriendName,(char *)(pdu->caMsg)+i*32,32);
        pFriendListWidget_->addItem(caFriendName);
    }
}

void FriendLW::updateGroup(PDU *pdu)
{
    //类型啊
    QString strRecvMsg=QString("%1 says %2").arg(pdu->caData).arg((char *)pdu->caMsg);
    pShowMsgTE_->append(strRecvMsg);
}

QListWidget *FriendLW::getpFriendListWidget()
{
    return pFriendListWidget_;
}

void FriendLW::showOnline()
{
    if(pOnline_->isHidden())
    {
        pOnline_->show();
        //这里要显示发送数据，需要tcpsocket
        PDU *pdu=mkPDU(0);  //没有实际要发的消息,就是发送一个请求。
        pdu->uiMsgType_=ENUM_MSG_TYPE_ALL_ONLINE_RESPEST;
        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=NULL;
    }
    else
    {
        pOnline_->hide();
    }
}

void FriendLW::serachUser()
{
    // static QString getText(QWidget *parent, const QString &title, const QString &label,
    //                        QLineEdit::EchoMode echo = QLineEdit::Normal,
    //                        const QString &text = QString(), bool *ok = nullptr,
    //                        Qt::WindowFlags flags = Qt::WindowFlags(),
    //                        Qt::InputMethodHints inputMethodHints = Qt::ImhNone);
    strName_=QInputDialog::getText(this,"搜索","搜索用户");
    //打印日志验证
    qDebug()<<strName_;
    if(!strName_.isEmpty())
    {
        //就要打包pdu协议,把相关的值赋值
        PDU *pdu=mkPDU(0);  //实际长度为零是因为，可以放到结构体里面的cadata
        pdu->uiMsgType_=ENUM_MSG_TYPE_SEARCH_USER_RESPEST;
        memcpy(pdu->caData,strName_.toStdString().c_str(),strName_.size());

        //发送pdu
        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=NULL;
    }
    else
    {
        return;
    }
}

void FriendLW::flushFriend()
{
    //要从数据库里面知道，必须要把caSelfName传进去。
    QString caSelfName=TcpClient::getinstance().getstrLoginName();
    PDU *pdu=mkPDU(0);
    memcpy(pdu->caData,caSelfName.toStdString().c_str(),32);
    pdu->uiMsgType_=ENUM_MSG_TYPE_FLUSH_FRIEND_RESPEST;

    TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
    free(pdu);
    pdu=NULL;
}

void FriendLW::delFriend()
{
    if(pFriendListWidget_->currentItem()!=NULL)
    {
        //获得点击图标的名字
        QString strFrinedName=pFriendListWidget_->currentItem()->text();
        QString strSelfNmae=TcpClient::getinstance().getstrLoginName();

        PDU *pdu=mkPDU(0);
        pdu->uiMsgType_=ENUM_MSG_TYPE_DEL_FRIEND_RESPEST;
        memcpy(pdu->caData,strSelfNmae.toStdString().c_str(),strSelfNmae.size());
        memcpy(pdu->caData+32,strFrinedName.toStdString().c_str(),strFrinedName.size());

        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=NULL;
    }
}

//先在这个页面开始进行。
void FriendLW::privateChat()
{
    if(pFriendListWidget_->currentItem()!=NULL)
    {
        //获得点击图标的名字
        QString strFrinedName=pFriendListWidget_->currentItem()->text();
        PrivateChat::getInstance().getChatName(strFrinedName);
        if(PrivateChat::getInstance().isHidden())
        {
            PrivateChat::getInstance().show();
        }
    }
    else
    {
        QMessageBox::warning(this,"发送","发送者不能为空");
    }
}

void FriendLW::groupchat()
{
    if(!pInputMsgLE_->text().isEmpty())
    //这里获得相关的
    {
        QString strMsg=pInputMsgLE_->text();
        pInputMsgLE_->clear();

        //要得到所有的在线，就要一个登录的，去获得刷新
        QString strSelfNmae=TcpClient::getinstance().getstrLoginName();
        PDU *pdu=mkPDU(strMsg.size()+1);
        pdu->uiMsgType_=ENUM_MSG_TYPE_GROUP_CHAT_RESPEST;
        memcpy(pdu->caData,strSelfNmae.toStdString().c_str(),strSelfNmae.size());
        memcpy((char*)(pdu->caMsg),strMsg.toStdString().c_str(),strMsg.size());
        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=nullptr;
    }
    else
    {
        QMessageBox::warning(this,"群发","群发不能为空");
    }

}
