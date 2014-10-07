#ifndef PCSC_CENXFS_BRIDGE_Card_H
#define PCSC_CENXFS_BRIDGE_Card_H

#pragma once

#include <string>

// PC/CS API
#include <winscard.h>

#include "EventSupport.h"
#include "XFSResult.h"

class Card : public EventNotifier {
    /// Хендл карты, с которой будет производиться работа.
    SCARDHANDLE hCard;
    /// Протокол, по которому работает карта.
    DWORD dwActiveProtocol;
    HSERVICE hService;
    std::string readerName;

    // Данный класс будет создавать объекты данного класса, вызывая конструктор.
    friend class PCSC;
private:
    /** Открывает указанную карточку для работы.
    @param hContext Ресурсный менеджер подсистемы PC/SC.
    @param readerName
    */
    Card(HSERVICE hService, SCARDCONTEXT hContext, const char* readerName) : hService(hService), readerName(readerName) {
        Status st = SCardConnect(hContext, readerName, SCARD_SHARE_SHARED,
            // У нас нет предпочитаемого протокола, работаем с тем, что дают
            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
            // Получаем хендл карты и выбранный протокол.
            &hCard, &dwActiveProtocol
        );
        log("Connect to", st);
    }
public:
    ~Card() {
        // При закрытии соединения ничего не делаем с карточкой, оставляем ее в считывателе.
        Status st = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        log("Disconnect from", st);
    }

    Result lock(REQUESTID ReqID) {
        Status st = SCardBeginTransaction(hCard);
        log("Lock", st);

        return Result(ReqID, hService, st);
    }
    Result unlock(REQUESTID ReqID) {
        // Заканчиваем транзакцию, ничего не делаем с картой.
        Status st = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
        log("Unlock", st);

        return Result(ReqID, hService, st);
    }

    inline Result result(REQUESTID ReqID, HRESULT result) {
        return Result(ReqID, hService, result);
    }
    void sendResult(HWND hWnd, REQUESTID ReqID, DWORD messageType, HRESULT result) {
        Result(ReqID, hService, result).send(hWnd, messageType);
    }
public:// Функции, вызываемые в WFPGetInfo
    WFSIDCSTATUS* getStatus() {
        // Состояние считывателя.
        MediaStatus state;
        DWORD nameLen;
        DWORD attrLen;
        Status st = SCardStatus(hCard,
            // Имя получать не будем, тем не менее длину получить требуется, NULL недопустим.
            NULL, &nameLen,
            // Небольшой хак допустим, у нас прозрачная обертка, ничего лишнего.
            (DWORD*)&state, &dwActiveProtocol,
            // Атрибуты получать не будем, тем не менее длину получить требуется, NULL недопустим.
            NULL, &attrLen
        );
        log("SCardStatus: ", st);

        if (!st) {
            return NULL;
        }

        WFSIDCSTATUS* lpStatus = xfsAlloc<WFSIDCSTATUS>();
        // Набор флагов, определяющих состояние устройства. Наше устройство всегда на связи,
        // т.к. в противном случае при открытии сесии с PC/SC драйвером будет ошибка.
        lpStatus->fwDevice = WFS_IDC_DEVONLINE;
        lpStatus->fwMedia = state.translateMedia();
        // У считывателей нет корзины для захваченных карт.
        lpStatus->fwRetainBin = WFS_IDC_RETAINNOTSUPP;
        // Модуль безопасноси отсутствует
        lpStatus->fwSecurity = WFS_IDC_SECNOTSUPP;
        // Количество возвращенных карт. Так как карта вставляется и вытаскивается руками,
        // то данный параметр не имеет смысла.
        //TODO Хотя, может быть, можно будет его отслеживать как количество вытащенных карт.
        lpStatus->usCards = 0;
        lpStatus->fwChipPower = state.translateChipPower();
        //TODO: Добавить lpszExtra со всеми параметрами, полученными от PC/SC.
        return lpStatus;
    }
    WFSIDCCAPS* getCaps() {
        WFSIDCCAPS* lpCaps;
        HRESULT h = WFMAllocateBuffer(sizeof(WFSIDCCAPS), WFS_MEM_ZEROINIT, (LPVOID*)&lpCaps);
        assert(h >= 0);

        // Устройство является считывателем карт.
        lpCaps->wClass = WFS_SERVICE_CLASS_IDC;
        // Карта вставляется рукой и может быть вытащена в любой момент.
        lpCaps->fwType = WFS_IDC_TYPEDIP;
        // Устройство содержит только считыватель карт.
        lpCaps->bCompound = FALSE;
        // Какие треки могут быть прочитаны -- никакие, только чип.
        lpCaps->fwReadTracks = WFS_IDC_NOTSUPP;
        // Какие треки могут быть записаны -- никакие, только чип.
        lpCaps->fwWriteTracks = WFS_IDC_NOTSUPP;
        //TODO: Виды поддерживаемых картой протоколов.
        //lpCaps->fwChipProtocols = WFS_IDC_CHIPT0 | WFS_IDC_CHIPT1;
        // Максимальное количество карт, которое устройство может захватить. Всегда 0, т.к. не захватывает.
        lpCaps->usCards = 0;
        // Тип модуля безопасности. Не поддерживается.
        lpCaps->fwSecType = WFS_IDC_SECNOTSUPP;
        // Данные два поля нужны только для моторизированных считывателей, мы не поддерживаем.
        lpCaps->fwPowerOnOption = WFS_IDC_NOACTION;
        lpCaps->fwPowerOffOption = WFS_IDC_NOACTION;
        // Возможность программирования Flux Sensor. На всякий случай скажем, что не умеем.
        //TODO: Что такое Flux Sensor?
        lpCaps->bFluxSensorProgrammable = FALSE;
        // Можно ли начать чтение/запись на карту, когда она уже была вытащена (при этом она затягивается
        // обратно). Так как у нас не моторизированный считыватель, то мы этого не умеем.
        lpCaps->bReadWriteAccessFollowingEject = FALSE;
        // Величина магнитной силы, которую необходипо приложить для размагничивания намагниченной
        // дорожки. Так как чтение/запись треков не поддерживается, то не поддерживаем.
        lpCaps->fwWriteMode = WFS_IDC_NOTSUPP;
        // Возможности считавателя по управлению питанием чипа.
        lpCaps->fwChipPower = WFS_IDC_NOTSUPP;
        //TODO: Добавить lpszExtra со всеми параметрами, полученными от PC/SC.
        return lpCaps;
    }
public:// Функции, вызываемые в WFPExecute
    WFSIDCCARDDATA* readChip() {
        WFSIDCCARDDATA* data = xfsAlloc<WFSIDCCARDDATA>();
        // data->lpbData содержит ATR (Answer To Reset), прочитанный с чипа
        data->wDataSource = WFS_IDC_CHIP;
        data->wStatus = WFS_IDC_DATAOK;
        // data->ulDataLength = ;
        // data->lpbData = ;
        return data;
    }
    WFSIDCCHIPIO* transmit(WFSIDCCHIPIO* input) {
        assert(input != NULL);

        WFSIDCCHIPIO* result = xfsAlloc<WFSIDCCHIPIO>();
        result->wChipProtocol = input->wChipProtocol;

        SCARD_IO_REQUEST ioRq;//TODO: Получить протокол.
        SCARD_IO_REQUEST ioRs;//TODO: Получить протокол.
        Status st = SCardTransmit(hCard,
            &ioRq, input->lpbChipData, input->ulChipDataLength
            NULL, result->lpbChipData, result->ulChipDataLength
        );
        log("SCardTransmit", st);

        return result;
    }
private:
    void log(std::string operation, Status st) const {
        std::stringstream ss;
        ss << operation << " card reader '" << readerName << "': " << st;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
};

#endif // PCSC_CENXFS_BRIDGE_Card_H