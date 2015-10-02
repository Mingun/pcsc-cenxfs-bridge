#ifndef PCSC_CENXFS_BRIDGE_Service_H
#define PCSC_CENXFS_BRIDGE_Service_H

#pragma once

// CEN/XFS API -- Должно быть сверху, т.к., если поместить
#include <xfsapi.h>

#include "EventSupport.h"
#include "Settings.h"

#include "PCSC/ProtocolTypes.h"
#include "PCSC/Status.h"

#include "XFS/ReadFlags.h"
#include "XFS/ResetAction.h"

#include <string>
// Для std::pair
#include <utility>
// CEN/XFS API -- Должно быть сверху, т.к., если поместить здесь,
// то начинаются странные ошибки компиляции из winnt.h как минимум в MSVC 2005.
//#include <xfsapi.h>
// PC/CS API
#include <winscard.h>

class Manager;
class Service : public EventNotifier {
    Manager& pcsc;
    /// Хендл XFS-сервиса, который представляет данный объект
    HSERVICE hService;
    /// Хендл карты, с которой будет производиться работа.
    SCARDHANDLE hCard;
    /// Протокол, по которому работает карта.
    PCSC::ProtocolTypes mActiveProtocol;
    /// Имя считывателя, уведомления от которого обрабатываются данным сервис-провайдером.
    /// Может либо быть явно заданным в настройках, либо заполнятся в момент обнаружения
    /// карточки в любом из доступных считывателей. В последнем случае, до тех пор, пока
    /// карточка не будет вынута, все события от прочих считывателей будут игнорироваться.
    std::string mBindedReaderName;
    /// Настройки данного сервиса.
    Settings mSettings;
    /// Флаг, отвечающий за то, что после создания сервиса он уже узнал текущее
    /// состояние считывателей. При создании сервиса данный флаг выставлен в `false`,
    /// а при первом уведомлении о считывателях он устанавливается в `true`.
    bool mInited;
    // Данный класс будет создавать объекты данного класса, вызывая конструктор.
    friend class ServiceContainer;
private:
    /** Открывает указанную карточку для работы.
    @param pcsc Ресурсный менеджер подсистемы PC/SC.
    @param hService Хендл, присвоенный сервису XFS-менеджером.
    @param settings Настройки XFS-сервиса.
    */
    Service(Manager& pcsc, HSERVICE hService, const Settings& settings);
public:
    ~Service();

    PCSC::Status open(const char* readerName);
    PCSC::Status close();

    PCSC::Status lock();
    PCSC::Status unlock();

    inline void setTraceLevel(DWORD level) { mSettings.traceLevel = level; }
    /** Данный метод вызывается при любом изменении любого считывателя и при изменении количества считывателей.
    @param state
        Информация о текущем состоянии изменившегося считывателя.
    */
    void notify(const SCARD_READERSTATE& state, bool deviceChange);
    /** Проверяет, что сервис ожидает сообщения от данного считывателя. */
    bool match(const SCARD_READERSTATE& state, bool deviceChange);
public:// Функции, вызываемые в WFPGetInfo
    std::pair<WFSIDCSTATUS*, PCSC::Status> getStatus();
    std::pair<WFSIDCCAPS*, PCSC::Status> getCaps() const;
public:// Функции, вызываемые в WFPExecute
    /** Начинает операцию ожидания вставки карточки в считыватель. Как только карточка
        будет вставлена в считыватель, генерирует сообщение `WFS_EXEE_IDC_MEDIAINSERTED`,
        а затем сообщение `WFS_EXECUTE_COMPLETE` с результатом `WFS_SUCCESS`.
    @par
        Если карточка не появится в считывателе за время `dwTimeOut`, то генерируется
        сообщение `WFS_EXECUTE_COMPLETE` с результатом `WFS_ERR_TIMEOUT`.
    @par
        Если операция ожидания была отменена, то генерируется сообщение
        `WFS_EXECUTE_COMPLETE` с результатом `WFS_ERR_CANCELED`.

    @param dwTimeOut
        Таймаут ожидания появления в считывателе карточки.
    @param hWnd
        Окно, которому будет доставлено сообщение `WFS_EXECUTE_COMPLETE` с результатом
        выполнения функции.
    @param ReqID
        Трекинговый номер для отслеживания запроса, передается в сообщении
        `WFS_EXECUTE_COMPLETE`.
    @param forRead
        Список данных, которые необходимо прочитать. Реально читаются только `WFS_IDC_CHIP`,
        для всех остальных типов возвращается признак того, что значение прочитать не удалось.
    */
    void asyncRead(DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID, XFS::ReadFlags forRead);

    std::pair<DWORD, BYTE*> readATR() const;
    WFSIDCCARDDATA* readChip() const;
    WFSIDCCARDDATA* readTrack2() const;
    WFSIDCCARDDATA** wrap(WFSIDCCARDDATA* iccData, XFS::ReadFlags forRead) const;

    /** Осуществляет передачу данных из входного параметра чипу и получет от него ответ.

    @param input
        Буфер, полученный от подсистемы XFS, содержащий параметры протокола и передаваемые данные.

    @return
        Пару, содержащую буфер, который необходимо передать приложению (оно заботится об освобождении
        памяти) и статус выполнения команды.
    */
    std::pair<WFSIDCCHIPIO*, PCSC::Status> transmit(const WFSIDCCHIPIO* input) const;
    /// Выполняет реинициализацию чипа.
    std::pair<WFSIDCCHIPPOWEROUT*, PCSC::Status> reset(XFS::ResetAction action) const;
public:// Служебные функции
    inline HSERVICE handle() const { return hService; }
    inline const Settings& settings() const { return mSettings; }
    inline const std::string& bindedReader() const { return mBindedReaderName; }
};

#endif // PCSC_CENXFS_BRIDGE_Service_H