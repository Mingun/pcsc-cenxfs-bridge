
// CEN/XFS API -- Наш файл, так как оригинальный предназначен для использования
// клиентами, а нам надо экспортировать функции, а не импортировать их.
#include "xfsspi.h"
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>
// Для std::pair
#include <utility>

#include "PCSCStatus.h"
#include "XFSResult.h"
#include "PCSC.h"

// Линкуемся с библиотекой реализации стандарта PC/SC в Windows
#pragma comment(lib, "winscard.lib")
// Линкуемся с библиотекой реализации XFS
#pragma comment(lib, "msxfs.lib")
// Линкуемся с библитекой для использования оконных функций (PostMessage)
#pragma comment(lib, "user32.lib")

#define MAKE_VERSION(major, minor) (((major) << 8) | (minor))


/** При загрузке DLL будут вызваны конструкторы всех глобальных объектов и установится
    соединение с подсистемой PC/SC. При выгрузке DLL вызовутся деструкторы глобальных
    объектов и соединение автоматически закроется.
*/
PCSC pcsc;
extern "C" {
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
HRESULT SPI_API WFPOpen(HSERVICE hService, LPSTR lpszLogicalName, 
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
HRESULT SPI_API WFPClose(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
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
HRESULT SPI_API WFPRegister(HSERVICE hService,  DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    // Регистрируем событие для окна.
    if (!pcsc.addSubscriber(hService, hWndReg, dwEventClass)) {
        // Если сервиса нет в PC/SC, то он потерян.
        return WFS_ERR_INVALID_HSERVICE;
    }
    // Добавление подписчика всегда успешно.
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
HRESULT SPI_API WFPDeregister(HSERVICE hService, DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    // Отписываемся от событий. Если никого не было удалено, то никто не был зарегистрирован.
    if (!pcsc.removeSubscriber(hService, hWndReg, dwEventClass) {
        // Если сервиса нет в PC/SC, то он потерян.
        return WFS_ERR_INVALID_HSERVICE;
    }
    // Удаление подписчика всегда успешно.
    Result(ReqID, hService, WFS_SUCCESS).send(hWnd, WFS_REGISTER_COMPLETE);
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

@message WFS_LOCK_COMPLETE

@param hService Сервис, к которому нужно получить эксклюзивный доступ.
@param dwTimeOut Таймаут (в мс), в течении которого необходимо получить доступ. `WFS_INDEFINITE_WAIT`,
       если таймаут не требуется.
@param hWnd Окно, которое должно получить сообщение о завершении асинхронной операции.
@param ReqID Идентификатора запроса, который нужно передать окну `hWnd` при завершении операции.
*/
HRESULT SPI_API WFPLock(HSERVICE hService, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;

    Status st = pcsc.get(hService).lock();
    Result(ReqID, hService, st).send(hWnd, WFS_LOCK_COMPLETE);

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
HRESULT SPI_API WFPUnlock(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;

    Status st = pcsc.get(hService).unlock();
    Result(ReqID, hService, st).send(hWnd, WFS_UNLOCK_COMPLETE);
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
HRESULT SPI_API WFPGetInfo(HSERVICE hService, DWORD dwCategory, LPVOID lpQueryDetails, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_CONNECTION_LOST;
    // Для IDC могут запрашиваться только эти константы (WFS_INF_IDC_*)
    switch (dwCategory) {
        case WFS_INF_IDC_STATUS: {      // Дополнительных параметров нет
            std::pair<WFSIDCSTATUS*, Status> status = pcsc.get(hService).getStatus();
            Result(ReqID, hService, status.second).data(status.first).send(hWnd, WFS_GETINFO_COMPLETE);
            break;
        }
        case WFS_INF_IDC_CAPABILITIES: {// Дополнительных параметров нет
            std::pair<WFSIDCCAPS*, Status> caps = pcsc.get(hService).getCaps();
            Result(ReqID, hService, caps.second).data(caps.first).send(hWnd, WFS_GETINFO_COMPLETE);
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
HRESULT SPI_API WFPExecute(HSERVICE hService, DWORD dwCommand, LPVOID lpCmdData, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;

    switch (dwCommand) {
        // Ожидание вставки карты с указанным таймаутом, немедленное чтение треков согласно форме,
        // переданой в парметре. Так как мы не умеем читать треки, то команда не поддерживается.
        case WFS_CMD_IDC_READ_TRACK: {
            //LPSTR formName = (LPSTR)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Аналогично чтению треков.
        case WFS_CMD_IDC_WRITE_TRACK: {
            //WFSIDCWRITETRACK* input = (WFSIDCWRITETRACK*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Команда говорит считывателю вернуть карту. Так как наш считыватель не умеет сам что-либо
        // делать с картой, эту команду мы не поддерживаем.
        case WFS_CMD_IDC_EJECT_CARD: {// Входных параметров нет.
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Команда на захват карты считывателем. Аналогично предудущей команде.
        case WFS_CMD_IDC_RETAIN_CARD: {// Входных параметров нет.
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Команда на сброс счетчика захваченных карт. Так как мы их не звахватываем, то не поддерживаем.
        case WFS_CMD_IDC_RESET_COUNT: {// Входных параметров нет.
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Устанавливает DES-ключ, необходимый для работы модуля CIM86. Не поддерживаем,
        // т.к. у нас нет такого модуля.
        case WFS_CMD_IDC_SETKEY: {
            //WFSIDCSETKEY* = (WFSIDCSETKEY*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Подключаем чип, сбрасываем его и читаем ATR (Answer To Reset).
        // Также данная команда может быть использована для "холодного" сброса чипа.
        // Эта команда не должна использоваться для постоянно присоединенного чипа.
        //TODO: Чип постоянно подсоединен или нет?
        case WFS_CMD_IDC_READ_RAW_DATA: {
            if (lpCmdData == NULL) {
                return WFS_ERR_INVALID_POINTER;
            }
            // Битовая маска с данными, которые должны быть прочитаны.
            WORD lpwReadData = *((WORD*)lpCmdData);
            if (lpwReadData & WFS_IDC_CHIP) {
                std::pair<WFSIDCCARDDATA*, Status> result = pcsc.get(hService).read();
                Result(ReqID, hService, result.second).data(result.first).send(hWnd, WFS_EXECUTE_COMPLETE);
                return WFS_SUCCESS;
            }
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Ждет указанное время, пока не вставят карточку, а потом записывает данные на указанный трек.
        // Не поддерживаем, т.к. не умеем писать треки.
        case WFS_CMD_IDC_WRITE_RAW_DATA: {
            //WFSIDCCARDDATA* data = (WFSIDCCARDDATA*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        // Посылает данные чипу и полуучает от него ответ. Данные прозрачны для провайдера.
        case WFS_CMD_IDC_CHIP_IO: {
            if (lpCmdData == NULL) {
                return WFS_ERR_INVALID_POINTER;
            }
            WFSIDCCHIPIO* data = (WFSIDCCHIPIO*)lpCmdData;
            std::pair<WFSIDCCHIPIO*, Status> result = pcsc.get(hService).transmit(data);
            Result(ReqID, hService, result.second).data(result.first).send(hWnd, WFS_EXECUTE_COMPLETE);
            return WFS_SUCCESS;
        }
        // Отключает питание чипа.
        case WFS_CMD_IDC_RESET: {
            if (lpCmdData == NULL) {
                return WFS_ERR_INVALID_POINTER;
            }
            // Битовая маска 
            WORD wResetIn = *((WORD*)lpCmdData);
            //TODO: Реализовать команду
            return WFS_ERR_INTERNAL_ERROR;
        }
        case WFS_CMD_IDC_CHIP_POWER: {
            if (lpCmdData == NULL) {
                return WFS_ERR_INVALID_POINTER;
            }
            WORD wChipPower = *((WORD*)lpCmdData);
            //TODO: Реализовать команду
            return WFS_ERR_INTERNAL_ERROR;
        }
        // Разбирает результат, ранее возвращенный командой WFS_CMD_IDC_READ_RAW_DATA. Так как мы ее
        // не поддерживаем, то и эту команду мы не поддерживаем.
        case WFS_CMD_IDC_PARSE_DATA: {
            // WFSIDCPARSEDATA* parseData = (WFSIDCPARSEDATA*)lpCmdData;
            return WFS_ERR_UNSUPP_COMMAND;
        }
        default: {
            // Все остальные команды недопустимы.
            return WFS_ERR_INVALID_COMMAND;
        }
    }// switch (dwCommand)
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
    return WFS_ERR_INTERNAL_ERROR;
}
/** Отменяет указанный (либо все) асинхронный запрос, выполняемый провайдером, прежде, чем он завершится.
    Все запросы, котрые не успели выпонится к этому времени, завершатся с кодом `WFS_ERR_CANCELED`.

    Отмена запросов не поддерживается.
@param hService
@param ReqID Идентификатор запроса для отмены или `NULL`, если необходимо отменить все запросы
       для указанного сервися `hService`.
*/
HRESULT SPI_API WFPCancelAsyncRequest(HSERVICE hService, REQUESTID ReqID) {
    if (!pcsc.isValid(hService))
        return WFS_ERR_INVALID_HSERVICE;
    // Возможные коды завершения функции:
    // WFS_ERR_CONNECTION_LOST //The connection to the service is lost.
    // WFS_ERR_INTERNAL_ERROR //An internal inconsistency or other unexpected error occurred in the XFS subsystem.
    // WFS_ERR_INVALID_HSERVICE //The hService parameter is not a valid service handle.
    // WFS_ERR_INVALID_REQ_ID //The RequestID parameter does not correspond to an outstanding request on the service.
    return WFS_ERR_UNSUPP_COMMAND;
}
HRESULT SPI_API WFPSetTraceLevel(HSERVICE hService, DWORD dwTraceLevel) {
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
/** Вызывается XFS для определения того, можно ли выгрузить DLL с данным сервис-провайдером прямо сейчас. */
HRESULT SPI_API WFPUnloadService() {
    // Возможные коды завершения функции:
    // WFS_ERR_NOT_OK_TO_UNLOAD
    //     The XFS Manager may not unload the service provider DLL at this time. It will repeat this
    //     request to the service provider until the return is WFS_SUCCESS, or until a new session is
    //     started by an application with this service provider.

    // Нас всегда можно выгрузить.
    return WFS_SUCCESS;
}
} // extern "C"