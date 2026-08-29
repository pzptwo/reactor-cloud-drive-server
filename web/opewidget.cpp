#include "opewidget.h"

opeWidget::opeWidget(QWidget *parent)
    : QWidget{parent}
{
    pListW_=new QListWidget(this);
    pListW_->addItem("好友");
    pListW_->addItem("图书");

    pFriend_=new FriendLW;
    pBook_=new Book;
    pSW_=new QStackedWidget;
    pSW_->addWidget(pFriend_);
    pSW_->addWidget(pBook_);

    QHBoxLayout *pMain=new QHBoxLayout;
    //默认一开始是第一个
    pMain->addWidget(pListW_);
    pMain->addWidget(pSW_);

    setLayout(pMain);

    connect(pListW_,&QListWidget::currentRowChanged,pSW_,&QStackedWidget::setCurrentIndex);
}

opeWidget &opeWidget::getInstance()
{
    static opeWidget instance;
    return instance;
}

//这里用指针还是引用好
FriendLW &opeWidget::getFriend()
{
    //解引用
    return *pFriend_;
}

Book &opeWidget::getBook()
{
    return *pBook_;
}
