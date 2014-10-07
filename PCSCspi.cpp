
// CEN/XFS API
#include <xfsspi.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include <sstream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <cassert>

#include "EventSupport.h"
#include "PCSCStatus.h"
#include "PCSCMediaStatus.h"
#include "XFSResult.h"

// Линкуемся с библиотекой реализации стандларта PC/SC в Windows
#pragma comment(lib, "winscard.lib")
// Линкуемся с библиотекой реализации XFS
#pragma comment(lib, "msxfs.lib")
// Линкуемся с библитекой для использования оконных функций (PostMessage)
#pragma comment(lib, "user32.lib")

#define MAKE_VERSION(major, minor) (((major) << 8) | (minor))

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

        WFSIDCSTATUS* lpStatus;
        HRESULT h = WFMAllocateBuffer(sizeof(WFSIDCSTATUS), WFS_MEM_ZEROINIT, (LPVOID*)&lpStatus);
        assert(h >= 0);
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
private:
    void log(std::string operation, Status st) const {
        std::stringstream ss;
        ss << operation << " card reader '" << readerName << "': " << st;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
};

/** Класс, в конструкторе инициализирующий подсистему PC/SC, а в деструкторе закрывающий ее.
    Необходимо Создать ровно один экземпляр данного класса при загрузке DLL и уничтожить его
    при выгрузке. Наиболее просто это делается, путем объявления глобальной переменной данного
    класса.
*/
class PCSC {
public:
    /// Тип для отображения сервисов XFS на карты PC/SC.
    typedef std::map<HSERVICE, Card*> CardMap;
public:
    /// Контекст подсистемы PC/SC.
    SCARDCONTEXT hContext;
    /// Список карт, открытых для взаимодействия с системой XFS.
    CardMap cards;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    PCSC() {
        // Создаем контекст.
        Status st = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
        std::stringstream ss;
        ss << "Establish context: " << st;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~PCSC() {
        for (CardMap::const_iterator it = cards.begin(); it != cards.end(); ++it) {
            delete it->second;
        }
        cards.clear();
        Status st = SCardReleaseContext(hContext);
        std::stringstream ss;
        ss << "Release context: " << st;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    bool isValid(HSERVICE hService) {
        return cards.find(hService) != cards.end();
    }
    Card& open(HSERVICE hService, const char* name) {
        Card* card = new Card(hService, hContext, name);
        cards.insert(std::make_pair(hService, card));
        return *card;
    }
    Card& get(HSERVICE hService) {
        assert(isValid(hService));
        Card* card = cards.find(hService)->second;
        assert(card != NULL);
        return *card;
    }
    void close(HSERVICE hService) {
        assert(isValid(hService));
        CardMap::iterator it = cards.find(hService);

        assert(it->second != NULL);
        // Закрытие PC/SC соединения происхоидт в деструкторе Card.
        delete it->second;
        cards.erase(it);
    }
};

/** При загрузке DLL будут вызваны конструкторы всех глобальных объектов и установится
    соединение с подсистемой PC/SC. При выгрузке DLL вызовутся деструкторы глобальных
    объектов и соединение автоматически закроется.
*/
PCSC pcsc;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/** Устанавливает сессию с подсистемой PC/SC.

@message WFS_OPEN_COMPLETE

@param hService Хендл сессии, с которым необходимо соотнести хендл открываемой карты.
@param lpszLogicalName Имя логического сервиса, настраивается в реестре HKLM\<user>\XFS\LOGICAL_SERVICES
@param hApp Хендл приложения, созданного вызовом WFSCreateAppHandle.
@param lpszAppID Идентификатор приложения. Может быть `NULL`.
@param dwTraceLevel 
@param dwTimeOut Таймаут (в мс), в течении которого необходимо открыть сервис. `WFS_INDEFINITE_WAIT`,
       если таймаут не требуется.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqId Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
@param hProvider Хендл данного сервис-провайдера. Может быть переден, например, в фукнцию WFMReleaseDLL.
@param dwSPIVersionsRequired Диапазон требуетмых версий провайдера. 0x00112233 означает диапазон
       версий [0.11; 22.33].
@param lpSPIVersion Информация об открытом соединении (выходной параметр).
@param dwSrvcVersionsRequired Специфичная для сервиса (карточный ридер) требуемая версия провайдера.
       0x00112233 означает диапазон версий [0.11; 22.33].
@param lpSrvcVersion Информация об открытом соединении, специфичная для вида провайдера (карточный ридер)
       (выходной параметр).
*/
HRESULT  WINAPI WFPOpen(HSERVICE hService, LPSTR lpszLogicalName, 
                        HAPP hApp, LPSTR lpszAppID, 
                        DWORD dwTraceLevel, DWORD dwTimeOut, 
                        HWND hWnd, REQUESTID ReqID,
                        HPROVIDER hProvider, 
                        DWORD dwSPIVersionsRequired, LPWFSVERSION lpSPIVersion, 
                        DWORD dwSrvcVersionsRequired, LPWFSVERSION lpSrvcVersion
) {
    // Возвращаем поддерживаемые версии.
    if (lpSPIVersion != NULL) {
        // Версия XFS менеджера, которая будет использоваться. Т.к. мы поддерживаем все версии,
        // то просто максимальная запрошенная версия (она в младшем слове).
        lpSPIVersion->wVersion = LOWORD(dwSPIVersionsRequired);
        // Минимальная и максимальная поддерживаемая нами версия
        //TODO: Уточнить минимальную поддерживаемую версию!
        lpSPIVersion->wLowVersion  = MAKE_VERSION(0, 0);
        lpSPIVersion->wHighVersion = MAKE_VERSION(255, 255);
    }
    if (lpSrvcVersion != NULL) {
        // Версия XFS менеджера, которая будет использоваться. Т.к. мы поддерживаем все версии,
        // то просто максимальная запрошенная версия (она в младшем слове).
        lpSrvcVersion->wVersion = LOWORD(dwSrvcVersionsRequired);
        // Минимальная и максимальная поддерживаемая нами версия
        //TODO: Уточнить минимальную поддерживаемую версию!
        lpSrvcVersion->wLowVersion  = MAKE_VERSION(0, 0);
        lpSrvcVersion->wHighVersion = MAKE_VERSION(255, 255);
    }

    // Получаем хендл карты, с которой будем работать.
    pcsc.open(hService, lpszLogicalName).sendResult(hWnd, ReqID, WFS_OPEN_COMPLETE, WFS_SUCCESS);

    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED                The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR          An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_TIMEOUT                 The timeout interval expired.
    // WFS_ERR_VERSION_ERROR_IN_SRVC   Within the service, a version mismatch of two modules occurred.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST       The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR        An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE      The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND          The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_POINTER       A pointer parameter does not point to accessible memory.
    // WFS_ERR_INVALID_TRACELEVEL    The dwTraceLevel parameter does not correspond to a valid trace level or set of levels.
    // WFS_ERR_SPI_VER_TOO_HIGH      The range of versions of XFS SPI support requested by the XFS Manager is higher than any supported by this particular service provider.
    // WFS_ERR_SPI_VER_TOO_LOW       The range of versions of XFS SPI support requested by the XFS Manager is lower than any supported by this particular service provider.
    // WFS_ERR_SRVC_VER_TOO_HIGH     The range of versions of the service-specific interface support requested by the application is higher than any supported by the service provider for the logical service being opened.
    // WFS_ERR_SRVC_VER_TOO_LOW      The range of versions of the service-specific interface support requested by the application is lower than any supported by the service provider for the logical service being opened.
    // WFS_ERR_VERSION_ERROR_IN_SRVC Within the service, a version mismatch of two modules occurred.

    return WFS_SUCCESS;
}
/** Завершает сессию (серию запросов к сервису, инициированную вызовом SPI функции WFPOpen)
    между XFS менеджером и указанным сервис-провайдером.
@par
    Если провайдер заблокирован вызовом WFPLock, он автоматически будет разблокирован и отписан от
    всех событий (если это не сделали ранее вызовом WFPDeregister).
@message WFS_CLOSE_COMPLETE

@param hService Закрываемый сервис-провайдер.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqId Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPClose(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    pcsc.close(hService);
    //TODO: Послать окну hWnd сообщение с кодом WFS_CLOSE_COMPLETE и wParam = WFSRESULT;

    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR An internal inconsistency or other unexpected error occurred in the XFS subsystem.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND The hWnd parameter is not a valid window handle.
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/** Подписывает указанное окно на события указанных классов от указанного сервис-провайдера.

@message WFS_REGISTER_COMPLETE

@param hService Сервис, чьи сообщения требуется мониторить.
@param dwEventClass Логический OR классов сообщений, которые нужно мониторить. 0 означает, что
       производится подписка на все классы событий.
@param hWndReg Окно, которое будет получать события указанных классов.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPRegister(HSERVICE hService,  DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Регистрируем событие для окна.
    pcsc.get(hService).add(hWndReg, dwEventClass);
    Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_REGISTER_COMPLETE);
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST     The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR      An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_EVENT_CLASS The dwEventClass parameter specifies one or more event classes not supported by the service.
    // WFS_ERR_INVALID_HSERVICE    The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND        The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_HWNDREG     The hWndReg parameter is not a valid window handle.
    return WFS_SUCCESS;
}
/** Прерывает мониторинг указанных классов сообщений от указанного сервис-провайдера для указанных окон.

@message WFS_DEREGISTER_COMPLETE

@param hService Сервис, чьи сообщения больше не требуется мониторить.
@param dwEventClass Логический OR классов сообщений, которые не нужно мониторить. 0 означает, что отписка
       производится от всех классов событий.
@param hWndReg Окно, которое больше не должно получать указанные сообщения. NULL означает, что отписка
       производится для всех окон.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPDeregister(HSERVICE hService, DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    pcsc.get(hService).remove(hWndReg, dwEventClass);
    Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_DEREGISTER_COMPLETE);
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED The request was canceled by WFSCancelAsyncRequest.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST     The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR      An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_EVENT_CLASS The dwEventClass parameter specifies one or more event classes not supported by the service.
    // WFS_ERR_INVALID_HSERVICE    The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND        The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_HWNDREG     The hWndReg parameter is not a valid window handle.
    // WFS_ERR_NOT_REGISTERED      The specified hWndReg window was not registered to receive messages for any event classes.
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/** Получает эксклюзивный доступ к устройству.

@message WFS_CLOSE_COMPLETE

@param hService Сервис, к которому нужно получить эксклюзивный доступ.
@param dwTimeOut Таймаут (в мс), в течении которого необходимо получить доступ. `WFS_INDEFINITE_WAIT`,
       если таймаут не требуется.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPLock(HSERVICE hService, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    pcsc.get(hService).lock(ReqID).send(hWnd, WFS_SUCCESS);

    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   The function required device access, and the device was not ready or timed out.
    // WFS_ERR_HARDWARE_ERROR  The function required device access, and an error occurred on the device.
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_SOFTWARE_ERROR  The function required access to configuration information, and an error occurred on the software.
    // WFS_ERR_TIMEOUT         The timeout interval expired.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.
    return WFS_SUCCESS;
}
/**
@message WFS_UNLOCK_COMPLETE

@param hService Сервис, к которому больше ну нужно иметь эксклюзивный доступ.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPUnlock(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;

    pcsc.get(hService).unlock(ReqID).send(hWnd, WFS_SUCCESS);
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_NOT_LOCKED      The service to be unlocked is not locked under the calling hService.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.

    //TODO: Послать окну hWnd сообщение с кодом WFS_UNLOCK_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/** Получает различную информацию о сервис-провайдере.

@message WFS_GETINFO_COMPLETE

@param hService Хендл сервис-провайдера, о котором получается информация.
@param dwCategory Вид запрашиваемой информации. Поддерживаются только стандартные виды информации.
@param pQueryDetails Указатель на дополнительные данные для запроса или `NULL`, если их нет.
@param dwTimeOut Таймаут, за который нужно завершить операцию.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPGetInfo(HSERVICE hService, DWORD dwCategory, LPVOID lpQueryDetails, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Для IDC могут запрашиваться только эти константы (WFS_INF_IDC_*)
    switch (dwCategory) {
        case WFS_INF_IDC_STATUS: {      // Дополнительных параметров нет
            WFSIDCSTATUS* status = pcsc.get(hService).getStatus();
            Result(ReqID, hService, WFS_SUCCESS).data(status).send(hWnd, WFS_GETINFO_COMPLETE);
            break;
        }
        case WFS_INF_IDC_CAPABILITIES: {// Дополнительных параметров нет
            WFSIDCCAPS* caps = pcsc.get(hService).getCaps();
            Result(ReqID, hService, WFS_SUCCESS).data(caps).send(hWnd, WFS_GETINFO_COMPLETE);
            break;
        }
        case WFS_INF_IDC_FORM_LIST:
        case WFS_INF_IDC_QUERY_FORM: {
            // Формы не поддерживаем. Форма определяет, в каких местах на треках находятся данные.
            // Так как треки мы не читаем и не пишем, то формы не поддерживаем.
            return WFS_ERR_UNSUPP_COMMAND;
        }
        default:
            return WFS_ERR_INVALID_CATEGORY;
    }
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   The function required device access, and the device was not ready or timed out.
    // WFS_ERR_HARDWARE_ERROR  The function required device access, and an error occurred on the device.
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_DATA    The data structure passed as input parameter contains invalid data..
    // WFS_ERR_SOFTWARE_ERROR  The function required access to configuration information, and an error occurred on the software.
    // WFS_ERR_TIMEOUT         The timeout interval expired.
    // WFS_ERR_USER_ERROR      A user is preventing proper operation of the device.
    // WFS_ERR_UNSUPP_DATA     The data structure passed as an input parameter although valid for this service class, is not supported by this service provider or device.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_CATEGORY   The dwCategory issued is not supported by this service class.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_POINTER    A pointer parameter does not point to accessible memory.
    // WFS_ERR_UNSUPP_CATEGORY    The dwCategory issued, although valid for this service class, is not supported by this service provider.
    //TODO: Послать окну hWnd сообщение с кодом WFS_GETINFO_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
/** Посылает команду для выполнения карточным ридером.

@message WFS_EXECUTE_COMPLETE

@param hService Хендл сервис-провайдера, который должен выполнить команду.
@param dwCommand Вид выполняемой команды.
@param lpCmdData Даные для команды. Зависят от вида.
@param dwTimeOut Количество миллисекунд, за которое команда должна выполниться, или `WFS_INDEFINITE_WAIT`,
       если таймаут не требуется.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPExecute(HSERVICE hService, DWORD dwCommand, LPVOID lpCmdData, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    //Status st = SCardTransmit(translate(hService), );
    // Возможные коды завершения асинхронного запроса (могут возвращаться и другие)
    // WFS_ERR_CANCELED        The request was canceled by WFSCancelAsyncRequest.
    // WFS_ERR_DEV_NOT_READY   The function required device access, and the device was not ready or timed out.
    // WFS_ERR_HARDWARE_ERROR  The function required device access, and an error occurred on the device.
    // WFS_ERR_INVALID_DATA    The data structure passed as input parameter contains invalid data..
    // WFS_ERR_INTERNAL_ERROR  An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_LOCKED          The service is locked under a different hService.
    // WFS_ERR_SOFTWARE_ERROR  The function required access to configuration information, and an error occurred on the software.
    // WFS_ERR_TIMEOUT         The timeout interval expired.
    // WFS_ERR_USER_ERROR      A user is preventing proper operation of the device.
    // WFS_ERR_UNSUPP_DATA     The data structure passed as an input parameter although valid for this service class, is not supported by this service provider or device.

    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_COMMAND    The dwCommand issued is not supported by this service class.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_HWND       The hWnd parameter is not a valid window handle.
    // WFS_ERR_INVALID_POINTER    A pointer parameter does not point to accessible memory.
    // WFS_ERR_UNSUPP_COMMAND     The dwCommand issued, although valid for this service class, is not supported by this service provider.
    return WFS_SUCCESS;
}
/** Отменяет указанный (либо все) асинхронный запрос, выполняемый провайдером, прежде, чем он завершится.
    Все запросы, котрые не успели выпонится к этому времени, завершатся с кодом `WFS_ERR_CANCELED`.

    Отмена запросов не поддерживается.
@param hService
@param ReqID Идентификатор запроса для отмены или `NULL`, если необходимо отменить все запросы
       для указанного сервися `hService`.
*/
HRESULT  WINAPI WFPCancelAsyncRequest(HSERVICE hService, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST //The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR //An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE //The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_REQ_ID //The RequestID parameter does not correspond to an outstanding request on the service.
    return WFS_ERR_UNSUPP_COMMAND;
}
HRESULT  WINAPI WFPSetTraceLevel(HSERVICE hService, DWORD dwTraceLevel) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST    The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR     An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE   The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_TRACELEVEL The dwTraceLevel parameter does not correspond to a valid trace level or set of levels.
    // WFS_ERR_NOT_STARTED        The application has not previously performed a successful WFSStartUp.
    // WFS_ERR_OP_IN_PROGRESS     A blocking operation is in progress on the thread; only WFSCancelBlockingCall and WFSIsBlocking are permitted at this time.
    return WFS_SUCCESS;
}
/** Вызывается XFS для поределения того, можно ли выгрузить DLL с данным сервис-провайдером прямо сейчас. */
HRESULT  WINAPI WFPUnloadService() {
    // Возможные коды завершения функции:
    // WFS_ERR_NOT_OK_TO_UNLOAD
    //     The XFS Manager may not unload the service provider DLL at this time. It will repeat this
    //     request to the service provider until the return is WFS_SUCCESS, or until a new session is
    //     started by an application with this service provider.

    // Нас всегда можно выгрузить.
    return WFS_SUCCESS;
}