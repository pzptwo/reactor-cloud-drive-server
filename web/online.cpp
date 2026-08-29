#include "online.h"
#include "ui_online.h"
#include "protocol.h"
#include "tcpclient.h"

online::online(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::online)
{
    ui->setupUi(this);
}

online::~online()
{
    delete ui;
}

void online::showOnlineUser(PDU *pdu)
{
    if(pdu==NULL)
    {
        return ;
    }
    //把数据添加到页面上
    //定义一个临时的
    char tmpName[32];
    uint retsize=pdu->uiMsgLen_/32; //注意是uint
    ui->online_lw->clear();
    for(uint i=0;i<retsize;i++)
    {
        //inline void QListWidget::addItem(QListWidgetItem *aitem)
        //{ insertItem(count(), aitem); }
        memcpy(tmpName,(char *)(pdu->caMsg)+i*32,32);
        ui->online_lw->addItem(tmpName);
    }
}

void online::on_addFriend_pd_clicked()
{
    //这里在ui里面设计转到槽函数
    //获得列表的文本,QListWidgetItem *currentItem() const;??inline QString text() const,内联函数
    QListWidgetItem* data=ui->online_lw->currentItem();
    if(data == nullptr)
        return;
    QString strAddUser=data->text();
    if(strAddUser==NULL)
    {
        return ;
    }
    else
    {
        qDebug()<<strAddUser;
        //还要获得登录名字,登录名字在TcpClient里面，利用单列
        QString strLoginName=TcpClient::getinstance().getstrLoginName();
        //其实不用，登录了肯定有名字，但是规范性还是写
        if(strLoginName==NULL)
        {
            return ;
        }
        //开始写自定义协议,cadata够用
        PDU *pdu=mkPDU(0);
        pdu->uiMsgType_=ENUM_MSG_TYPE_ADD_USER_RESPEST;
        memcpy(pdu->caData,strAddUser.toStdString().c_str(),strAddUser.size());
        memcpy(pdu->caData+32,strLoginName.toStdString().c_str(),strLoginName.size());
        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=NULL;
    }
}

