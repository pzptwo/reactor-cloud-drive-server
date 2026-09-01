#include "sharefile.h"

shareFile::shareFile(QWidget *parent)
    : QWidget{parent}
{
    pSelectAllPb_=new QPushButton("全选");
    pCancelSelectPb_=new QPushButton("取消选择");

    pOKPb_=new QPushButton("确定");
    pCancelPb_=new QPushButton("取消");
    pFriendw_=new QWidget ;
    pFriendWVBL=new QVBoxLayout(pFriendw_);

    pSA_=new QScrollArea;

    // QButtonGroup 不管布局，只管逻辑分组（方便全选/取消全选时统一操作所有复选框）。
    pButtonGroup_=new QButtonGroup(pFriendw_);
    pButtonGroup_->setExclusive(false);

    //pSA_->setWidget(pFriendw_);
    QHBoxLayout *pTopHBL=new QHBoxLayout;
    pTopHBL->addWidget(pSelectAllPb_);
    pTopHBL->addWidget(pCancelSelectPb_);
    //加一个弹簧
    pTopHBL->addStretch();


    QHBoxLayout *pDownHBL=new QHBoxLayout;
    pDownHBL->addWidget(pOKPb_);
    pDownHBL->addWidget(pCancelPb_);

    //中间是好友区域
    QVBoxLayout *pMainVBL=new QVBoxLayout;
    pMainVBL->addLayout(pTopHBL);
    pMainVBL->addWidget(pSA_);
    pMainVBL->addLayout(pDownHBL);

    setLayout(pMainVBL);

    connect(pSelectAllPb_,&QPushButton::clicked,this,&shareFile::selectAll);
    connect(pCancelSelectPb_,&QPushButton::clicked,this,&shareFile::CancelAll);
    connect(pOKPb_,&QPushButton::clicked,this,&shareFile::selectOk);
    connect(pCancelPb_,&QPushButton::clicked,this,&shareFile::CancelPb);

}

shareFile &shareFile::getInstance()
{
    static shareFile instance;
    return instance;
}

void shareFile::updateFriendlw(QListWidget *Friendlw)
{
    if(Friendlw==nullptr)
    {
        return ;
    }
    QAbstractButton* tmp=nullptr;
    QList<QAbstractButton*>preFriendList=pButtonGroup_->buttons();
    for(int i=preFriendList.size()-1;i>=0;i--)
    {
        tmp=preFriendList[i];
        pFriendWVBL->removeWidget(tmp);
        pButtonGroup_->removeButton(tmp);
        delete tmp;
        tmp=nullptr;
    }

    // explicit QCheckBox(QWidget *parent = nullptr);
    // explicit QCheckBox(const QString &text, QWidget *parent = nullptr);
    //添加上来
    QCheckBox *pCB=nullptr;
    for(int i=0;i<Friendlw->count();i++)
    {
        pCB=new QCheckBox(Friendlw->item(i)->text());
        pFriendWVBL->addWidget(pCB);
        pButtonGroup_->addButton(pCB);
    }
    pSA_->setWidget(pFriendw_);
}
void shareFile::selectAll()
{
    QList<QAbstractButton*>pBg=pButtonGroup_->buttons();
    for(int i=0;i<pBg.size();i++)
    {
        if(!pBg[i]->isChecked())
        {
            pBg[i]->setChecked(true);
        }
    }
}
void shareFile::CancelAll()
{
    QList<QAbstractButton*>pBg=pButtonGroup_->buttons();
    for(int i=0;i<pBg.size();i++)
    {
        if(pBg[i]->isChecked())
        {
            pBg[i]->setChecked(false);
        }
    }
}

void shareFile::selectOk()
{
    QString strsharetooneName=TcpClient::getinstance().getstrLoginName();
    QString strCurpath=TcpClient::getinstance().getCurPath();
    //还要得到文件的名字
    QString strShareFileName=opeWidget::getInstance().getBook().getShareFileName();
    //合成路径
    QString strPath=strCurpath+"/"+strShareFileName;
    QByteArray pathUtf8 = strPath.toUtf8();
    QList<QAbstractButton*>pBg=pButtonGroup_->buttons();
    int num=0;
    for(int i=0;i<pBg.size();i++)
    {
        if(pBg[i]->isChecked())
        {
            num++;
        }
    }
    //这里假设人占32，和strPath都放在caMSg里面

    PDU *pdu=mkPDU(32*num+pathUtf8.size()+1);
    pdu->uiMsgType_=ENUM_MSG_TYPE_SHARE_FILE_RESPEST;
    //将共享文件的名字和有多少个人传到caData里面
    sprintf(pdu->caData,"%s %d",strsharetooneName.toStdString().c_str(),num);
    //给caMsg赋值
    int j=0;
    for(int i=0;i<pBg.size();i++)
    {
        if(pBg[i]->isChecked())
        {
            QByteArray nameUtf8 = pBg[i]->text().toUtf8();
            int copyLen = qMin(nameUtf8.size(), 31);
            memcpy((char *)(pdu->caMsg)+j*32, nameUtf8.constData(), copyLen);
            ((char *)(pdu->caMsg))[j*32 + copyLen] = '\0';
            j++;
        }
    }
    memcpy((char*)(pdu->caMsg)+num*32, pathUtf8.constData(), pathUtf8.size());
    TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
    free(pdu);
    pdu=nullptr;
}

void shareFile::CancelPb()
{
    hide();
}
