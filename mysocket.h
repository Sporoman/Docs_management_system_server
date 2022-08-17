#ifndef MYSOCKET_H
#define MYSOCKET_H

#include <QTcpSocket>
#include <QObject>

struct User;


class MySocket : public QTcpSocket
{
    Q_OBJECT

public:
    User* user;

public:
    explicit MySocket(QObject *parent = nullptr);
    ~MySocket();

    void SetUserInfo(int id, int level, int role, QString login);
};

struct User
{
    int id;
    int level;
    int role;
    QString login;
};

#endif // MYSOCKET_H
