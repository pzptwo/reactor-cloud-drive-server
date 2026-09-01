#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QWidget>
#include <QString>
#include <QTcpSocket>
#include <QFile>

QT_BEGIN_NAMESPACE
namespace Ui {
class TcpClient;
}
QT_END_NAMESPACE

class TcpClient : public QWidget
{
    Q_OBJECT

public:
    TcpClient(QWidget *parent = nullptr);
    ~TcpClient();
    void loadconfig();  //配置文件

    //void extracted(PDU *&pdu, char *&pos);
    void recvMsg();
    //为了是后面的能使用是同一个tcpclicent里面的
    static TcpClient &getinstance();
    //获得tcpsocketor创建一个方法
    QTcpSocket &getTcpSocket();
    QString getstrLoginName();
    QString getCurPath();
    void modCurPath(QString strCurPath);

    QFile downloadFile;

public slots:
    void connectHost();

private slots:
    //void on_send_pb_clicked();



    void on_login_pb_clicked();

    void on_register_pb_clicked();

    void on_layout_pb_clicked();

private:
    Ui::TcpClient *ui;
    QString strIp_;
    quint16 port_;
    //添加TcpSocket进行连接
    QTcpSocket mytcpSocket_;    //与服务器连接，2.与服务器进行交互。
    QString strLoginName_;
    QString strCurPath_;
};
#endif // TCPCLIENT_H
