#ifndef FRIENDLW_H
#define FRIENDLW_H

#include <QWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QHBoxLayout>
#include "online.h"
#include "protocol.h"
#include "privatechat.h"
class FriendLW : public QWidget
{
    Q_OBJECT
public:
    explicit FriendLW(QWidget *parent = nullptr);

    void showAllOnline(PDU *pdu);
    //简单一点，正规的话属性都要定义在private，public接口
    QString strName_;
    //要根据pdu里面的数据刷新
    void flushFriendLW(PDU *pdu);

    //由于要调用pShowMsgTE_
    void updateGroup(PDU *pdu);
    QListWidget *getpFriendListWidget();
signals:
public slots:
    void showOnline();
    void serachUser();
    void flushFriend();
    void delFriend();
    void privateChat();
    void groupchat();
private:
    QTextEdit *pShowMsgTE_;
    QListWidget *pFriendListWidget_;
    QLineEdit *pInputMsgLE_;

    QPushButton* pDelFriendPB_;
    QPushButton* pFlushFriendPB_;
    QPushButton* pShowOnlineUsrPB_;
    QPushButton* pSearchUsrPB_;
    QPushButton* pMsgSendPB_;
    QPushButton* pPrivateChatPB_;
    online *pOnline_;
};

#endif // FRIENDLW_H
