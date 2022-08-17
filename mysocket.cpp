#include "mysocket.h"

MySocket::MySocket(QObject *parent) : QTcpSocket{parent}
{
    user = new User();
}

MySocket::~MySocket()
{
    delete user;
}

void MySocket::SetUserInfo(int id, int level, int role, QString login)
{
    user->id    = id;
    user->level = level;
    user->role  = role;
    user->login = login;
}
