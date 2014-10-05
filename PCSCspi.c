
// CEN/XFS API
#include <xfsspi.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

/** Устанавливает сессию с подсистемой PC/SC.
@param lpszLogicalName Имя логическогосервиса, настраивается в реестре HKLM\<user>\XFS\LOGICAL_SERVICES
*/
HRESULT  WINAPI WFPOpen(HSERVICE hService, LPSTR lpszLogicalName, 
                        HAPP hApp, LPSTR lpszAppID, 
                        dwTraceLevel, DWORD dwTimeOut, 
                        HWND hWnd, REQUESTID ReqID, HPROVIDER hProvider, 
                        DWORD dwSPIVersionsRequired, LPWFSVERSION lpSPIVersion, 
                        DWORD dwSrvcVersionsRequired, LPWFSVERSION lpSrvcVersion
) {
    // Создаем контекст.
    SCARDCONTEXT hContext = {0};
    LONG r = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    if (SCARD_S_SUCCESS != r) {
        return WFS_
    }
    // Получаем хендл карты, с которой будем работать.
    SCARDHANDLE hCard = {0};
    // Протокол, выбранный для работы.
    DWORD dwActiveProtocol;
    // Возможно, открывать соединение к карте лучше здесь?
    SCardConnect(hContext, lpszLogicalName, SCARD_SHARE_SHARED,
        // У нас нет предпочитаемого протокола, работаеем с тем, что дают
        SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        // Получаем хендл карты и выбранный протокол.
        &hCard, &dwActiveProtocol
    );
    //TODO: Послать окну hWnd сообщение с кодом WFS_OPEN_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
HRESULT  WINAPI WFPClose(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    SCARDCONTEXT hContext = {0};//TODO: Достать контекст
    LONG r = SCardReleaseContext(hContext);
    //TODO: Послать окну hWnd сообщение с кодом WFS_CLOSE_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
HRESULT  WINAPI WFPRegister(HSERVICE hService,  DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    return WFS_SUCCESS;
}
HRESULT  WINAPI WFPDeregister(HSERVICE hService, DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID) {
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
HRESULT  WINAPI WFPLock(HSERVICE hService, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    SCARDCONTEXT hContext = {0};//TODO: Достать контекст
    LONG r = SCardBeginTransaction(hContext);
    //TODO: Послать окну hWnd сообщение с кодом WFS_CLOSE_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
HRESULT  WINAPI WFPUnlock(HSERVICE hService, HWND hWnd, REQUESTID ReqID) {
    SCARDCONTEXT hContext = {0};//TODO: Достать контекст
    // Заканчиваем транзакцию, ничего не делаем с картой.
    LONG r = SCardEndTransaction(hContext, SCARD_LEAVE_CARD);
    //TODO: Послать окну hWnd сообщение с кодом WFS_CLOSE_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/**
@param pQueryDetails Указатель на дополнительные данные. Если данных нет, то `NULL`.
@param dwTimeOut Таймаут, за который нужно завершить операцию.
@param dwCategory Вид запрашиваемой информации. Поддерживаются только стандартные виды информации.
@param hWnd Окно, которому необходимо послать информацию о завершении операции.
@param ReqID Идентификатор запроса, по которому источник запроса будет отслежиать ответ. Передается окну `hWnd` без изменений.
*/
HRESULT  WINAPI WFPGetInfo(HSERVICE hService, DWORD dwCategory, LPVOID lpQueryDetails, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    // Для IDC могут запрашиваться только эти константы (WFS_INF_IDC_*)
    switch (dwCategory) {
        case WFS_INF_IDC_STATUS: {
            SCARDHANDLE hCard; // TODO: Достать хендл карты
            SCardStatus(hCard);
            // Входного параметра нет. Выходной параметр
            LPWFSIDCSTATUS st = (LPWFSIDCSTATUS)lpQueryDetails;
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
    //TODO: Послать окну hWnd сообщение с кодом WFS_GETINFO_COMPLETE и wParam = WFSRESULT;
    return WFS_SUCCESS;
}
HRESULT  WINAPI WFPExecute(HSERVICE hService, DWORD dwCommand, LPVOID lpCmdData, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    return WFS_SUCCESS;
}
HRESULT  WINAPI WFPCancelAsyncRequest(HSERVICE hService, REQUESTID RequestID) {
    return WFS_SUCCESS;
}
HRESULT  WINAPI WFPSetTraceLevel(HSERVICE hService, DWORD dwTraceLevel) {
    return WFS_SUCCESS;
}
HRESULT  WINAPI WFPUnloadService() {
    return WFS_SUCCESS;
}
