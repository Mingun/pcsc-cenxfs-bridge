PC/SC-CEN/XFS-bridge
====================
Данный проект реализует мост между протоколами [PC/SC][1] (протокол для общения c smart-картами)
и протоколом [CEN/XFS][2], используемом банкоматным ПО для доступа к устройствам, в том числе
чиповым считывателям карт.

Лиценизя MIT. В кратце, возможно бесплатное использование в коммерческих и открытых проектах. Текст
лицензии на русском и английском языках в файле LICENSE.md. Юридическую силу имеет только английский
вариант.

Зависимости
-----------
1. Boost (должна быть прописана переменная окружения %BOOST_ROOT%, указывающая на корневой каталог
boost-а, т.е. каталог, содержащий папки libs, doc, stage и т.п. предполагается, что boost собран в
каталог по умолчанию, коим является stage)
  1.1. `boost.chrono`
  1.2. `boost.thread`
  1.3. `boost.date_time` (зависимость от `boost.thread`)
2. [XFS SDK][1] -- взять можно у проекта [freexfs][4]. Там же содержится и документация. Также можно
взять на официальном FTP-сайте, но его достаточно трудно отыскать.
3. Подсистема PC/SC являтется частью SDK Windows.

Сборка
------
Запустить командную строку Visual Studio и выполнить в ней команду
```
make
```
Либо открыть обычную командную строку в выполнить команды, предварительно заменив `%VC_INSTALL_DIR%`
на путь к установленной MSVC. Так как Kalignite собран под x86, то и библиотеку будем собирать под
эту архитектуру. Естественно, когда появится 64-битная вверсия, необходимо будет использовать `x86_amd64`:
```
"%VC_INSTALL_DIR%\VC\vcvarsall.bat" x86
make
```

Используется C++03. Проверена сборка следующими компиляторами:

1. MSVC 2005
2. MSVC 2013

Лицензия
--------
Лицензия MIT. Вкратце -- разрешено бесплатное использование как в свободных, так и в комменрческих проектах.

Реализация
----------
При загрузке динамической библиотеки создается глобальный объект `PCSC`, в конструкторе которого
инициализируется подсистема PC/SC и запускается поток опроса изменений в устройствах и подключения
новых устройств. В деструкторе глобального объекта соединение с подсистемой PC/SC закрывается, это
происходит автоматически, когда менеджер XFS выгружает библиотеку.

Класс `PCSC` содержит список задач на чтение карты, которые создаются при вызове метода `WFPExecute`,
и список сервисов, представляющих открытые XFS-менеджером сервисы (через `WFPOpen`).

Когда происходит событие PC/SC, отдельный поток сначала уведомляет все подписавшиеся окна (через
`WFPRegister`) на изменения (естественно, выполнятеся трансляция события из PC/SC в XFS форму), а
затем уведомляет все задачи обо всех произошедших изменениях. Таким образом реализуется требование 
XFS, что все события должны быть испущены до того, как произойдет `WFS_xxx_COMPLETE`-событие.

Если задача считает, что изменение ей интересно, она генерирует событие `WFS_xxx_COMPLETE` и ее метод
`match` возвращает `true`, в результате чего она удаляется из списка задач.

Известные проблемы
------------------
1. Пока при попытке использовать библиотеку Kalignite выдает ошибку о ненормальном завершении программы.

Протестированные считыватели
----------------------------
1. OMNIKEY AG Smart Card Reader USB 0 -- работа пока не подтверждена

Порядок открытия сервис-провайдера Kalignite-ом
-----------------------------------------------

1. Загрузка библиотеки с реализацией сервис-провайдера. В этот момент устанавливается подключение к
   подсистеме PC/SC: `SCardEstablishConnect`.
2. Вызов `WFPOpen(?, "CardReader", 0, "Kalignite App CFRJ", ?, 120000, ...)`. В этот момент осуществляется
   поиск PC/SC считывателей.
3. Вызов со всеми классами событий `WFPRegister(?, SERVICE_EVENTS | USER_EVENTS | SYSTEM_EVENTS | EXECUTE_EVENTS, ...)`
4. Установка уровня трассировки провайдера `WFMSetTraceLevel(...)`
5. Получение статуса считывателя `WFPGetInfo(?, WFS_INF_IDC_STATUS, ?, 300000, ...)`
6. Получение возможностей считывателя `WFPGetInfo(?, WFS_INF_IDC_CAPABILITIES, ?, 300000, ...)`
7. Если считыватель не умеет читать треки, то Kalignite падает с Fatal error.
8. Выполнение команды `WFSAsyncExecute(?, WFS_CMD_IDC_READ_RAW_DATA, ?, WFS_INDEFINITE_WAIT, ...)` (ожидание вставки карты с бесконечным таймаутом)
9. Через некоторое время Kalignite отменяет запрос: `WFPCancelAsyncRequest(...)`
10. Снова получает статус считывателя: `WFPGetInfo(?, WFS_INF_IDC_STATUS, ?, 300000, ...)`

[1]: http://www.pcscworkgroup.com/
[2]: http://www.cen.eu/work/areas/ict/ebusiness/pages/ws-xfs.aspx
[3]: https://code.google.com/p/freexfs/downloads/detail?name=XFS%20SDK3.0.rar&can=2&q=
[4]: https://code.google.com/p/freexfs/