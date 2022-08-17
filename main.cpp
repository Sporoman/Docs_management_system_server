#include <QCoreApplication>
#include "server.h"

#include <QDir>
#include <QFile>

QFile _logFile;

void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Кодировка для Русского языка
    setlocale(LC_ALL, "");

    // Проверяем параметры запуска
    QString ip;
    int port = 0;

    if (argc >= 1)
        ip = argv[1];
    if (argc >= 2)
        port = QString(argv[2]).toInt();

    // Создаём лог-файл
    if(!QDir("logs").exists())  // Создаём папку для логов, если она не создана
        QDir().mkdir("logs");
    QString dateTime = QDateTime::currentDateTime().toString("yyyy.MM.dd hh-mm-ss");
    _logFile.setFileName(QString("logs/" + dateTime + " server.log"));
    _logFile.open(QIODevice::WriteOnly);

    // Устанавливаем кастомные настройки для вывода qDebug()
    qInstallMessageHandler(customMessageOutput);

    // Запускаем сервер
    Server server(ip, port);
    server.startServer();

    // Завершаем работу
    int exitCode = a.exec();

    _logFile.close();
    return exitCode;
}

void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Получаем текущее время
    QString time = QTime::currentTime().toString("hh:mm:ss");

    // Пишем информацию в файл
    QTextStream ts(&_logFile);
    ts << time << ' ' << msg << '\n';

    // Пишем и сбрасываем информацию в консоль
    fprintf(stderr, "%s %s\n", time.toLocal8Bit().constData(), msg.toLocal8Bit().constData());
    fflush(stderr);
}
