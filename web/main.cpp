#include "tcpclient.h"

#include "friendlw.h"
#include "online.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // online w;
    // w.show();
    // FriendLW w;
    // w.show();

    // TcpClient w;
    // w.show();
    TcpClient::getinstance().show();
    // opeWidget ope;
    // ope.show();
    return a.exec();
}
