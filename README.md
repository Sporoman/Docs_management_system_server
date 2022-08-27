# Система для автоматизации документооборота -Сервер-

<p>Данное приложение является результатом стремления разобраться, как же все эти системы работают. В разработке участвовали исключительно знания по C++ и Qt без опыта построения каких-либо серьёзных архитектур. По ходу кодирования, я пытался оставить как можно больше комментариев, которые могут помочь при разборе этой системы. Представленные наработки можно использовать как учебный ресурс, если вы также как и я ищете знания и начинаете свой путь в разработке.</p>

<p>Система представлена в виде двух проектов: Сервер и <a href="https://github.com/Sporoman/Docs_management_system_client">Клиент</a>.</p>

<p>Основная цель этой разработки - решение задачи автоматизации процесса документооборота. Для системы были спроектированы три роли:</p>
- Администратор
- Пользователь
- Модератор 

<p>Каждая из этих ролей имеет свой функционал, который позволяет полноценно использовать возможности системы и устраняет необходимость работать с базой данных напрямую.</p>

# Функционал ролей
## Администратор

Я спроектировал три роли (администратор, пользователь, модератор), к каждой из которых присвоил свой логический функционал. Основой конечно является возможность загрузки документов на сервер, и возможность их последующего скачивания (с разбитием на уровни доступа, с возможность добавление документов в "избранное" для каждого пользователя и т.д.). Если вам интересно, я прикладываю к этому письму презентацию к своему диплому - там можно поверхностно посмотреть как это всё выглядит.
