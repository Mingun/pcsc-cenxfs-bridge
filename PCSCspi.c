
// CEN/XFS API
#include <xfsspi.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

class Card {
    /// Хендл карты, с которой будет производиться работа.
    SCARDHANDLE hCard;
    /// Протокол, по которому работает карта.
    DWORD dwActiveProtocol;

    // Данный класс будет создавать объекты данного класса, вызывая конструктор.
    friend class PCSC;
private:
    /** Открывает указанную карточку для работы.
    @param hContext Ресурсный менеджер подсистемы PC/SC.
    @param readerName
    */
    Card(SCARDCONTEXT hContext, const char* readerName) {
        LONG r = SCardConnect(hContext, readerName, SCARD_SHARE_SHARED,
            // У нас нет предпочитаемого протокола, работаеем с тем, что дают
            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
            // Получаем хендл карты и выбранный протокол.
            &hCard, &dwActiveProtocol
        );
    }
public:
    ~Card() {
        // При закрытии соединения ничего не делаем с карточкой, оставляем ее в считывателе.
        LONG r = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
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
        LONG r = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);

    }
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~PCSC() {
        for (typename CardMap::const_iterator it = cards.begin(); it != cards.end(); ++it) {
            delete *it;
        }
        cards.clear();
        LONG r = SCardReleaseContext(hContext);
    }
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    bool isValid(HSERVICE hService) {
        return cards.find(hService) != cards.end();
    }
    Card& open(HSERVICE hService, const char* name) {
        Card* card = new Card(hContext, name);
        cards.insert(std::make_pair(hService, card));
        return *card;
    }
    Card& get(HSERVICE hService) {
        assert(isValid(hService));
        return *cards.find(hService);
    }
    void close(HSERVICE hService) {
        assert(isValid(hService));
        typename CardMap::iterator it = cards.find(hService);
        delete *it;
        cards.erase(it);
    }
public:
    static SCARDHANDLE translate(HSERVICE hService) {
        // TODO: Сделать преобразование хендла сервиса XFS в хендл карты PC/SC.
        SCARDHANDLE hCard = {0};
        return hCard;
    }
    /** Конвернирует код возврата функций PC/SC в код возврата функций XFS.
    @param rsCode Код завершения API-функции PC/SC.
    */
    static int translate(LONG rsCode) {
        switch (rsCode) {
            // Успешно
            case SCARD_S_SUCCESS:           return WFS_SUCCESS;
            // Некорректный хендл карты (hCard)
            case SCARD_E_INVALID_HANDLE:    return WFS_ERR_INVALID_HSERVICE;
            // Ресурсный менеджер подсистемы PC/SC не запущен
            case SCARD_E_NO_SERVICE:        return WFS_ERR_INTERNAL_ERROR;
            // Считыватель был отключен
            case SCARD_E_READER_UNAVAILABLE:return WFS_ERR_CONNECTION_LOST;
            // Кто-то эксклюзивно владеет картой
            case SCARD_E_SHARING_VIOLATION: return WFS_ERR_LOCKED;
            // Внутренняя ошибка взаимодействия с устройством
            case SCARD_F_COMM_ERROR:        return WFS_ERR_HARDWARE_ERROR;
        }
        // TODO: Уточнить тип ошибки, возвращаемой в случае, если трансляция кодов ошибок не удалась.
        return WFS_ERR_INTERNAL_ERROR;
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
                        dwTraceLevel, DWORD dwTimeOut, 
                        HWND hWnd, REQUESTID ReqID,
                        HPROVIDER hProvider, 
                        DWORD dwSPIVersionsRequired, LPWFSVERSION lpSPIVersion, 
                        DWORD dwSrvcVersionsRequired, LPWFSVERSION lpSrvcVersion
) {
    // Получаем хендл карты, с которой будем работать.
    pcsc.open(hService, lpszLogicalName);

    if (lpSPIVersion != NULL) {
        lpSPIVersion->wVersion
    }
    WFSRESULT* pResult;
    WFMAllocateBuffer(sizeof(WFSRESULT), ulMemFlags, &pResult);
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

    //TODO: Послать окну hWnd сообщение с кодом WFS_OPEN_COMPLETE и wParam = WFSRESULT;
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
@param dwEventClass Логический OR классов сообщений, которые не нужно мониторить. 0 означает, что
       производится подписка на все классы событий.
@param hWndReg Окно, которое будет получать события указанных классов.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT  WINAPI WFPRegister(HSERVICE hService,  DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
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
    LONG r = SCardBeginTransaction(translate(hService));
    //TODO: Послать окну hWnd сообщение с кодом WFS_CLOSE_COMPLETE и wParam = WFSRESULT;

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

    SCARDCONTEXT hContext = {0};//TODO: Достать контекст
    // Заканчиваем транзакцию, ничего не делаем с картой.
    LONG r = SCardEndTransaction(hContext, SCARD_LEAVE_CARD);
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
    // Для IDC могут запрашиваться только эти константы (WFS_INF_IDC_*)
    switch (dwCategory) {
        case WFS_INF_IDC_STATUS: {// Дополнительных параметров нет
            LONG r = SCardStatus(translate(hService));

            LPWFSIDCSTATUS st;
            // Статус устройства (WFS_IDC_*)
            st.fwDevice
            break;
        }
        case WFS_INF_IDC_CAPABILITIES: {
            LPWFSIDCCAPS st = (LPWFSIDCCAPS)lpQueryDetails;
            break;
        }
        case WFS_INF_IDC_FORM_LIST:
        case WFS_INF_IDC_QUERY_FORM: {
            // Формы не поддерживаем. Что это за формы такие -- непонятно.
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
    LONG r = SCardTransmit(translate(hService), );
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
@param RequestID Идентификатор запроса для отмены или `NULL`, если необходимо отменить все запросы
       для указанного сервися `hService`.
*/
HRESULT  WINAPI WFPCancelAsyncRequest(HSERVICE hService, REQUESTID RequestID) {
    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST //The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR //An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE //The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_REQ_ID //The RequestID parameter does not correspond to an outstanding request on the service.
    return WFS_ERR_UNSUPP_COMMAND;
}
HRESULT  WINAPI WFPSetTraceLevel(HSERVICE hService, DWORD dwTraceLevel) {
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