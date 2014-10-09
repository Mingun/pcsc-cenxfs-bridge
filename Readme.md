PC/SC-CEN/XFS-bridge
====================
Данный проект реализует мост между протоколами [PC/SC][1] (протокол для общения c smart-картами)
и протоколом [CEN/XFS][2], используемом банкоматным ПО для доступа к устройствам, в том числе
чиповым считывателям карт.

Порядок открытия сервис-провайдера Kalignite-ом
-----------------------------------------------

1. Загрузка библиотеки с реализацией сервис-провайдера. В этот момент устанавливается подключение к
   подсистеме PC/SC: `SCardEstablishConnect`.
2. Вызов `WFPOpen(?, "CardReader", 0, "Kalignite App CFRJ", ?, 120000, ...)`. В этот момент осуществляется
   поиск PC/SC считывателей.
3. Вызов со всеми классами событий `WFPRegister(?, SERVICE_EVENTS | USER_EVENTS | SYSTEM_EVENTS | EXECUTE_EVENTS, ...)`
4. Установка уровня трассировки провайдера `WFMSetTraceLevel(...)`
5. Получение статуса считывателя `WFPGetInfo(?, WFS_INF_IDC_STATUS, 0, dwTimeOut=300000, ...)`
5. Получение возможностей считывателя `WFPGetInfo(?, WFS_INF_IDC_CAPABILITIES, 0, dwTimeOut=300000, ...)`
6. Если считыватель не умеет читать треки, то Kalignite падает с Fatal error.

[1]: http://www.pcscworkgroup.com/
[2]: http://www.cen.eu/work/areas/ict/ebusiness/pages/ws-xfs.aspx