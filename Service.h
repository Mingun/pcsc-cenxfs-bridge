#ifndef PCSC_CENXFS_BRIDGE_Service_H
#define PCSC_CENXFS_BRIDGE_Service_H

#pragma once

#include <string>

// CEN/XFS API
#include <xfsapi.h>
// PC/CS API
#include <winscard.h>

#include "EventSupport.h"
#include "PCSCStatus.h"
#include "Settings.h"

class PCSC;
class Service : public EventNotifier {
    PCSC& pcsc;
    /// Хендл XFS-сервиса, который представляет данный объект
    HSERVICE hService;
    /// Хендл карты, с которой будет производиться работа.
    SCARDHANDLE hCard;
    /// Протокол, по которому работает карта.
    DWORD dwActiveProtocol;
    /// Настройки данного сервиса.
    Settings settings;
    // Данный класс будет создавать объекты данного класса, вызывая конструктор.
    friend class PCSC;
private:
    /** Открывает указанную карточку для работы.
    @param pcsc Ресурсный менеджер подсистемы PC/SC.
    @param hService Хендл, присвоенный сервису XFS-менеджером.
    @param settings Настройки XFS-сервиса.
    */
    Service(PCSC& pcsc, HSERVICE hService, const Settings& settings);
public:
    ~Service();
    inline HSERVICE handle() const { return hService; }

    Status open(SCARDCONTEXT hContext);
    Status close();

    Status lock();
    Status unlock();

    inline void setTraceLevel(DWORD level) { settings.traceLevel = level; }
    /** Данный метод вызывается при любом изменении любого считывателя и при изменении количества считывателей.
    @param state
        Информация о текущем состоянии изменившегося считывателя.
    */
    void notify(SCARD_READERSTATE& state) const;
public:// Функции, вызываемые в WFPGetInfo
    std::pair<WFSIDCSTATUS*, Status> getStatus();
    std::pair<WFSIDCCAPS*, Status> getCaps() const;
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
    */
    void asyncRead(DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID);

    std::pair<WFSIDCCARDDATA*, Status> read() const;
    std::pair<WFSIDCCHIPIO*, Status> transmit(WFSIDCCHIPIO* input) const;
private:
    void log(std::string operation, Status st) const;
};

#endif // PCSC_CENXFS_BRIDGE_Service_H