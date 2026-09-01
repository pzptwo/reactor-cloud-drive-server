#ifndef SHAREFILE_H
#define SHAREFILE_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QButtonGroup>
#include <QListWidget>
#include <QCheckBox>
#include "tcpclient.h"
#include "opewidget.h"
#include "protocol.h"

class shareFile : public QWidget
{
    Q_OBJECT
public:
    explicit shareFile(QWidget *parent = nullptr);

    static shareFile &getInstance();
    void updateFriendlw(QListWidget *Friendlw);
public slots:
    void selectAll();
    void CancelAll();
    void selectOk();
    void CancelPb();
private:
    QPushButton *pSelectAllPb_;
    QPushButton *pCancelSelectPb_;
    QPushButton *pOKPb_;
    QPushButton *pCancelPb_;

    QVBoxLayout *pFriendWVBL;
    QScrollArea *pSA_;
    QWidget *pFriendw_;
    QButtonGroup *pButtonGroup_;
};

#endif // SHAREFILE_H
