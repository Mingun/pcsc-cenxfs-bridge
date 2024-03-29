PC/SC-CEN/XFS-bridge
====================
Данный проект реализует мост между протоколами [PC/SC][1] (протокол для общения c smart-картами)
и протоколом [CEN/XFS][2], используемом банкоматным ПО для доступа к устройствам, в том числе
чиповым считывателям карт.

Лицензия
--------
Лицензия MIT. Вкратце, возможно бесплатное использование в коммерческих и открытых проектах. Текст
лицензии на русском и английском языках в файле LICENSE.md. Юридическую силу имеет только английский
вариант.

Зависимости
-----------
1. Boost (должна быть прописана переменная окружения `%BOOST_ROOT%`, указывающая на корневой каталог
boost-а, т.е. каталог, содержащий папки libs, doc, stage и т.п. предполагается, что boost собран в
каталог по умолчанию, коим является stage)
    1. `boost.chrono`
    2. `boost.thread`
    3. `boost.date_time` (зависимость от `boost.thread`)
2. [XFS SDK][1]
    1. взять можно у проекта [freexfs][4]. Там же содержится и документация.
    2. Также можно взять на официальном FTP-сайте, но его достаточно трудно отыскать. На официальном
       сайте группы CEN/XFS найти ссылки на документацию не удалось, к счастью, пользователь **winner13**
       в форума [bankomatchik.ru][6] каким-то чудом отыскал [FTP-ссылку][7].
3. Подсистема PC/SC являтется частью SDK Windows.

Сборка
------
Подготовить зависимости: скачать буст и собрать необходимые библиотеки. Сборку можно выполнить следующим образом:
1. Запустить командную строку Visual Studio
2. Перейти в `%BOOST_ROOT%`
3. Выполнить команду `bootstrap.bat msvc` для сборки инструмента сборки буста
4. Выполнить команду `build-boost.bat` из корня проекта для сбора необходимых библиотек в нужном варианте

Запустить командную строку Visual Studio и выполнить в ней команду
```
make
```
Либо открыть обычную командную строку в выполнить команды, предварительно заменив `%VC_INSTALL_DIR%`
на путь к установленной MSVC. Так как Kalignite собран под x86, то и библиотеку будем собирать под
эту архитектуру. Естественно, когда появится 64-битная версия, необходимо будет использовать `x86_amd64`:
```
"%VC_INSTALL_DIR%\VC\vcvarsall.bat" x86
make
```

Используется C++03. Проверена сборка следующими компиляторами:

1. MSVC 2005
2. MSVC 2008
3. MSVC 2013

Архитектура
-----------
При загрузке динамической библиотеки создается глобальный объект `Manager`, в конструкторе которого
инициализируется подсистема PC/SC и запускается поток опроса изменений в устройствах и подключения
новых устройств. В деструкторе глобального объекта соединение с подсистемой PC/SC закрывается, это
происходит автоматически, когда менеджер XFS выгружает библиотеку.

Хотя может показаться, что можно было напрямую мапить хендл сервис-провайдера (`HSERVICE`), на хендл
контекста PC/SC (`SCARDCONTEXT`), этого не делается потому, что функция `SCardListReaders` блокирующая,
а она требует хендл контекста. Таким образом, если бы на каждый сервис-провайдер был заведен свой PC/SC
контекст, потребовалось бы на каждый создавать по своему потоку для опроса изменений в устройствах.

Класс `Manager` содержит список задач на чтение карты, которые создаются при вызове метода `WFPExecute`,
и список сервисов, представляющих открытые XFS-менеджером сервисы (через `WFPOpen`).

Когда происходит событие PC/SC, отдельный поток сначала уведомляет все подписавшиеся окна (через
`WFPRegister`) на изменения (естественно, выполняется трансляция события из PC/SC в XFS форму), а
затем уведомляет все задачи обо всех произошедших изменениях. Таким образом реализуется требование
XFS, что все события должны быть испущены до того, как произойдет `WFS_xxx_COMPLETE`-событие.

Если задача считает, что изменение ей интересно, она генерирует событие `WFS_xxx_COMPLETE` и ее метод
`match` возвращает `true`, в результате чего она удаляется из списка задач.

События о завершении отправляются Windows функцией PostMessage, которая укладывает ее в очередь сообщений
потока, который обрабатывает события завершения. На совести конечного приложения, что оно предоставляет
хендлы окон, которые существуют в одном потоке, иначе `WFS_xxx_COMPLETE`-событие может прийти раньше,
чем прочие виды событий. Кроме того, если события приложения обрабатывает в другом потоке, чем асинхронные
вызовы сервис-провайдера, то события могут прийти раньше, чем завершится асинхронный вызов, их инициирующий.
На совести приложения работать правильно в таком случае и не терять уведомления. Это особенность XFS API,
оно предъявляет очень жесткие требования к приложению.

Настройки
---------
Большинство настроек предназначены для обхода проблем, обнаруженных в процессе тестирования, но некоторые
управляют штатным функционалом сервис-провайдера. Все настройки выполняются в ветке реестра, под веткой
с провайдером логического сервиса (`HKEY_LOCAL_MACHINE\SOFTWARE\XFS\SERVICE_PROVIDERS\<провайдер>`).
Все `DWORD` значения в таблице являются логическими флагами, со значением `0` -- сброшены, любое другое --
выставлен:

Название        |Тип     |Назначение
----------------|--------|----------
ReaderName      |`REG_SZ`|PC/SC название считывателя, с которым должен работать данный провайдер. Если параметр пустой или отсутствует, то слушаются все подключенные считыватели и используется первый, в который будет вставлена карточка (это делается каждый раз, т.е. если карточку вытащили из первого считывателя и вставили во второй, то работа будет происходить со вторым считывателем). Если не пустой, то событие вставки карты будет обрабатываться только от указанного считывателя
Exclusive       |`DWORD` |Если флаг установлен, то считыватель будет использовать карту в монопольном режиме (`SCARD_SHARE_EXCLUSIVE`), т.е. никто, кроме сервис-провайдера, не сможет общаться с картой одновременно. Если сброшен или отсутсвует, то карта открывается в совместном режиме (`SCARD_SHARE_SHARED`)
**Workarounds** |        |Подраздел -- обходы багов
CorrectChipIO   |`DWORD` |Анализировать длину передаваемых чипу команд и корректировать ее в соответствии с тем, что передается в заголовке команды. Kalignite может передавать лишние байты в команде чтения, а это вызывает ошибку у функции `SCardTransmit`. Если сброшен или отсутствует, то анализ не производится
CanEject        |`DWORD` |Сообщать, что устройство умеет извлекать карты в возможностях устройства и принимать команду извлечения карты (`WFS_CMD_IDC_EJECT_CARD`). При этом ничего не делается. Если сброшен или отсутствует, то в возможностях сообщать, что команда не поддерживается, а при получении этой команды возвращать ошибку **неподдерживаемая команда** (`WFS_ERR_UNSUPP_COMMAND`). Kalignite пытается выдавать карту, даже если эта возможность не поддерживается, и не ожидает, что команда не будет выполнена, падая с Fatal Error в случае кода ответа, отличного от успеха
**Track2**      |        |Подраздел **Workarounds** -- настройки второй дорожки
_(по умолчанию)_|`REG_SZ`|Значение второй дорожки, сообщаемое провайдером, без начального и конечного разделителей, как будет отдано приложению. Значение сообщается, только если флаг `Report` взведен
Report          |`DWORD` |Сообщать о возможности чтения второй магнитной дорожки. Если флаг взведен, а значение трека пустое, то при чтении возвращается код ошибки **данные отсутствуют** (`WFS_IDC_DATAMISSING`). Если флаг сброшен, то в возможностях устройства сообщается, что чтение второй дорожки не поддерживается. Kalignite требует, чтобы вторая дорожка была прочитана, даже если в условиях чтения указать не читать вторую дорожку (на момент чтения все в порядке, но потом при работе сценария он падает с Fatal Error из-за отсутствия второй дорожки)

Протестированные считыватели
----------------------------
Для работы с Kaliginte-ом были активированы все обходы багов.

1. [OMNIKEY 3121][5] (pcsc_scan знает его, как OMNIKEY AG Smart Card Reader USB 0) -- успешно работает
2. [ACR38u][8] -- успешно работает

[1]: http://www.pcscworkgroup.com/
[2]: http://www.cen.eu/work/areas/ict/ebusiness/pages/ws-xfs.aspx
[3]: https://code.google.com/p/freexfs/downloads/detail?name=XFS%20SDK3.0.rar&can=2&q=
[4]: https://code.google.com/p/freexfs/
[5]: http://www.hidglobal.com/products/readers/omnikey/3121
[6]: http://bankomatchik.ru/forums/topic/4654#p65827
[7]: ftp://ftp.cenorm.be/PUBLIC/CWAs/other/WS-XFS/SDK%20XFS3/sdk303.zip
[8]: http://www.acr38u.com/