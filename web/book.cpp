#include "book.h"
#include <QListWidgetItem>
#include <QFileDialog>
#include "opewidget.h"
#include "sharefile.h"

Book::Book(QWidget *parent)
    : QWidget{parent}
{
    bDownlaod_=false;
    pTimer_=new QTimer;
    pBookListW_=new QListWidget;
    pReturnPB_=new QPushButton("返回");
    pCreateDirPB_=new QPushButton("创建文件夹");
    pDelDirPB_=new QPushButton("删除文件夹");
    pRanamePB_=new QPushButton("重命名文件");
    pFlushFilePB_=new QPushButton("刷新文件");
    QVBoxLayout *pDirVBL=new QVBoxLayout;
    pDirVBL->addWidget(pReturnPB_);
    pDirVBL->addWidget(pCreateDirPB_);
    pDirVBL->addWidget(pDelDirPB_);
    pDirVBL->addWidget(pRanamePB_);
    pDirVBL->addWidget(pFlushFilePB_);

    pUploadPB_=new QPushButton("上传文件");
    pDownLoadPB_=new QPushButton("下载文件");
    pDelFilePB_=new QPushButton("删除文件");
    pShareFilePB_=new QPushButton("共享文件");
    pMoveFilePB_=new QPushButton("移动文件");
    pSelectMoveDir_=new QPushButton("选择路径");
    pSelectMoveDir_->setEnabled(false);
    QVBoxLayout *pFileVBL=new QVBoxLayout;
    pFileVBL->addWidget(pUploadPB_);
    pFileVBL->addWidget(pDownLoadPB_);
    pFileVBL->addWidget(pDelFilePB_);
    pFileVBL->addWidget(pShareFilePB_);
    pFileVBL->addWidget(pMoveFilePB_);

    QHBoxLayout *pMain=new QHBoxLayout;
    pMain->addWidget(pBookListW_);
    pMain->addLayout(pDirVBL);
    pMain->addLayout(pFileVBL);

    setLayout(pMain);
    connect(pCreateDirPB_,&QPushButton::clicked,this,&Book::createDir);
    connect(pFlushFilePB_,&QPushButton::clicked,this,&Book::flushDir);
    connect(pDelDirPB_,&QPushButton::clicked,this,&Book::delDir);
    connect(pRanamePB_,&QPushButton::clicked,this,&Book::reName);
    //void doubleClicked(const QModelIndex &index);
    connect(pBookListW_,&QListWidget::doubleClicked,this,&Book::entryDir);
    connect(pReturnPB_,&QPushButton::clicked,this,&Book::returnpre);
    connect(pUploadPB_,&QPushButton::clicked,this,&Book::updateFile);
    connect(pTimer_,&QTimer::timeout,this,&Book::updateFileDate);
    connect(pDelFilePB_,&QPushButton::clicked,this,&Book::delRegFile);
    connect(pDownLoadPB_,&QPushButton::clicked,this,&Book::downloadFile);
    connect(pShareFilePB_,&QPushButton::clicked,this,&Book::shareFile);
    connect(pMoveFilePB_,&QPushButton::clicked,this,&Book::moveFile);
    connect(pSelectMoveDir_,&QPushButton::clicked,this,&Book::selectMoveDir);
}

void Book::updateFileList(PDU *pdu)
{
    if(pdu==nullptr)
    {
        return;
    }
    QListWidgetItem *pItemTmp=nullptr;
    int row=pBookListW_->count()-1;
    for(row;row>=0;row--)
    {
        //inline void QListWidget::removeItemWidget(QListWidgetItem *aItem)
        //QListWidgetItem *item(int row) const;
        pItemTmp=pBookListW_->item(row);
        pBookListW_->removeItemWidget(pItemTmp);
        delete pItemTmp;
    }
    FileInfo *fileInfo=nullptr;
    int iCount=pdu->uiMsgLen_/sizeof(FileInfo);
    for(int i=0;i<iCount;i++)
    {
        fileInfo=(FileInfo *)pdu->caMsg+i;
        //打印日志验证
        qDebug()<<fileInfo->caFileName<<fileInfo->iFileType;
        QListWidgetItem *pItem=new QListWidgetItem;
        if(fileInfo->iFileType==0)
        {
            pItem->setIcon(QIcon(QPixmap(":/map/dir.jpg")));
        }
        else if(fileInfo->iFileType==1)
        {
            pItem->setIcon(QIcon(QPixmap(":/map/reg.png")));
        }
        pItem->setText(fileInfo->caFileName);
        pBookListW_->addItem(pItem);
    }
}

void Book::ClearEntryName()
{
    strEntryName_.clear();
}

QString Book::getEntryName()
{
    return strEntryName_;
}

void Book::setDownloadStatus(bool status)
{
    bDownlaod_=status;
}

QString Book::getDownlaodPath()
{
    return pDownloadPath_;
}

bool Book::getbDownlaod()
{
    return bDownlaod_;
}



void Book::createDir()
{
    //需要登录名，目录信息，新建文件名字

    //class Q_WIDGETS_EXPORT QInputDialog : public QDialog
    // static QString getText(QWidget *parent, const QString &title, const QString &label,
    //                        QLineEdit::EchoMode echo = QLineEdit::Normal,
    //                        const QString &text = QString(), bool *ok = nullptr,
    //                        Qt::WindowFlags flags = Qt::WindowFlags(),
    //                        Qt::InputMethodHints inputMethodHints = Qt::ImhNone);
    QString strNewDir=QInputDialog::getText(this,"新文件夹","新文件夹名字");
    if(strNewDir.size()<32)
    {
        if(!strNewDir.isEmpty())
        {
            QString strLoginName=TcpClient::getinstance().getstrLoginName();
            QString strCurPath=TcpClient::getinstance().getCurPath();
            PDU *pdu=mkPDU(strCurPath.size()+1);
            pdu->uiMsgType_=ENUM_MSG_TYPE_CREATE_DIR_RESPEST;
            memcpy(pdu->caData,strLoginName.toStdString().c_str(),strLoginName.size());
            memcpy(pdu->caData+32,strNewDir.toStdString().c_str(),strNewDir.size());

            memcpy((char *)(pdu->caMsg),strCurPath.toStdString().c_str(),strCurPath.size());
            TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
            free(pdu);
            pdu=nullptr;
        }
        else
        {
            QMessageBox::warning(this,"新文件夹","新文件夹名字不能为空");
        }
    }
    else
    {
        QMessageBox::warning(this,"新文件夹","新文件夹名字不能超过32");
    }
}

void Book::flushDir()
{
    //获得路径传给服务器
    QString strPath=TcpClient::getinstance().getCurPath();
    PDU *pdu=mkPDU(strPath.size()+1);
    pdu->uiMsgType_=ENUM_MSG_TYPE_FLUSH_DIR_RESPEST;
    memcpy((char*)(pdu->caMsg),strPath.toStdString().c_str(),strPath.size());
    TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
    free(pdu);
    pdu=nullptr;

}

void Book::delDir()
{
    //这里要获得路径及选择的item,因为这里的是widgetList
    QString strPath=TcpClient::getinstance().getCurPath();
    //QListWidgetItem *currentItem() const;
    QListWidgetItem* pItem=pBookListW_->currentItem();
    if(pItem==nullptr)
    {
        QMessageBox::warning(this,"选择的文件","选择的文件不能为空");
        return ;
    }
    else
    {
        QString getName=pItem->text();
        PDU *pdu=mkPDU(strPath.size()+1);
        pdu->uiMsgType_=ENUM_MSG_TYPE_DEL_DIR_RESPEST;
        memcpy((char *)pdu->caMsg,strPath.toStdString().c_str(),strPath.size());
        memcpy(pdu->caData,getName.toStdString().c_str(),32); //这里默认显示32

        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=nullptr;

    }
}

void Book::reName()
{
    QString strPath=TcpClient::getinstance().getCurPath();
    //QListWidgetItem *currentItem() const;
    QListWidgetItem* pItem=pBookListW_->currentItem();
    if(pItem==nullptr)
    {
        QMessageBox::warning(this,"选择的重名的文件","选择的重名的文件不能为空");
        return ;
    }
    else
    {
        QString strOldName=pItem->text();
        QString strNewName=QInputDialog::getText(this,"重命名文件","请输入新的文件名字");

        PDU *pdu=mkPDU(strPath.size()+1);
        pdu->uiMsgType_=ENUM_MSG_TYPE_RENAME_FILE_RESPEST;
        memcpy((char *)pdu->caMsg,strPath.toStdString().c_str(),strPath.size());
        //这里要判断32
        memcpy(pdu->caData,strOldName.toStdString().c_str(),32);
        memcpy(pdu->caData+32,strNewName.toStdString().c_str(),32);

        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=nullptr;

        //flushDir();
    }
}

void Book::entryDir(const QModelIndex &index)
{
    //注意strCurPath_=QString("./%1").arg(strLoginName_);，所以要更新strCurPath
    //inline QVariant QModelIndex::data(int arole) const

        //首先双击获得文件名
        QString strName=index.data().toString();
        qDebug()<<strName;
        QString strPath=TcpClient::getinstance().getCurPath();

        PDU *pdu=mkPDU(strPath.size()+1);
        pdu->uiMsgType_=ENUM_MSG_TYPE_ENTRY_DIR_RESPEST;
        strEntryName_ = strName;
        memcpy((char *)pdu->caMsg,strPath.toStdString().c_str(),strPath.size());
        memcpy(pdu->caData,strName.toStdString().c_str(),strName.size());

        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=nullptr;
}

void Book::returnpre()
{
    QString strPath=TcpClient::getinstance().getCurPath();
    QString strName=TcpClient::getinstance().getstrLoginName();
    QString strRootPath="./"+strName;
    if(strPath==strRootPath)
    {
        QMessageBox::warning(this,"返回上一级","已经是最上层目录");
    }

    else
    {
        int index=strPath.lastIndexOf('/');
        strPath.remove(index,strPath.size()-index);
        qDebug()<<"----"<<strPath;
        //更新当前路径
        TcpClient::getinstance().modCurPath(strPath);

        //调用刷新的逻辑
        //这里防止进入的路径没有及时更改，先清空
        ClearEntryName();
        flushDir();
    }

}

void Book::updateFile()
{
    updatePath_=QFileDialog::getOpenFileName();
    qDebug()<<updatePath_;

    if(updatePath_.isEmpty())
    {
        QMessageBox::warning(this,"选择的上传文件","选择的上传文件不能为空");
    }
    else
    {
        int index=updatePath_.lastIndexOf("/");
        QString strName=updatePath_.right(updatePath_.size()-index-1);
        qDebug()<<strName;

        QFile file(updatePath_);
        qint64 fileSize=file.size();
        QString strPath=TcpClient::getinstance().getCurPath();
        PDU *pdu=mkPDU(strPath.size()+1);
        pdu->uiMsgType_=ENUM_MSG_TYPE_UPDATE_FILE_RESPEST;
        sprintf(pdu->caData,"%s %lld",strName.toStdString().c_str(),fileSize);
        memcpy((char *)pdu->caMsg,strPath.toStdString().c_str(),strPath.size());

        //获得的就要上传给服务器，还是通过pdu
        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=nullptr;

        //前面是发送pdu,后面这里是传文件二进制数据，以文件类型来,需要路径，保存把
        pTimer_->start(1000);
    }
}

void Book::updateFileDate()
{
    pTimer_->stop();
    //以只写的方式打开
    QFile file(updatePath_);
    if(file.open(QIODevice::ReadOnly))
    {
        //循环读取，缓存4096最好
        char *pBuffer=new char[4096];
        while(true)
        {
            qint64 ret=file.read(pBuffer,4096);
            if(ret>0&&ret<=4096)
            {
                TcpClient::getinstance().getTcpSocket().write(pBuffer,ret);
            }
            else if(ret==0)
            {
                break;
            }
            else
            {
               QMessageBox::warning(this,"上传文件","上传文件失败:读文件失败");
                break;
            }
        }
        file.close();
        delete []pBuffer;
        pBuffer=nullptr;
    }
    else
    {
        QMessageBox::warning(this,"上传文件","上传文件失败：打不开");
        return;
    }
}

void Book::delRegFile()
{
    //这里要获得路径及选择的item,因为这里的是widgetList
    QString strPath=TcpClient::getinstance().getCurPath();
    //QListWidgetItem *currentItem() const;
    QListWidgetItem* pItem=pBookListW_->currentItem();
    if(pItem==nullptr)
    {
        QMessageBox::warning(this,"选择的文件","选择的文件不能为空");
        return ;
    }
    else
    {
        QString getName=pItem->text();
        PDU *pdu=mkPDU(strPath.size()+1);
        pdu->uiMsgType_=ENUM_MSG_TYPE_DEL_FILE_RESPEST;
        memcpy((char *)pdu->caMsg,strPath.toStdString().c_str(),strPath.size());
        memcpy(pdu->caData,getName.toStdString().c_str(),32); //这里默认显示32

        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=nullptr;

    }
}

void Book::downloadFile()
{
    //从链表上面选取：
    //这里要获得路径及选择的item,因为这里的是widgetList

    //QListWidgetItem *currentItem() const;
    QListWidgetItem* pItem=pBookListW_->currentItem();
    if(pItem==nullptr)
    {
        QMessageBox::warning(this,"选择的文件","选择的文件不能为空");
        return ;
    }
    else
    {

        //还要记录选择的下载路径
        // static QString getSaveFileName---->::
        QString pDownloadPath=QFileDialog::getSaveFileName();
        if(pDownloadPath.isEmpty())
        {
            QMessageBox::warning(this,"下载文件","请选择下载文件的位置");
            pDownloadPath.clear();
        }
        else
        {
            pDownloadPath_=pDownloadPath;
        }

        QString strPath=TcpClient::getinstance().getCurPath();
        PDU *pdu=mkPDU(strPath.size()+1);
        QString getName=pItem->text();

        pdu->uiMsgType_=ENUM_MSG_TYPE_DOWNLOAD_FILE_RESPEST;
        memcpy((char *)pdu->caMsg,strPath.toStdString().c_str(),strPath.size());
        memcpy(pdu->caData,getName.toStdString().c_str(),32); //这里默认显示32

        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=nullptr;

    }
}

void Book::shareFile()
{
    QListWidgetItem* pItem=pBookListW_->currentItem();
    if(pItem==nullptr)
    {
        QMessageBox::warning(this,"选择的文件","选择的文件不能为空");
        return ;
    }
    else
    {
        ShareFileName_=pItem->text();

    }
    FriendLW &friendlw=opeWidget::getInstance().getFriend();
    // 先触发异步刷新好友列表
    friendlw.flushFriend();
    QListWidget *pFriendList=friendlw.getpFriendListWidget();

    shareFile::getInstance().updateFriendlw(pFriendList);
    if(shareFile::getInstance().isHidden())
    {
        shareFile::getInstance().show();
    }

}

QString Book::getShareFileName()
{
    return ShareFileName_;
}

void Book::moveFile()
{
    QListWidgetItem* pItem=pBookListW_->currentItem();
    if(pItem!=nullptr)
    {
        QString strCurpath=TcpClient::getinstance().getCurPath();
        moveFileName_=pItem->text();
        moveSrcFilePath_=strCurpath+'/'+moveFileName_;
        pSelectMoveDir_->setEnabled(true);
    }
    else
    {
        QMessageBox::warning(this,"移动文件","移动的文件不能为空");
        return ;
    }
}

void Book::selectMoveDir()
{
    QListWidgetItem* pItem=pBookListW_->currentItem();
    if(pItem!=nullptr)
    {
        QString strCurpath=TcpClient::getinstance().getCurPath();
        QString moveDesName=pItem->text();
        moveDesFilePath_=strCurpath+'/'+moveDesName;
        //这里要传给服务器所有需要原路径的长度及目标路径的长度和文件名字
        int srcLen=moveSrcFilePath_.size();
        int desLen=moveDesFilePath_.size();

        PDU *pdu=mkPDU(srcLen+desLen+2);
        pdu->uiMsgType_=ENUM_MSG_TYPE_MOVE_FILE_RESPEST;

        sprintf(pdu->caData,"%d %d %s",srcLen,desLen,moveFileName_.toStdString().c_str());
        memcpy(pdu->caMsg,moveSrcFilePath_.toStdString().c_str(),srcLen);
        memcpy((char *)(pdu->caMsg)+(srcLen+1),moveDesFilePath_.toStdString().c_str(),desLen);

        TcpClient::getinstance().getTcpSocket().write((char *)pdu,pdu->uiPDULen_);
        free(pdu);
        pdu=nullptr;
        pSelectMoveDir_->setEnabled(false);
    }
    else
    {
        QMessageBox::warning(this,"选择移动路径","移动路径不能为空");
        return ;
    }
}
