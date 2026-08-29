#ifndef PRIVATECHAT_H
#define PRIVATECHAT_H

#include "tcpclient.h"
#include "protocol.h"
#include <QMessageBox>
#include <QString>
#include <QWidget>

namespace Ui {
class PrivateChat;
}

class PrivateChat : public QWidget
{
    Q_OBJECT

public:
    explicit PrivateChat(QWidget *parent = nullptr);
    ~PrivateChat();

    //为了使用单例
    static PrivateChat &getInstance();
    void getChatName(QString strname);

    void updateMsg(PDU *pdu);

private slots:
    void on_sendMsg_Pd_clicked();

private:
    Ui::PrivateChat *ui;
    QString strChatName_;
    QString strSelfName_;
};

#endif // PRIVATECHAT_H
