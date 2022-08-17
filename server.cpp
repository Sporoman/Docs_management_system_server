#include "server.h"

#include <QSqlQuery>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDir>
#include <QFile>
#include <QDate>
#include <QSqlError>

Server::Server(QString ip, int port) : _nextBlockSize(0)
{
    // !!! Ненужное закомментить !!!
    // Настройки для домашнего сервера
    //_hostIp   = QHostAddress("192.168.0.139");
    //_hostPort = 28015;

    // Настройки для локальной сети
    _hostIp   = QHostAddress("127.0.0.1"); // QHostAddress::LocalHost;
    _hostPort = 1234;

    // Настройки при передаче параметров запуска
    if (!ip.isEmpty())
        _hostIp = QHostAddress(ip);
    if (port != 0)
        _hostPort = port;

    // Настройки для БД POSTGRE
    _db = QSqlDatabase::addDatabase("QPSQL");
    _db.setDatabaseName("diplom_bd");
    _db.setHostName("localhost");
    _db.setPort(5432);
    _db.setUserName("postgres");
    _db.setPassword("1");

    // Проверка на открытие БД
    if (_db.open())
        qDebug() << "Successful connection to Database";
    else
    {
        qDebug() << "Filed Database connection. Press anything to exit...";
        system("pause");
        exit(-1);
    }

    // Настройка переменных-членов сервера
    _docsPath = "D:/diplom/docs/";
    _docsTrashPath = "D:/diplom/docs/trash/";

    // Проверяем хранилище на существование
    if(!QDir(_docsPath).exists())
    {
        qDebug().nospace() << "Start server failed. No storage found. " ;
        system("pause");
        exit(-3);
    }

    // Проверяем папку мусорных документов на существование
    if(!QDir(_docsTrashPath).exists())
        QDir().mkdir(_docsTrashPath);

    // Создаём файл логов сессий
    if(!QDir("logs/sessions").exists())  // Создаём папку для логов сессии, если она не создана
        QDir().mkdir("logs/sessions");
    QString dateTime = QDateTime::currentDateTime().toString("yyyy.MM.dd hh-mm-ss");
    _logFile_sessions.setFileName(QString("logs/sessions/" + dateTime + " server_sessions.log"));
    _logFile_sessions.open(QIODevice::WriteOnly);
}

Server::~Server()
{
    // Не забываем закрыть файл
    _logFile_sessions.close();
}

void Server::startServer()
{
    if (this->listen(_hostIp, _hostPort))
        qDebug().nospace() << "Server started on " << _hostIp.toString() << ":" << _hostPort << '\n';
    else
    {
        qDebug().nospace() << "Start server failed. Uncorrect IP (" << _hostIp <<") "
                           << "or Port (" << _hostPort << ") is busy. Press anything to exit...";
        system("pause");
        exit(-2);
    }
}

void Server::incomingConnection(qintptr handle)
{
    // Создание нового сокета
    MySocket* socket = new MySocket(this);
    socket->setSocketDescriptor(handle);
    socket->setObjectName(QString::number(handle));

    // Не забываем связывать сигналы со слотами
    connect(socket, SIGNAL(readyRead()),    this, SLOT(socketReady()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(socketDisconnect()));

    qDebug() << "+socket" << socket->objectName() << "from" << socket->peerAddress().toString() << "connected \n";

    // Пишем информацию в файл сессии
    WriteOnSessionFile(QString("+socket " + socket->objectName() + " from " + socket->peerAddress().toString() + " connected"));
}

void Server::socketDisconnect()
{
    // Указываем вызывающий нас сокет
    MySocket* socket = (MySocket*)sender();

    // Выводим в консоль лог отключения
    qDebug() << "-socket" << socket->objectName() << "from" << socket->peerAddress().toString()
             << "as" << socket->user->login << "disconnected \n";

    // Пишем информацию в файл сессии
    WriteOnSessionFile(QString("-socket " + socket->objectName() + " from " + socket->peerAddress().toString() +
                               " as " + socket->user->login + " disconnected"));

    // Забываем id пользователя, для разблокировки входа под его аккаунтом
    if (_activeUsersId.contains(socket->user->id))
        _activeUsersId.remove(_activeUsersId.indexOf(socket->user->id));

    // Очищаем данные пользователя в сокете
    socket->SetUserInfo(-1, -1, -1, "-1");

    // Удаляем сокет
    socket->deleteLater();
}

void Server::socketReady()
{
    // Указываем вызывающий нас сокет
    MySocket* socket = (MySocket*)sender();

    // Создаём поток данных из сокета
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_2);
    if(in.status() == QDataStream::Ok)
    {
        while(true)
        {
            // Если это свежий блок - записываем его размер
            if (_nextBlockSize == 0)
            {
                if (socket->bytesAvailable() < sizeof(qint64))
                    break;

                in >> _nextBlockSize;
            }

            // Если размер блока больше, чем доступных байт, значит сообщение ещё не пришло полностью
            if (_nextBlockSize > socket->bytesAvailable())
                break;

            // Если всё дошло нормально, то обрабатываем входящий запрос,
            // превращая его в Json документ
            QJsonDocument jDoc;
            in >> jDoc;

            ProccessIncomingRequest(socket, &jDoc);

            // Если данные ещё остались - запускаем всё по новой
            if (socket->bytesAvailable() > 0)
            {
                // Обнуляем размер блока
                _nextBlockSize = 0;
                continue;
            }

            // Не забываем обнулять размер блока
            _nextBlockSize = 0;
            break;
        }
    }
    else
        qDebug() << "DataStream Error";
}

void Server::ProccessIncomingRequest(MySocket *socket, QJsonDocument* jDoc)
{
    // Распаковываем документ
    QJsonObject getObject(jDoc->object().value("args").toObject());
    Commands::Command command = static_cast<Commands::Command>(jDoc->object().value("commandNumber").toInt());

    // Выводим информацию о пакете
    // Если аргументов нет, то выводим номер команды
    if (getObject.empty())
        qDebug().nospace() << "Message from " << socket->user->login << " (socket " << socket->objectName() << "): "
                           << "Command: " << command << "-" << Commands::getNameCommand(command);
    else
    {
        // Создаём временный объект, чтобы не отображать содержимое присланного документа
        QJsonObject showObj(jDoc->object().value("args").toObject());

        // Если есть документ - отображаем только его наличие
        if (showObj.contains("document"))
        {
            showObj.remove("document");
            showObj.insert("document", true);
        }
        // Если есть фотография - отображаем только её наличие
        if (showObj.contains("photo"))
        {
            showObj.remove("photo");
            showObj.insert("photo", true);
        }

        qDebug().nospace() << "Message from " << socket->user->login << " (socket " << socket->objectName() << "): "
                           << "Command: " << command << "-" << Commands::getNameCommand(command) << '\n'
                           << "         Args: " << showObj;
    }

    // Обрабатываем пришедшую команду
    switch(socket->user->role)  // Разграничение команд по ролям
    {
        case 1: ProccessAdminCommand(socket, command, &getObject); break; // Администратор
        case 2: ProccessUserCommand (socket, command, &getObject); break; // Пользователь
        case 3: ProccessModerCommand(socket, command, &getObject); break; // Модератор
        default:
            switch(command)
            {
                case Commands::identity: Identity(socket, &getObject); break;
                case Commands::quit:     Quit(socket);                 break;
                default: case Commands::error: Error(socket);
            }
    }
}

void Server::ProccessAdminCommand(MySocket *socket, Commands::Command command,QJsonObject *obj)
{
    switch(command)
    {
        case Commands::getRolesAndLevels:  GetRolesAndLevels(socket);       break;
        case Commands::addNewUser:         AddNewUser(socket, obj);         break;
        case Commands::searchUserForEdit:  SearchUserForEdit(socket, obj);  break;
        case Commands::searchUserForShow:  SearchUserForShow(socket, obj);  break;
        case Commands::editUser:           EditUser(socket, obj);           break;
        case Commands::userInfo:           UserInfo(socket);                break;
        case Commands::editUserInfo:       EditUserInfo(socket, obj);       break;
        case Commands::setUserStatus:      SetUserStatus(socket, obj);      break;
        case Commands::quit:               Quit(socket);                    break;

        case Commands::error: default: Error(socket);
    }
}

void Server::ProccessUserCommand(MySocket *socket, Commands::Command command,QJsonObject *obj)
{
    switch(command)
    {
        case Commands::getDocs:            GetDocs(socket, obj);           break;
        case Commands::getFavDocs:         GetFavDocs(socket, obj);        break;
        case Commands::addFavoriteDoc:     AddFavoriteDoc(socket, obj);    break;
        case Commands::deleteFavoriteDoc:  DeleteFavoriteDoc(socket, obj); break;
        case Commands::sendDocInfo:        SendDocInfo(socket, obj);       break;
        case Commands::sendDocToClient:    SendDocToClient(socket, obj);   break;
        case Commands::sendDocToServer:    SendDocToServer(socket, obj);   break;
        case Commands::getRolesAndLevels:  GetRolesAndLevels(socket);      break;
        case Commands::editUserInfo:       EditUserInfo(socket, obj);      break;
        case Commands::userInfo:           UserInfo(socket);               break;
        case Commands::quit:               Quit(socket);                   break;

        case Commands::error: default: Error(socket);
    }
}

void Server::ProccessModerCommand(MySocket *socket, Commands::Command command,QJsonObject *obj)
{
    switch(command)
    {
        case Commands::getDocs:            GetDocs(socket, obj);           break;
        case Commands::sendDocFullInfo:    SendDocFullInfo(socket, obj);   break;
        case Commands::getRolesAndLevels:  GetRolesAndLevels(socket);      break;
        case Commands::editDocInfo:        EditDocInfo(socket, obj);       break;
        case Commands::deleteDoc:          DeleteDoc(socket, obj);         break;
        case Commands::getStatistics:      GetStatistics(socket, obj);     break;
        case Commands::editUserInfo:       EditUserInfo(socket, obj);      break;
        case Commands::userInfo:           UserInfo(socket);               break;
        case Commands::quit:               Quit(socket);                   break;

        case Commands::error: default: Error(socket);
    }
}

void Server::SendToClient(MySocket *socket, Commands::Command command, QJsonObject *object)
{
    // Формируем объект передачи
    QJsonObject obj;
    obj.insert("commandNumber", command);
    obj.insert("args", *object);

    QJsonDocument doc(obj);

    // Создаём поток данных
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);

    // Заполняем поток данных заглушкой qint64 под размер сообщения
    // и соответственно самим сообщением
    out << qint64(0) << doc.toJson();

    // Считаем объём сообщения и пишем его вместо заглушки
    out.device()->seek(0);
    out << qint64(data.size() - sizeof(qint64));

    qDebug().nospace() << "Send message to " << socket->user->login << " (socket "
             << socket->objectName() << ") with command: " << Commands::getNameCommand(command) << '\n';

    // Отправляем запрос клиенту
    socket->write(data);
}

void Server::Identity(MySocket *socket, QJsonObject *obj)
{   
    // Считываем логин и пароль пользователя
    QString login    = obj->value("login").toString();
    QString password = obj->value("password").toString();

    // Формируем и отправляем запрос к БД
    QString strQuery = "SELECT users.password, users.id, levels.level, users.login, users.role_id, users.active "
                       "FROM users, levels "
                       "WHERE users.login = '" + login + "' AND "
                       "users.level_id = levels.id;";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    query.next();

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Если записей с таким логином не найдено, то сразу возвращаем отказ к доступу
    if (!query.isValid())
    {
        sendObject.insert("access", false);
        sendObject.insert("error",  "Неправильный логин или пароль.");
    }
    else
    {
        // Проверяем пароль
        if (password == query.value(0).toString())
        {
            // Теперь проверяем id
            if (_activeUsersId.contains(query.value(1).toInt()))
            {
                sendObject.insert("access", false);
                sendObject.insert("error",  "Данный профиль уже используется.");
            }
            else
            {
                // Теперь проверяем активность аккаунта
                if (!query.value(5).toBool())
                {
                    sendObject.insert("access", false);
                    sendObject.insert("error",  "Данный аккаунт отключён.");
                }
                else
                {
                    // Запоминаем данные пользователя и записываем их в сокет
                    int userId  = query.value(1).toInt();
                    int level   = query.value(2).toInt();
                    QString login(query.value(3).toString());
                    int role    = query.value(4).toInt();

                    // Если роль - модератор хранилища, выдаём наивсший уровень доступа к документам
                    if (role == 3)
                        level = 99;

                    // Запоминаем данные пользователя
                    socket->SetUserInfo(userId, level, role, login);

                    // Запоминаем id пользователя, для блокировки входа под его аккаунтом, пока он в системе
                    _activeUsersId.push_back(userId);

                    // Формируем и возвращаем объект
                    sendObject.insert("access", true);
                    sendObject.insert("level",  level);
                    sendObject.insert("role",   role);

                    // Выводим в консоль лог подключения
                    qDebug() << "++socket" << socket->objectName()
                             << "logged in as" << socket->user->login;

                    // Пишем информацию в файл сессии
                    WriteOnSessionFile(QString("++socket " + socket->objectName() + " logged in as " + socket->user->login));
                }
            }
        }
        else
        {
            sendObject.insert("access", false);
            sendObject.insert("error",  "Неправильный логин или пароль.");
        }
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::identity, &sendObject);
}

void Server::Quit(MySocket *socket)
{
    // Формируем ответный пакет
    QJsonObject sendObject;

    // Выводим в консоль лог выхода из аккаунта
    qDebug() << "--socket" << socket->objectName()
             << "quit out as" << socket->user->login;

    // Пишем информацию в файл сессии
    WriteOnSessionFile(QString("--socket " + socket->objectName() + " quit out as " + socket->user->login));

    // Забываем id пользователя, для разблокировки входа под его аккаунтом
    if (_activeUsersId.contains(socket->user->id))
        _activeUsersId.remove(_activeUsersId.indexOf(socket->user->id));

    // Очищаем данные пользователя в сокете (но сохраняем логин)
    socket->SetUserInfo(-1, -1, -1, socket->user->login);

    // Формируем ответное сообщение объект
    sendObject.insert("success", true);

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::quit, &sendObject);
}

void Server::UserInfo(MySocket *socket)
{
    // Формируем и отправляем запрос к БД + название роли + уровень доступа + фото
    QString strQuery = "SELECT fio, phone, reg_date, name, level, levels.description, photo "
                       "FROM users, users_info, levels, roles "
                       "WHERE level_id = levels.id AND "
                       "users.id = users_info.user_id AND "
                       "role_id = roles.id AND "
                       "user_id = " + QString::number(socket->user->id) + ";";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    query.next();

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Если записей с таким ID не найдено, то сразу возвращаем ошибку
    if (!query.isValid())
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Пользователь не найден");
    }
    else
    {
        // Если запись найдена, то формируем отправляемый объект userInfo
        QString fio       = query.value(0).toString();
        QString phone     = query.value(1).toString();
        QString regDate   = query.value(2).toString();
        QString roleName  = query.value(3).toString();
        QString level     = query.value(4).toString();
        QString levelName = query.value(5).toString();
        QByteArray photo  = query.value(6).toByteArray();

        // Формируем и возвращаем json объект
        sendObject.insert("fio",       fio);
        sendObject.insert("phone",     phone);
        sendObject.insert("reg_date",  regDate);
        sendObject.insert("role_name", roleName);
        sendObject.insert("level",     level);
        sendObject.insert("levelName", levelName);
        sendObject.insert("photo",     QString(photo));

        sendObject.insert("success", true);
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::userInfo, &sendObject);
}

void Server::GetDocs(MySocket *socket, QJsonObject *obj)
{
    // Проверяем, прислали ли нам уровень документа
    QString levelQuery;
    if (!obj->value("level").isUndefined())
    {
        QString level = obj->value("level").toString();
        if (level != "all")
            levelQuery = "AND levels.level = '" + level + "'";
    }

    // Проверяем, прислали ли нам кусок названия документа
    QString text;
    if (!obj->value("text").isUndefined())
        text = obj->value("text").toString();

    // Сначала получаем список id избранных документов
    QString strQuery = "SELECT docs.id "
                       "FROM docs, levels, favorites_docs "
                       "WHERE docs.level_id = levels.id AND "
                       "favorites_docs.user_id = '" + QString::number(socket->user->id) + "' AND "
                       "favorites_docs.doc_id = docs.id AND "
                       "levels.level <= '" + QString::number(socket->user->level) + "' AND "
                       "docs.name ILIKE '%" + text + "%' " + levelQuery + ";";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();

    // Запоминаем все результаты, для будущего сравнения
    QVector<int> vecId;
    while(query.next())
        vecId.push_back(query.value(0).toInt());

    // Теперь формируем и отправляем запрос к БД на все необходимые документы
    strQuery = "SELECT docs.id, docs.name, levels.level "
                       "FROM docs, levels "
                       "WHERE docs.level_id = levels.id AND "
                       "levels.level <= '" + QString::number(socket->user->level) + "' AND "
                       "docs.name ILIKE '%" + text + "%' " + levelQuery + " "
                       "LIMIT 50;";
    query.prepare(strQuery);
    query.exec();

    // Создаём массив данных для отправки
    QJsonArray docsArr; // Массив документов
    while(query.next())
    {
        int idDoc     = query.value(0).toInt();
        QString name  = query.value(1).toString();
        int level     = query.value(2).toInt();
        bool favorite = false;

        // Если вектор содержит id документа - он избранный
        if (vecId.contains(idDoc))
            favorite = true;

        // Документ
        QJsonObject document;
        document.insert("id_doc",   idDoc);
        document.insert("name"  ,   name);
        document.insert("level" ,   level);
        document.insert("favorite", favorite);

        // Добавляем документ в массив документов
        docsArr.push_back(document);
    }

    // Формируем ответный пакет
    QJsonObject sendObject;
    sendObject.insert("docs", docsArr);

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::getDocs, &sendObject);
}

void Server::GetFavDocs(MySocket *socket, QJsonObject *obj)
{
    // Проверяем, прислали ли нам уровень документа
    QString levelQuery;
    if (!obj->value("level").isUndefined())
    {
        QString level = obj->value("level").toString();
        if (level != "all")
            levelQuery = "AND levels.level = '" + level + "'";
    }

    // Проверяем, прислали ли нам кусок названия документа
    QString text;
    if (!obj->value("text").isUndefined())
        text = obj->value("text").toString();

    // Формируем и отправляем запрос к БД
    QString strQuery = "SELECT docs.id, docs.name, levels.level "
                       "FROM docs, levels, favorites_docs "
                       "WHERE docs.level_id = levels.id AND "
                       "favorites_docs.user_id = '" + QString::number(socket->user->id) + "' AND "
                       "favorites_docs.doc_id = docs.id AND "
                       "levels.level <= '" + QString::number(socket->user->level) + "' AND "
                       "docs.name ILIKE '%" + text + "%' " + levelQuery + " "
                       "LIMIT 50;";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();

    // Создаём массив данных для отправки
    QJsonArray docsArr; // Массив документов
    while(query.next())
    {
        int idDoc    = query.value(0).toInt();
        QString name = query.value(1).toString();
        int level    = query.value(2).toInt();

        // Документ
        QJsonObject document;
        document.insert("id_doc",   idDoc);
        document.insert("name"  ,   name);
        document.insert("level" ,   level);
        document.insert("favorite", true);

        // Добавляем документ в массив документов
        docsArr.push_back(document);
    }

    // Формируем ответный пакет
    QJsonObject sendObject;
    sendObject.insert("docs", docsArr);

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::getFavDocs, &sendObject);
}

void Server::AddFavoriteDoc(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    int id_doc(obj->value("id_doc").toInt());

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Проверяем запись на существование
    if(CheckFavoriteDocExistence(socket, id_doc))
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Документ уже находится в \"Избранном\".");
    }
    else
    {
        // Формируем и отправляем запрос к БД
        QString strQuery = "INSERT INTO favorites_docs(user_id, doc_id) "
                           "VALUES ('" + QString::number(socket->user->id) + "', '" + QString::number(id_doc) + "');";

        QSqlQuery query;
        query.prepare(strQuery);
        if (query.exec())
        {
            // Добавляем статистическую информацию
            AddAction(socket->user->id, Actions::ActionsId::add_fav_doc);
            sendObject.insert("success", true);
        }
        else
        {
            sendObject.insert("success", false);
            sendObject.insert("error", "Сервер отклонил запрос. Попробуйте позже.");
        }
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::addFavoriteDoc, &sendObject);
}

void Server::DeleteFavoriteDoc(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    int id_doc(obj->value("id_doc").toInt());

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Проверяем запись на существование
    if(!CheckFavoriteDocExistence(socket, id_doc))
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Документа уже нет в \"Избранном\".");
    }
    else
    {
        // Формируем и отправляем запрос к БД
        QString strQuery =  "DELETE FROM favorites_docs "
                            "WHERE user_id = '" + QString::number(socket->user->id) + "' "
                            "AND doc_id = '" + QString::number(id_doc) + "';";

        QSqlQuery query;
        query.prepare(strQuery);
        if (query.exec())
        {
            // Добавляем статистическую информацию
            AddAction(socket->user->id, Actions::ActionsId::remove_fav_doc);
            sendObject.insert("success", true);
        }
        else
        {
            sendObject.insert("success", false);
            sendObject.insert("error", "Сервер отклонил запрос. Попробуйте позже.");
        }
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::deleteFavoriteDoc, &sendObject);
}

void Server::SendDocInfo(MySocket *socket, QJsonObject *obj)
{
    QString id_doc = QString::number(obj->value("id_doc").toInt());

    // Формируем и отправляем запрос к БД
    QString strQuery = "SELECT docs.name  "
                       "FROM docs, levels "
                       "WHERE docs.level_id = levels.id "
                       "AND levels.level <= '" + QString::number(socket->user->level) + "' "
                       "AND docs.id = " + id_doc + ";";

    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    query.next();

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Если записей с таким ID не найдено, то сразу возвращаем ошибку
    if (!query.isValid())
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Документ временно недоступен.");
    }
    else
    {
            sendObject.insert("name", query.value(0).toString());
            sendObject.insert("success", true);
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::sendDocInfo, &sendObject);
}

void Server::SendDocToClient(MySocket *socket, QJsonObject *obj)
{
    QString id_doc = QString::number(obj->value("id_doc").toInt());

    // Формируем и отправляем запрос к БД
    QString strQuery = "SELECT docs.path "
                       "FROM docs, levels "
                       "WHERE docs.level_id = levels.id "
                       "AND levels.level <= '" + QString::number(socket->user->level) + "' "
                       "AND docs.id = " + id_doc + ";";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    query.next();

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Если записей с таким ID не найдено, то сразу возвращаем ошибку
    if (!query.isValid())
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Документ временно недоступен.");
    }
    else
    {
        // Если запись найдена, то формируем отправляемый пакет с файлом
        QFile file(query.value(0).toString());

        if (file.open(QIODevice::ReadOnly))
        {
            QByteArray byteArray = file.readAll();

            // Вот тут внимательно. Переводим байты в base64 и засовываем их стринг,
            // чтобы поместить всё это в json-объект
            sendObject.insert("document", QString(byteArray.toBase64()));
            sendObject.insert("success", true);

            file.close();

            // Добавляем статистическую информацию
            AddAction(socket->user->id, Actions::ActionsId::download_doc);
        }
        else
        {
            sendObject.insert("success", false);
            sendObject.insert("error", "Сервер не смог передать документ.");
        }
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::sendDocToClient, &sendObject);
}

void Server::SendDocToServer(MySocket *socket, QJsonObject *obj)
{
    // Имя документа в хранилище - это его будущий номер в БД
    int docNumber = GetDocNumber();

    // Объект - это json с документом внутри в виде QString(QByteArray.toBase64())
    QString documentFromString(obj->value("document").toString());
    QByteArray documentFromByte(documentFromString.toUtf8());
    documentFromByte = documentFromByte.fromBase64(documentFromByte);

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Для начала проверяем файл на существование
    QString strQuery = "SELECT id "
                       "FROM docs "
                       "WHERE name = '" + obj->value("name").toString() + "';";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    if (!query.next())
    {
        // Сохраняем файл в хранилище
        QString path(QString(_docsPath + QString::number(docNumber)));
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(documentFromByte);
            file.close();

            // Вытаскиваем переданные значения
            QString name(obj->value("name").toString());
            QString level(QString::number(obj->value("level").toInt()));

            // Добавляем запись о файле в базу данных
            // Формируем и отправляем запрос к БД
            strQuery = "INSERT INTO docs(name, level_id, path, user_id, load_date) "
                       "VALUES ('" + name + "', '" + level + "', '" + path + "',  "
                               "'" + QString::number(socket->user->id) + "', NOW());";
            query.prepare(strQuery);
            if (query.exec())
            {
                // Добавляем статистическую информацию
                AddAction(socket->user->id, Actions::ActionsId::upload_doc);
                sendObject.insert("success", true);
            }
            else
            {
                sendObject.insert("success", false);
                sendObject.insert("error", "Сервер отклонил запрос. Попробуйте позже.");
            }
        }
        else
        {
            sendObject.insert("success", false);
            sendObject.insert("error", "Хранилище временно недоступно.");
        }
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Документ с таким именем уже существует.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::sendDocToServer, &sendObject);
}

void Server::GetRolesAndLevels(MySocket *socket)
{
    // Формируем и отправляем запрос к БД на получение Уровней
    QSqlQuery query;
    query.prepare("SELECT level FROM levels ORDER BY level;");
    query.exec();

    // Создаём массив данных для отправки
    QJsonArray levelsArr; // Массив уровней
    while(query.next())
    {
        int level = query.value(0).toInt();
        levelsArr.push_back(level); // Добавляем объект уровня в массив уровней
    }

    // Формируем и отправляем запрос к БД на получение Ролей
    query.prepare("SELECT name FROM roles;");
    query.exec();

    // Создаём массив данных для отправки
    QJsonArray rolesArr; // Массив ролей
    while(query.next())
    {
        QString role = query.value(0).toString();
        rolesArr.push_back(role);    // Добавляем объект роли в массив ролей
    }

    // Формируем ответный пакет
    QJsonObject sendObject;
    sendObject.insert("levels", levelsArr);
    sendObject.insert("roles", rolesArr);

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::getRolesAndLevels, &sendObject);
}

void Server::AddNewUser(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    QString login(obj->value("login").toString());
    QString password(obj->value("password").toString());
    QString role(obj->value("role").toString());
    QString level(obj->value("level").toString());

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Проверяем аккаунт на существование с помощью логина
    if(CheckAccountExistence(login))
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Пользователь с данным логином уже существует.");
    }
    else
    {
        // Выясняем id роли
        QString roleId = QString::number(GetRoleId(role));

        // Выясняем id уровня
        QString levelId = QString::number(GetLevelId(level));

        // Формируем и отправляем запрос к БД
        QString strQuery = "INSERT INTO users(login, password, role_id, level_id) "
                   "VALUES ('" + login + "', '" + password + "', '" + roleId + "', '" + levelId + "');";

        QSqlQuery query;
        query.prepare(strQuery);
        if (query.exec())
        {
            // Если команда выполненна успешно, то необходимо не только передать сообщение
            // об успехе, но и создать новую запись в таблице user_info
            sendObject.insert("success", true);

            // Формируем и отправляем запрос к БД на поиск id нового пользователя
            strQuery = "SELECT id FROM users WHERE login = '" + login + "';";
            query.prepare(strQuery);
            query.exec();
            query.next();

            QString userId = query.value(0).toString();

            // Формируем и отправляем запрос к БД на добавление записи в user_info
            strQuery = "INSERT INTO users_info(user_id, fio, phone, reg_date) "
                        "VALUES ('" + userId + "', '-', '-', NOW());";
            query.prepare(strQuery);
            query.exec();

            // Добавляем статистическую информацию
            AddAction(socket->user->id, Actions::ActionsId::create_account);
        }
        else
        {
            sendObject.insert("success", false);
            sendObject.insert("error", "Сервер отклонил запрос. Попробуйте позже.");
        }
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::addNewUser, &sendObject);
}

void Server::SearchUserForEdit(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    QString obj_id(obj->value("id").toString());
    QString obj_login(obj->value("login").toString());

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Создаём запрос на поиск аккаунта по id или логину
    QString strQuery = "SELECT users.id, login, password, roles.name , levels.level, active "
                       "FROM users, roles, levels "
                       "WHERE role_id = roles.id AND level_id = levels.id ";

    if (!obj_id.isEmpty())
        strQuery += "AND users.id = '" + obj_id + "';";
    else if (!obj_login.isEmpty())
        strQuery += "AND login = '" + obj_login + "';";
    else
    {
        // Если все поля пустые - возвращаем ошибку
        sendObject.insert("success", false);
        sendObject.insert("error", "Пришли пустые поля для поиска.");
        SendToClient(socket, Commands::Command::searchUserForEdit, &sendObject);
        return;
    }

    // Формируем и отправляем запрос к БД
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    if(query.next())
    {
        int id           = query.value(0).toInt();
        QString login    = query.value(1).toString();
        QString password = query.value(2).toString();
        QString role     = query.value(3).toString();
        int level        = query.value(4).toInt();
        bool status      = query.value(5).toBool();

        sendObject.insert("success", true);

        sendObject.insert("id",       id);
        sendObject.insert("login",    login);
        sendObject.insert("password", password);
        sendObject.insert("role",     role);
        sendObject.insert("level",    level);
        sendObject.insert("status",   status);
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Пользователь не найден.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::searchUserForEdit, &sendObject);
}

void Server::SearchUserForShow(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    QString obj_id(obj->value("id").toString());
    QString obj_login(obj->value("login").toString());

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Создаём запрос на поиск аккаунта по id или логину
    QString strQuery = "SELECT users.id, fio, phone, reg_date, name, levels.level, levels.description, photo, active "
                       "FROM users, users_info, levels, roles "
                       "WHERE level_id = levels.id AND role_id = roles.id "
                       "AND users.id = users_info.user_id AND ";

    if (!obj_id.isEmpty())
        strQuery += "users.id = '" + obj_id + "';";
    else if (!obj_login.isEmpty())
        strQuery += "login = '" + obj_login + "';";
    else
    {
        // Если все поля пустые - возвращаем ошибку
        sendObject.insert("success", false);
        sendObject.insert("error", "Пришли пустые поля для поиска.");

        SendToClient(socket, Commands::Command::searchUserForShow, &sendObject);
        return;
    }

    // Формируем и отправляем запрос к БД
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    if(query.next())
    {
        // Если запись найдена, то формируем отправляемый объект userInfo
        int id             = query.value(0).toInt();
        QString fio        = query.value(1).toString();
        QString phone      = query.value(2).toString();
        QString regDate    = query.value(3).toString();
        QString roleName   = query.value(4).toString();
        int level          = query.value(5).toInt();
        QString level_name = query.value(6).toString();
        QByteArray photo   = query.value(7).toByteArray();
        bool status        = query.value(8).toBool();

        // Формируем и возвращаем json объект
        sendObject.insert("id",         id);
        sendObject.insert("fio",        fio);
        sendObject.insert("phone",      phone);
        sendObject.insert("reg_date",   regDate);
        sendObject.insert("role_name",  roleName);
        sendObject.insert("level",      level);
        sendObject.insert("level_name", level_name);
        sendObject.insert("photo",      QString(photo));
        sendObject.insert("status",     status);

        sendObject.insert("success", true);
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Пользователь не найден.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::searchUserForShow, &sendObject);
}

void Server::EditUser(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    int id(obj->value("id").toInt());
    QString login(obj->value("login").toString());
    QString password(obj->value("password").toString());
    QString role(obj->value("role").toString());
    QString level(obj->value("level").toString());

    // Выясняем id роли
    QString roleId = QString::number(GetRoleId(role));

    // Выясняем id уровня
    QString levelId = QString::number(GetLevelId(level));

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Формируем и отправляем запрос к БД
    QString strQuery = "UPDATE users "
                       "SET login = '" + login + "', password = '" + password + "', "
                           "role_id = '" + roleId + "', level_id = '" + levelId + "' "
                       "WHERE id = '" + QString::number(id) + "';";

    QSqlQuery query;
    query.prepare(strQuery);
    if (query.exec())
    {
        // Добавляем статистическую информацию
        AddAction(socket->user->id, Actions::ActionsId::update_info_account);

        sendObject.insert("success", true);
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Сервер отклонил запрос. Попробуйте позже.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::editUser, &sendObject);
}

void Server::EditUserInfo(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    QString fio(obj->value("fio").toString());
    QString phone(obj->value("phone").toString());
    QString photo;

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Формируем и отправляем запрос к БД
    QString strQuery;

    if (obj->value("photo_change").toBool())    // Если фотография изменяется
    {
        QString imageFromString(obj->value("photo").toString());
        photo = "'" + imageFromString + "'";

        strQuery = "UPDATE users_info "
                   "SET fio = '" + fio + "', phone = '" + phone + "', photo = " + photo + " "
                   "WHERE user_id = '" + QString::number(socket->user->id) + "';";
    }
    else    // Если нет
    {
        strQuery = "UPDATE users_info "
                   "SET fio = '" + fio + "', phone = '" + phone + "' "
                   "WHERE user_id = '" + QString::number(socket->user->id) + "';";
    }

    QSqlQuery query;
    query.prepare(strQuery);
    if (query.exec())
    {
        // Добавляем статистическую информацию
        AddAction(socket->user->id, Actions::ActionsId::update_info_profile);

        sendObject.insert("success", true);
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Сервер отклонил запрос. Попробуйте позже.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::editUserInfo, &sendObject);
}

void Server::SetUserStatus(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    int id(obj->value("id").toInt());

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Формируем и отправляем запрос к БД
    QString strQuery = "UPDATE users "
                       "SET active = NOT active "
                       "WHERE id = '" + QString::number(id) + "';";

    QSqlQuery query;
    query.prepare(strQuery);
    if (query.exec())
    {
        // Узнаем текущий статус аккаунта
        bool status = GetAccStatus(id);

        // Добавляем статистическую информацию
        if(status)
            AddAction(socket->user->id, Actions::ActionsId::set_active_true);
        else
            AddAction(socket->user->id, Actions::ActionsId::set_active_false);

        sendObject.insert("success", true);
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Сервер отклонил запрос. Попробуйте позже.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::setUserStatus, &sendObject);
}

void Server::SendDocFullInfo(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    QString idDoc(obj->value("id_doc").toString());

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Формируем и отправляем запрос к БД
    QString strQuery = "SELECT docs.id, docs.name, levels.level, load_date "
                       "FROM docs, levels "
                       "WHERE docs.level_id = levels.id "
                       "AND levels.level <= '" + QString::number(socket->user->level) + "' "
                       "AND docs.id = " + idDoc + ";";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    if(query.next())
    {
        // Если запись найдена, то отправляем найденный документ
        int id           = query.value(0).toInt();
        QString name     = query.value(1).toString();
        int level        = query.value(2).toInt();
        QString loadDate = query.value(3).toString();

        // Формируем и возвращаем json объект
        sendObject.insert("id",        id);
        sendObject.insert("name",      name);
        sendObject.insert("level",     level);
        sendObject.insert("load_date", loadDate);

        sendObject.insert("success", true);
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Документ не найден.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::sendDocFullInfo, &sendObject);
}

void Server::EditDocInfo(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    QString id(obj->value("id_doc").toString());
    QString name(obj->value("name").toString());
    QString level(obj->value("level").toString());

    // Выясняем id уровня
    QString levelId = QString::number(GetLevelId(level));

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Формируем и отправляем запрос к БД
    QString strQuery = "UPDATE docs "
                       "SET name = '" + name + "', level_id = '" + levelId + "' "
                       "WHERE id = '" + id + "';";

    QSqlQuery query;
    query.prepare(strQuery);
    if (query.exec())
    {
        // Добавляем статистическую информацию
        AddAction(socket->user->id, Actions::ActionsId::update_info_doc);

        sendObject.insert("success", true);
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Сервер отклонил запрос. Попробуйте позже.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::editDocInfo, &sendObject);
}

void Server::DeleteDoc(MySocket *socket, QJsonObject *obj)
{
    // Необходимо удалить всё, что связанно с документом:
    // файл в хранилище, записи в "избранных" документах и сам документ в БД.
    // Начнём с файла (не удаляем его, а просто переносим в папку 'trash')

    // Распаковываем пакет
    QString id(obj->value("id_doc").toString());

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Отправляем запрос к БД
    QString strQuery = "SELECT path FROM docs WHERE id = '" + id + "';";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    QString path;
    if (query.next())  // Если документ найден
    {
        // Переносим файл в папку trash
        path = query.value(0).toString();
        QString docName = GetDocName(path);

        if(QFile::rename(path, _docsTrashPath + docName))
        {
            // Если удалось перенести файл - значит и система и хранилище работают в нормальном режиме.
            // Идём дальше - удаляем записи с "избранных"
            strQuery = "DELETE FROM favorites_docs WHERE doc_id = '" + id + "';";
            query.prepare(strQuery);
            if (query.exec())
            {
                // Осталось удалить сам документ (переносим данные о нём в специальную trash таблицу документов)
                if (!AddDocToTrashtable(id, _docsTrashPath + docName, socket->user->id))
                    qDebug() << "Документ с id" << id << "не удалось занести в trash таблицу";

                strQuery = "DELETE FROM docs WHERE id = '" + id + "';";
                query.prepare(strQuery);
                if (query.exec())
                {
                    // Добавляем статистическую информацию
                    AddAction(socket->user->id, Actions::ActionsId::delete_doc);
                    sendObject.insert("success", true);
                }
                else
                {
                    sendObject.insert("success", false);
                    sendObject.insert("error", "Документ очищен, но его запись в БД осталась.");
                }
            }
            else
            {
                sendObject.insert("success", false);
                sendObject.insert("error", "Документ удалился, но записи в БД о нём остались.");
            }
        }
        else
        {
            sendObject.insert("success", false);
            sendObject.insert("error", "Хранилище недоступно, попробуйте позже.");
        }
    }
    else
    {
        sendObject.insert("success", false);
        sendObject.insert("error", "Документ не найден.");
    }

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::deleteDoc, &sendObject);
}

void Server::GetStatistics(MySocket *socket, QJsonObject *obj)
{
    // Распаковываем пакет
    QString countOfInterval(obj->value("count_of_interval").toString());
    QString interval(obj->value("interval").toString());
    QString fullInterval = countOfInterval + ' ' + interval;      // Формируем полный интервал

    // Формируем ответный пакет
    QJsonObject sendObject;

    // Формируем и отправляем запрос к БД
    QString strQuery = "SELECT id, name FROM actions;";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    while(query.next())
    {
        QString id   = QString::number(query.value(0).toLongLong());
        QString name = query.value(1).toString();

        // Запрашиваем количество записей действия
        QString strQueryCount;
        if (countOfInterval == "0" || interval == "all")
            strQueryCount = "SELECT count(action_id) FROM statistics "
                            "WHERE action_id = '" + id + "';";
        else
            strQueryCount = "SELECT count(action_id) FROM statistics "
                            "WHERE action_id = '" + id + "' "
                            "AND date > now() - interval '" + fullInterval + "';";
        QSqlQuery queryCount;
        queryCount.prepare(strQueryCount);
        queryCount.exec();
        queryCount.next();
        int count = queryCount.value(0).toInt();

        // Формируем json объект
        sendObject.insert(name, count);
    }

    sendObject.insert("success", true);

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::getStatistics, &sendObject);
}

void Server::AddAction(int user_id, Actions::ActionsId action)
{
    QString userId   = QString::number(user_id);
    QString actionId = QString::number(action);

    // Отправляем запрос к БД
    QString strQuery = "INSERT INTO statistics(user_id, action_id, date) "
                       "VALUES ('" + userId + "', '" + actionId + "', NOW());";

    QSqlQuery query;
    query.prepare(strQuery);
    if (!query.exec())
        qDebug() << query.lastError();
}

void Server::Error(MySocket *socket)
{
    // Формируем ответный пакет
    QJsonObject sendObject;
    sendObject.insert("success", false);
    sendObject.insert("error", "Команда не найдена, либо у вас недостаточно прав.");

    // Отправляем ответ пользователю
    SendToClient(socket, Commands::error, &sendObject);
}

void Server::WriteOnSessionFile(const QString &text)
{
    // Получаем текущее время
    QString time = QTime::currentTime().toString("hh:mm:ss");

    // Пишем информацию в файл
    QTextStream ts(&_logFile_sessions);
    ts << time << ' ' << text << '\n';
}

bool Server::CheckAccountExistence(QString &login)
{
    QString strQuery = "SELECT id FROM users WHERE login = '" + login + "';";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();

    if(query.next())
        return true;

    return false;
}

bool Server::CheckFavoriteDocExistence(MySocket *socket, int id_doc)
{
    QString strQuery = "SELECT * FROM favorites_docs "
                       "WHERE user_id = '" + QString::number(socket->user->id) + "' "
                       "AND doc_id = '" + QString::number(id_doc) + "';";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();

    if(query.next())
        return true;

    return false;
}

int Server::GetRoleId(QString &name)
{
    // Выясняем id роли
    QString strQuery = "SELECT id FROM roles WHERE name = '" + name + "';";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    query.next();

    return query.value(0).toInt();
}

int Server::GetLevelId(QString &level)
{
    // Выясняем id уровня
    QString strQuery = "SELECT id FROM levels WHERE level = '" + level + "';";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    query.next();

    return query.value(0).toInt();
}

int Server::GetDocNumber()
{
    // Формируем и отправляем запрос к БД к таблице docs
    QString strQuery = "SELECT MAX(id) "
                       "FROM docs;";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    query.next();
    int docs_max = query.value(0).toInt();

    // Формируем и отправляем запрос к БД  к таблице docs_trash
    strQuery = "SELECT MAX(id) "
               "FROM docs_trash;";
    query.prepare(strQuery);
    query.exec();
    query.next();
    int docsTrash_max = query.value(0).toInt();

    // Сравниваем значение и выбираем большее
    int result;
    docs_max > docsTrash_max ? result = docs_max : result = docsTrash_max;

    // Имя документа в хранилище - это его будущий номер в БД
    return result + 1;
}

QString Server::GetDocName(const QString &path)
{
    int index = path.lastIndexOf('/');
    return path.mid(index + 1);
}

bool Server::AddDocToTrashtable(const QString& idDoc, const QString& path, int moder_id)
{
    QString strQuery = "SELECT name, level_id, load_date, user_id FROM docs WHERE id = '" + idDoc + "';";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    if (!query.next())
        return false;

    QString name      = query.value("name").toString();
    QString level_id  = QString::number(query.value("level_id").toInt());
    QString load_date = query.value("load_date").toString();
    QString user_id   = QString::number(query.value("user_id").toInt());

    strQuery = "INSERT INTO docs_trash(id, name, level_id, path, user_id, moder_id, load_date, delete_date) "
               "VALUES ('" + idDoc + "', '" + name + "', '" + level_id + "', '" + path + "',  '" + user_id + "', "
                        "'" + QString::number(moder_id) + "', '" + load_date + "', NOW());";
    query.prepare(strQuery);
    if(query.exec())
        return true;
    return false;
}

bool Server::GetAccStatus(int id)
{
    QString strQuery = "SELECT active FROM users WHERE id = '" + QString::number(id) + "';";
    QSqlQuery query;
    query.prepare(strQuery);
    query.exec();
    query.next();

    bool active = query.value(0).toBool();
    return active;
}
