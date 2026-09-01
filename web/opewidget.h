#ifndef OPEWIDGET_H
#define OPEWIDGET_H

#include <QWidget>
#include <QListWidget>
#include "friendlw.h"
#include "book.h"
#include <QStackedWidget>

class opeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit opeWidget(QWidget *parent = nullptr);
    static opeWidget &getInstance();
    //需要获得friend
    FriendLW & getFriend();
    Book & getBook();
signals:
private:
    QListWidget* pListW_;
    FriendLW* pFriend_;
    Book* pBook_;
    QStackedWidget* pSW_;

};

#endif // OPEWIDGET_H
