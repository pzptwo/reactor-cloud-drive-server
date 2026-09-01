#ifndef ONLINE_H
#define ONLINE_H

#include <QWidget>
#include "protocol.h"

namespace Ui {
class online;
}

class online : public QWidget
{
    Q_OBJECT

public:
    explicit online(QWidget *parent = nullptr);
    ~online();

    void showOnlineUser(PDU *pdu);

private slots:
    void on_addFriend_pd_clicked();

private:
    Ui::online *ui;
};

#endif // ONLINE_H
