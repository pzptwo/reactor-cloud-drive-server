#ifndef BOOK_H
#define BOOK_H

#include <QWidget>
#include <QPushButton>
#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "tcpclient.h"
#include <QMessageBox>
#include <QInputDialog>
#include "protocol.h"
#include <QTimer>
#include "friendlw.h"
#include <QListWidget>

class Book : public QWidget
{
    Q_OBJECT
public:
    explicit Book(QWidget *parent = nullptr);
    void updateFileList(PDU *pdu);
    void ClearEntryName();
    QString getEntryName();
    void setDownloadStatus(bool status);
    //偷懒l
    qint64 total_;
    qint64 recived_;
    QString getDownlaodPath();
    bool getbDownlaod();




private:
    QListWidget *pBookListW_;
    QPushButton *pReturnPB_;
    QPushButton *pCreateDirPB_;
    QPushButton *pDelDirPB_;
    QPushButton *pRanamePB_;
    QPushButton *pFlushFilePB_;
    QPushButton *pUploadPB_;
    QPushButton *pDownLoadPB_;
    QPushButton *pDelFilePB_;
    QPushButton *pShareFilePB_;
    QPushButton *pMoveFilePB_;
    QPushButton *pSelectMoveDir_;

    QString strEntryName_;
    QString updatePath_;
    QTimer *pTimer_;

    QString pDownloadPath_;
    bool bDownlaod_;

    QString ShareFileName_;

    QString moveSrcFilePath_;
    QString moveFileName_;
    QString moveDesFilePath_;

signals:
public slots:
    void createDir();
    void flushDir();
    void delDir();
    void reName();
    //需要双击，进行相关的参数
    void entryDir(const QModelIndex &index);
    void returnpre();
    void updateFile();
    void updateFileDate();
    void delRegFile();
    void downloadFile();
    void shareFile();
    QString getShareFileName();
    void moveFile();
    void selectMoveDir();
};

#endif // BOOK_H
