# Система для автоматизации документооборота | <a href="#-сервер-"><b>-Сервер-</b></a>

<p>Данное приложение является результатом стремления разобраться, как же все эти системы работают. В разработке участвовали исключительно знания по C++ и Qt без опыта построения каких-либо серьёзных архитектур. По ходу кодирования я пытался оставить как можно больше комментариев, которые могут помочь при разборе этой системы. Представленные наработки можно использовать как учебный ресурс, если вы, также как и я, ищете знания и начинаете свой путь в разработке.</p>

<p>Система представлена в виде двух проектов: <a href="#-сервер-"><b>Сервер</b></a> и <a href="https://github.com/Sporoman/Docs_management_system_client"><b>Клиент</b></a>.</p>

<p>Основная цель этой разработки - решение задачи автоматизации процесса документооборота. Для системы были спроектированы три роли:</p>

- Администратор
- Пользователь
- Модератор 

<p>Каждая из этих ролей имеет свой функционал, который позволяет полноценно использовать все возможности системы и устраняет необходимость влезать в базу данных и сервер ручками напрямую.</p>

## Функционал ролей
### Администратор

- Создание новых аккаунтов в системе 
- Редактирование системной информации у существующих аккаунтов
- Заморозка и разморозка аккаунтов
- Просмотр профилей пользователей

### Пользователь

- Поиск документов
- Скачивание документов
- Загрузка документов
- Добавление документов в "Избранное" (и удаление их от туда)

### Модератор

- Просмотр и редакция информации о документах
- Удаление документов
- Просмотр статистики об отслеживаемых действиях за определенный период
- Генерация PDF отчёта по статистике

### Общее для всех

- Авторизация
- Возможность просматривать и редактировать свой профиль

# -Сервер-

<p>На сервере находится вся логика приложения. Он обрабатывает входящие запросы от клиента, выполняет заданные действия, общается с базой данных и возвращает ответ клиенту. Связь клиента и сервера устанавливается при помощи сокетов. Между собой они общаются в виде заданной структуры сообщений в json формате. </p>

## Возможности сервера

- Авторизация пользователей
- Логирование действий пользователей
- Отдельное логирование подключений и отключений пользователей
- Валидация входящих данных
