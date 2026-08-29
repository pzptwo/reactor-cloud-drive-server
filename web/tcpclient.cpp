#include "tcpclient.h"
#include "ui_tcpclient.h"
#include "protocol.h"
#include "opewidget.h"
#include <QFile>
#include <QDebug>
#include <QString>
#include <QMessageBox>
#include <QStringList>
#include <QHostAddress>
#include "book.h"
#include "sharefile.h"

TcpClient::TcpClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TcpClient)
{
    ui->setupUi(this);
    this->loadconfig();
    resize(700,300);
    //连接服务器的函数是connectToHost(),这里封装了成功发送connected信号,所以要验证是否成功。
    //绑定信号与槽函数
    connect(&mytcpSocket_,SIGNAL(connected()),this,SLOT(connectHost()));//QT4,容易丢括号。
    connect(&mytcpSocket_,&QTcpSocket::readyRead,this,&TcpClient::recvMsg);
    //QT5connect( &对象名, &类名::信号名, 接收者对象, &类名::槽函数名 );
    //connect(&mytcpSocket_,&QTcpSocket::connected,this,&TcpClient::connectHost);
    //virtual void connectToHost(const QHostAddress &address, quint16 port, OpenMode mode = ReadWrite);
    mytcpSocket_.connectToHost(QHostAddress(strIp_),port_);
}

TcpClient::~TcpClient()
{
    delete ui;
}

void TcpClient::loadconfig()
{
    QFile config(":/client.config");
    //打开文件
    if(config.open(QIODevice::ReadOnly))
    {
        QByteArray config_data =config.readAll();   //把配置文件的内容读取。并且转换为QString
        qDebug()<<config_data;
        QString strData=config_data.toStdString().c_str();
        qDebug()<<strData;
        config.close();
        //分隔"127.0.0.1\r\n8888"
        // 兼容 Windows(\r\n) 与 Unix(\n) 两种换行
        strData.replace("\r\n", " ");
        strData.replace("\n", " ");
        // QStringList split(const QRegularExpression &sep,
        //                   Qt::SplitBehavior behavior = Qt::KeepEmptyParts) const;

        QStringList spStr=strData.split(" ");
        strIp_=spStr.at(0);
        port_=spStr.at(1).toUShort();
        qDebug()<<"ip:"<<strIp_<<"port:"<<port_;

    }
    else
    {
        QMessageBox::critical(this,"error","open error");
        //error
    }
}

//一定要把connect用上，否则信号槽没有用
void TcpClient::recvMsg()
{
    if(!opeWidget::getInstance().getBook().getbDownlaod())
    {
    //tcpsocket套接字里面有东西了
    qDebug()<<mytcpSocket_.bytesAvailable();//当前客户端已经发送过来、等待你读取的 字节数量;
    while(mytcpSocket_.bytesAvailable() >= sizeof(uint))
    {
    //分析现在的pdu格式,一定要先把uiPDULen先读出来，不先读的话，uiMsgLen得不到
    //先用peek窥探完整PDU长度，避免半包时消费了头部却读不完整
    uint uiPDUlen=0;
    mytcpSocket_.peek((char*)&uiPDUlen,sizeof(uint));
    if(mytcpSocket_.bytesAvailable() < (int)uiPDUlen)
    {
        return; // 数据还没到齐，等下一次readyRead
    }
    //数据够了，正式读取
    mytcpSocket_.read((char*)&uiPDUlen,sizeof(uint));
    uint uiMsgLen=uiPDUlen-sizeof(PDU);
    PDU *pdu=mkPDU(uiMsgLen);
    mytcpSocket_.read((char *)pdu+sizeof(uint),uiPDUlen-sizeof(uint));
    switch (pdu->uiMsgType_)
    {
        case ENUM_MSG_TYPE_REGISTER_RESPONSE:
        {
            if(0==strcmp(pdu->caData, REGISTER_OK))
            {
                QMessageBox::information(this,"注册","注册成功");
            }

            else if(0==strcmp(pdu->caData, REGISTER_FALSE))
            {
                QMessageBox::warning(this,"注册","注册失败");
            }
            break;
        }
        case ENUM_MSG_TYPE_LOGIN_RESPONSE:
        {
            if(0==strcmp(pdu->caData, LOGIN_OK))
            {
                //这里我要把路径记下来，方便后面的pdu
                strCurPath_=QString("./%1").arg(strLoginName_);
                QMessageBox::information(this,"登录","登录成功");
                opeWidget::getInstance().show();
                this->hide();
            }

            else if(0==strcmp(pdu->caData, LOGIN_FALSE))
            {
                QMessageBox::warning(this,"登录","登录失败");
            }
            break;
        }
        case ENUM_MSG_TYPE_ALL_ONLINE_RESPONSE:
        {
            //要把数据传到online页面上，online在friendlw上，
            opeWidget::getInstance().getFriend().showAllOnline(pdu);
            break;
        }
        case ENUM_MSG_TYPE_SEARCH_USER_RESPONSE:
        {
            //根据不同cadata来判断
            if(0==strcmp(pdu->caData,SEARCH_ONLINE))
            {
                QMessageBox::information(this,"搜索",SEARCH_ONLINE);
            }
            else if(0==strcmp(pdu->caData,SEARCH_OFFLINE))
            {
                QMessageBox::information(this,"搜索",SEARCH_OFFLINE);
            }
            else if(0==strcmp(pdu->caData,SEARCH_NOPERSON))
            {
                QMessageBox::information(this,"搜索",SEARCH_NOPERSON);
            }
            break;
        }
        //分为两种类型
        case ENUM_MSG_TYPE_ADD_USER_RESPONSE:
        {

            QMessageBox::information(this,"添加用户",pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_ADD_USER_RESPEST:
        {
            //注意现在在的socket,与上面的不一样，是想加好友的，被加好友的在这里回复
            char caLoginName[32]={'\0'};
            memcpy(caLoginName,pdu->caData+32,32);
            //枚举int
            int ret=QMessageBox::information(this,"添加用户",QString("%1 want to add you as friend.").arg(caLoginName),QMessageBox::Yes,QMessageBox::No);
            PDU *respdu=mkPDU(0);
            memcpy(respdu->caData,pdu->caData,32);  //拷贝
            memcpy(respdu->caData+32,pdu->caData+32,32);
            if(ret==QMessageBox::Yes)
            {
                respdu->uiMsgType_=ENUM_MSG_TYPE_ADD_USER_AGREED;
                //要发一个回复的respdu，后面进行转发
            }
            else
            {
                respdu->uiMsgType_=ENUM_MSG_TYPE_ADD_USER_REFUSE;
            }
            mytcpSocket_.write((char *)respdu,respdu->uiPDULen_);
            free(respdu);
            respdu=NULL;
            break;
        }
        case ENUM_MSG_TYPE_ADD_USER_AGREED:
        {
            //打印消息
            char caAddUser[32]={'\0'};
            memcpy(caAddUser,pdu->caData,32);
            QMessageBox::information(this,"添加好友",QString("'%1'同意添加好友").arg(caAddUser));

            //这里主动刷新好友列表，变化了
            break;
        }

        case ENUM_MSG_TYPE_ADD_USER_REFUSE:
        {
            char caAddUser[32]={'\0'};
            memcpy(caAddUser,pdu->caData,32);
            QMessageBox::information(this,"添加好友",QString("'%1'拒绝添加好友").arg(caAddUser));
            break;
        }
        case ENUM_MSG_TYPE_FLUSH_FRIEND_RESPONSE:
        {
            char caAddUser[32]={'\0'};
            memcpy(caAddUser,pdu->caData,32);
            //这里要改变的opeWd里面的friendLW
            opeWidget::getInstance().getFriend().flushFriendLW(pdu);
            // 如果共享文件窗口打开着，同步更新好友复选框
            if(!shareFile::getInstance().isHidden())
            {
                //同步更新选择共享文件接受者的页面
                QListWidget *pFriendList = opeWidget::getInstance().getFriend().getpFriendListWidget();
                shareFile::getInstance().updateFriendlw(pFriendList);
            }
            break;
        }
        case ENUM_MSG_TYPE_DEL_FRIEND_RESPONSE:
        {
            QMessageBox::information(this,"删除好友",DEL_FRIEND_OK);
            break;
        }
        case ENUM_MSG_TYPE_DEL_FRIEND_RESPEST:
        {

            //QString caLoginName=TcpClient::getinstance().getstrLoginName();
            char caSelfName[32]={'\0'};
            memcpy(caSelfName,pdu->caData,32);
            //接收另一个被删好友的pdu
            QMessageBox::information(this,"删除好友",QString("%1删除你作为好友").arg(caSelfName));
            break;
        }
        case ENUM_MSG_TYPE_PRIVATE_CHAT_RESPEST:
        {
            if(PrivateChat::getInstance().isHidden())
            {
                PrivateChat::getInstance().show();
            }
            char caSendName[32]={'\0'};
            memcpy(caSendName,pdu->caData,32);
            PrivateChat::getInstance().getChatName(caSendName);
            //把pdu传过去在该页面进行相关修改
            PrivateChat::getInstance().updateMsg(pdu);
            break;
        }
        case ENUM_MSG_TYPE_GROUP_CHAT_RESPEST:
        {
            opeWidget::getInstance().getFriend().updateGroup(pdu);
            break;
        }
        case ENUM_MSG_TYPE_CREATE_DIR_RESPONSE:
        {
            QMessageBox::information(this,"创建文件夹",pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_FLUSH_DIR_RESPONSE:
        {
            QString strEntryName=opeWidget::getInstance().getBook().getEntryName();
            if(!strEntryName.isEmpty())
            {
                strCurPath_=strCurPath_+"/"+strEntryName;
                qDebug()<<strCurPath_;
            }
            opeWidget::getInstance().getBook().updateFileList(pdu);
            break;
        }
        case ENUM_MSG_TYPE_DEL_DIR_RESPONSE:
        {
            QMessageBox::information(this,"删除文件夹",pdu->caData);
            opeWidget::getInstance().getBook().flushDir();
            break;
        }
        case ENUM_MSG_TYPE_RENAME_FILE_RESPONSE:
        {
            QMessageBox::information(this,"重命名文件",pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_ENTRY_DIR_RESPONSE:
        {
            opeWidget::getInstance().getBook().ClearEntryName();
            QMessageBox::information(this,"进入文件夹",pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_UPDATE_FILE_RESPONSE:
        {
            QMessageBox::information(this,"上传文件",pdu->caData);
            break;
        }
        case ENUM_MSG_TYPE_DEL_FILE_RESPONSE:
        {
            QMessageBox::information(this,"删除文件",pdu->caData);
            opeWidget::getInstance().getBook().flushDir();
            break;
        }
        case ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPONSE:
        {
            qDebug()<<pdu->caData;
            //前面是sprintf---->sscanf
            char DownloadName[32]={'\0'};
            sscanf(pdu->caData,"%s %lld",DownloadName,&(opeWidget::getInstance().getBook().total_));
            if(strlen(DownloadName)>0&&opeWidget::getInstance().getBook().total_>0)
            {
                opeWidget::getInstance().getBook().setDownloadStatus(true);
                downloadFile.setFileName(opeWidget::getInstance().getBook().getDownlaodPath());
                if(!downloadFile.open(QIODevice::WriteOnly))
                {
                    QMessageBox::warning(this,"下载文件","获得保存文件的路劲失败");
                }
            }

            //因为是二进制数据，这里先判断是否打开了

            break;
        }
        case ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPONSE:
        {
            QMessageBox::information(this,"共享文件","共享文件成功");
            break;
        }
        case ENUM_MSG_TYPE_SHARE_FILE_NOTE:
        {
            //服务器传过来的只有文件的路径，拷贝下来，截取方便打印日志观察是否合适
            char *pPath=new char[pdu->uiMsgLen_];
            strcpy(pPath,(char *)pdu->caMsg);
            //记得类型要匹配
            char* pos=strrchr(pPath,'/');
            if(nullptr!=pos)
            {
                pos++;
                QString strNote=QString("%1 share %2\n do you accept the file").arg(pdu->caData).arg(pos);
                int ret=QMessageBox::question(this,"共享文件",strNote);

                if(ret==QMessageBox::Yes)
                {
                    //还是要传递文件的路径
                    PDU *respdu=mkPDU(pdu->uiMsgLen_);
                    respdu->uiMsgType_=ENUM_MSG_TYPE_SHARE_FILE_NOTE_RESPONSE;
                    QString recvName=TcpClient::getinstance().getstrLoginName();
                    strcpy(respdu->caData,recvName.toStdString().c_str());
                    memcpy(respdu->caMsg,pdu->caMsg,pdu->uiMsgLen_);

                    TcpClient::getinstance().getTcpSocket().write((char*)respdu,respdu->uiPDULen_);

                    free(respdu);
                    respdu=nullptr;

                }
                delete[]pPath;
                pPath=nullptr;
            }
            break;
        }
        case ENUM_MSG_TYPE_MOVE_FILE_RESPONSE:
        {
            QMessageBox::information(this,"移动文件",pdu->caData);
            break;
        }
    default:
        break;
    }
    free(pdu);
    pdu=NULL;
    }  // while
    }
    else
    {
        QByteArray buffer=mytcpSocket_.readAll();
        downloadFile.write(buffer);
        //对方分批发，所以要判断啦
        Book &pBook=opeWidget::getInstance().getBook();
        pBook.recived_+=buffer.size();

        if(pBook.total_==pBook.recived_)
        {
            QMessageBox::information(this,"下载文件","下载文件成功");
            downloadFile.close();
            pBook.total_=0;
            pBook.recived_=0;
            //发送完就要吧下载状态设置为false
            pBook.setDownloadStatus(false);
        }
        else if(pBook.total_<pBook.recived_)
        {
            downloadFile.close();
            pBook.total_=0;
            pBook.recived_=0;
            //发送完就要吧下载状态设置为false
            pBook.setDownloadStatus(false);
            QMessageBox::critical(this,"下载文件","下载文件失败");
        }
        //这里是属于在下载。

    }
}

TcpClient &TcpClient::getinstance()
{
    static TcpClient instance;
    return instance;
}

QTcpSocket &TcpClient::getTcpSocket()
{
    return mytcpSocket_;
}

QString TcpClient::getstrLoginName()
{
    return strLoginName_;
}

QString TcpClient::getCurPath()
{
    return strCurPath_;
}

void TcpClient::modCurPath(QString strCurPath)
{
    strCurPath_=strCurPath;
}

void TcpClient::connectHost()
{
    QMessageBox::information(this,"连接服务器","连接服务器成功");
}
/*
void TcpClient::on_send_pb_clicked()
{
    //获得文本框的数据
    QString strMsg=ui->lineEdit->text();    //得到实际数据了，要发送数据，
    if(strMsg.isEmpty())
    {
        QMessageBox::warning(this,"信息发送","信息发送不能为空");
    }
    else
    {
        //要发送了,通过myTcpSocket进行通信，要得到自定义的协议
        PDU *pdu=mkPDU(strMsg.size());
        //注意发送的实际的数据caMsg_,所以通过memcpy或者strcpy进行赋值
        //void * __cdecl memcpy(void * __restrict__ _Dst,const void * __restrict__ _Src,size_t _Size) __MINGW_ATTRIB_DEPRECATED_SEC_WARN;
        memcpy(pdu->caMsg,strMsg.toStdString().c_str(),strMsg.size());
        qDebug()<<(char *)pdu->caMsg;
        //现在随便定义一个类型
        pdu->uiMsgType_=6666;
        mytcpSocket_.write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=NULL;
    }

}
*/



void TcpClient::on_login_pb_clicked()
{
    //这里进行登录的应用
    QString strName=ui->name_le->text();
    QString strPwd=ui->pwd_le->text();

    //这里先验证一遍（我想优化的方向可能是封装在数据库里面？？？？）
    if(!strName.isEmpty()&&!strPwd.isEmpty())
    {
        strLoginName_=strName;
        //这里为啥要用上pdu(数据库在服务器那边),这里没有实际发送的消息
        PDU *pdu=mkPDU(0);
        pdu->uiMsgType_=ENUM_MSG_TYPE_LOGIN_RESPEST;
        memcpy(pdu->caData,strName.toStdString().c_str(),32);
        memcpy(pdu->caData+32,strPwd.toStdString().c_str(),32);
        //发送
        mytcpSocket_.write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=NULL;
    }
    else
    {
        QMessageBox::warning(this,"登录","登录用户名和名字不能为空");
    }

}


void TcpClient::on_register_pb_clicked()
{
    QString strName=ui->name_le->text();
    QString strPwd=ui->pwd_le->text();

    //这里先验证一遍（我想优化的方向可能是封装在数据库里面？？？？）
    if(!strName.isEmpty()&&!strPwd.isEmpty())
    {
        //这里为啥要用上pdu(数据库在服务器那边),这里没有实际发送的消息
        PDU *pdu=mkPDU(0);
        pdu->uiMsgType_=ENUM_MSG_TYPE_REGISTER_RESPEST;
        memcpy(pdu->caData,strName.toStdString().c_str(),32);
        memcpy(pdu->caData+32,strPwd.toStdString().c_str(),32);
        //发送
        mytcpSocket_.write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=NULL;
    }
    else
    {
        QMessageBox::warning(this,"注册","注册时用户名和名字不能为空");
    }
}


void TcpClient::on_layout_pb_clicked()
{

}

