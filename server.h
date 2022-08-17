#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QFile>

#include "mysocket.h"
#include "commands.h"
#include "actions.h"


class Server: public QTcpServer
{
    Q_OBJECT

public:
    Server(QString ip, int port);
    ~Server();

    void startServer();

private:
    QSqlDatabase _db;
    QHostAddress _hostIp;
    quint16 _hostPort;
    qint64  _nextBlockSize;

    QFile   _logFile_sessions;
    QString _docsPath;
    QString _docsTrashPath;

    QVector<int> _activeUsersId;

private slots:
    void incomingConnection(qintptr handle) override;
    void socketReady();
    void socketDisconnect();

private:
    void ProccessIncomingRequest(MySocket *socket, QJsonDocument* jDoc);
    void ProccessAdminCommand(MySocket *socket, Commands::Command command,QJsonObject *obj);
    void ProccessUserCommand(MySocket *socket, Commands::Command command,QJsonObject *obj);
    void ProccessModerCommand(MySocket *socket, Commands::Command command,QJsonObject *obj);
    void SendToClient(MySocket *socket, Commands::Command command, QJsonObject *object);

    void Identity(MySocket *socket, QJsonObject *obj);
    void Quit(MySocket *socket);
    void Error(MySocket *socket);

    void UserInfo(MySocket *socket);
    void GetDocs(MySocket *socket, QJsonObject *obj);
    void GetFavDocs(MySocket *socket, QJsonObject *obj);
    void AddFavoriteDoc(MySocket *socket, QJsonObject *obj);
    void DeleteFavoriteDoc(MySocket *socket, QJsonObject *obj);
    void SendDocInfo(MySocket *socket, QJsonObject *obj);
    void SendDocToClient(MySocket *socket, QJsonObject *obj);
    void SendDocToServer(MySocket *socket, QJsonObject *obj);

    void GetRolesAndLevels(MySocket *socket);
    void AddNewUser(MySocket *socket, QJsonObject *obj);
    void SearchUserForEdit(MySocket *socket, QJsonObject *obj);
    void SearchUserForShow(MySocket *socket, QJsonObject *obj);
    void EditUser(MySocket *socket, QJsonObject *obj);
    void EditUserInfo(MySocket *socket, QJsonObject *obj);
    void SetUserStatus(MySocket *socket, QJsonObject *obj);

    void SendDocFullInfo(MySocket *socket, QJsonObject *obj);
    void EditDocInfo(MySocket *socket, QJsonObject *obj);
    void DeleteDoc(MySocket *socket, QJsonObject *obj);
    void GetStatistics(MySocket *socket, QJsonObject *obj);

    void AddAction(int user_id, Actions::ActionsId action);

    void WriteOnSessionFile(const QString &text);
    bool CheckAccountExistence(QString &login);
    bool CheckFavoriteDocExistence(MySocket *socket, int id_doc);
    int GetRoleId(QString &name);
    int GetLevelId(QString &level);
    int GetDocNumber();
    QString GetDocName(const QString &path);
    bool AddDocToTrashtable(const QString& idDoc, const QString& path, int moder_id);
    bool GetAccStatus(int id);
};

#endif // SERVER_H
