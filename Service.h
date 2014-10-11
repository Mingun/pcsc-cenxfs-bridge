#ifndef PCSC_CENXFS_BRIDGE_Service_H
#define PCSC_CENXFS_BRIDGE_Service_H

#pragma once

#include <string>

// CEN/XFS API -- Для WFMOutputTraceData
#include <xfsadmin.h>
// PC/CS API
#include <winscard.h>

#include "EventSupport.h"
#include "PCSCStatus.h"
#include "XFSResult.h"

class Service : public EventNotifier {
    /// Хендл XFS-сервиса, который представляет данный объект
    HSERVICE hService;
    /// Хендл карты, с которой будет производиться работа.
    SCARDHANDLE hCard;
    /// Протокол, по которому работает карта.
    DWORD dwActiveProtocol;
    /// Название считывателя, для которого открыт протокол.
    std::string readerName;

    // Данный класс будет создавать объекты данного класса, вызывая конструктор.
    friend class PCSC;
private:
    /** Открывает указанную карточку для работы.
    @param hContext Ресурсный менеджер подсистемы PC/SC.
    @param readerName
    */
    Service(HSERVICE hService)
        : hService(hService), hCard(0), dwActiveProtocol(0) {}
public:
    ~Service() {
        if (hCard != 0) {
            close();
        }
    }
    inline HSERVICE handle() const { return hService; }

    Status open(SCARDCONTEXT hContext);
    Status close();

    Status lock();
    Status unlock();
    /// Уведомляет всех слушателей обо всех произошедших изменениях со считывателями.
    void notify(SCARD_READERSTATE& state) const;
    inline void sendResult(HWND hWnd, REQUESTID ReqID, DWORD messageType, HRESULT result) const {
        Result(ReqID, hService, result).send(hWnd, messageType);
    }
public:// Функции, вызываемые в WFPGetInfo
    std::pair<WFSIDCSTATUS*, Status> getStatus();
    std::pair<WFSIDCCAPS*, Status> getCaps();
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
    std::pair<WFSIDCCARDDATA*, Status> read();
    std::pair<WFSIDCCHIPIO*, Status> transmit(WFSIDCCHIPIO* input);
private:
    void log(std::string operation, Status st) const;
};

#endif // PCSC_CENXFS_BRIDGE_Service_H