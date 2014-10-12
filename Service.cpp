#include "Service.h"

#include "Utils.h"
#include "PCSC.h"
#include "PCSCMediaStatus.h"
#include "PCSCReaderState.h"

#include <string>
#include <sstream>
#include <cassert>
// Для работы с текущим временем, для получения времени дедлайна.
#include <boost/chrono/chrono.hpp>

/// Функтор, создающий результат уведомления о вставке карты каждому заинтересованному слушателю.
class CardInserted {
    HSERVICE hService;
public:
    CardInserted(HSERVICE hService) : hService(hService) {}
    Result operator()() const {
        return Result(0, hService, WFS_SUCCESS).cardInserted();
    }
};
/// Функтор, создающий результат уведомления о удалении карты каждому заинтересованному слушателю.
class CardRemoved {
    HSERVICE hService;
public:
    CardRemoved(HSERVICE hService) : hService(hService) {}
    Result operator()() const {
        return Result(0, hService, WFS_SUCCESS).cardRemoved();
    }
};
/// Функтор, создающий результат уведомления о появлении нового устройства каждому заинтересованному слушателю.
class DeviceDetected {
    HSERVICE hService;
    SCARD_READERSTATE& state;
public:
    DeviceDetected(HSERVICE hService, SCARD_READERSTATE& state) : hService(hService), state(state) {}
    Result operator()() const {
        WFSDEVSTATUS* status = xfsAlloc<WFSDEVSTATUS>();
        // Имя физичеcкого устройства, чье состояние изменилось
        strcpy(status->lpszPhysicalName, state.szReader);
        // Рабочая станция, на которой запущен сервис.
        status->lpszWorkstationName = NULL;//TODO: Заполнить имя рабочей станции.
        status->dwState = ReaderState(state.dwEventState).translate();
        return Result(0, hService, WFS_SUCCESS).data(status);
    }
};

/*
class ProtocolTypes {
    DWORD value;
public:
    inline ProtocolTypes(DWORD flags) : value(flags) {}
    inline WORD translate() {
        WORD result = 0;
        if (value & (1 << 0)) {// Нулевой бит -- протокол T0
            result |= WFS_IDC_CHIPT0;
        }
        if (value & (1 << 1)) {// Первый бит -- протокол T1
            result |= WFS_IDC_CHIPT1;
        }
        return result;
    }
};*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Service::Service(PCSC& pcsc, HSERVICE hService, const std::string& readerName)
    : pcsc(pcsc)
    , hService(hService)
    , hCard(0)
    , dwActiveProtocol(0)
    , readerName(readerName)
    {}
Service::~Service() {
    if (hCard != 0) {
        close();
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Status Service::open(SCARDCONTEXT hContext) {
    assert(hCard == 0 && "Must open only one card at one service");
    Status st = SCardConnect(hContext, readerName.c_str(), SCARD_SHARE_SHARED,
        // У нас нет предпочитаемого протокола, работаем с тем, что дают
        SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        // Получаем хендл карты и выбранный протокол.
        &hCard, &dwActiveProtocol
    );
    log("SCardConnect", st);
    return st;
}
Status Service::close() {
    assert(hCard != 0 && "Attempt disconnect from non-connected card");
    // При закрытии соединения ничего не делаем с карточкой, оставляем ее в считывателе.
    Status st = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    log("SCardDisconnect", st);
    hCard = 0;
    return st;
}

Status Service::lock() {
    Status st = SCardBeginTransaction(hCard);
    log("SCardBeginTransaction", st);

    return st;
}
Status Service::unlock() {
    // Заканчиваем транзакцию, ничего не делаем с картой.
    Status st = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
    log("SCardEndTransaction", st);

    return st;
}
/// Уведомляет всех слушателей обо всех произошедших изменениях со считывателями.
void Service::notify(SCARD_READERSTATE& state) const {
    /*if (state.dwEventState & SCARD_STATE_) {
        EventNotifier::notify(WFS_SYSTEM_EVENT, DeviceDetected(hService, state));
    }*/
    if (state.dwEventState & SCARD_STATE_EMPTY) {
        EventNotifier::notify(WFS_SERVICE_EVENT, CardRemoved(hService));
    }
    if (state.dwEventState & SCARD_STATE_PRESENT) {
        EventNotifier::notify(WFS_EXECUTE_EVENT, CardInserted(hService));
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::pair<WFSIDCSTATUS*, Status> Service::getStatus() {
    // Состояние считывателя.
    MediaStatus state;
    DWORD nameLen;
    DWORD atrLen;
    Status st = SCardStatus(hCard,
        // Имя получать не будем, тем не менее длину получить требуется, NULL недопустим.
        NULL, &nameLen,
        // Небольшой хак допустим, у нас прозрачная обертка, ничего лишнего.
        (DWORD*)&state, &dwActiveProtocol,
        // ATR получать не будем, тем не менее длину получить требуется, NULL недопустим.
        NULL, &atrLen
    );
    log("SCardStatus: ", st);

    WFSIDCSTATUS* lpStatus = xfsAlloc<WFSIDCSTATUS>();
    // Набор флагов, определяющих состояние устройства. Наше устройство всегда на связи,
    // т.к. в противном случае при открытии сесии с PC/SC драйвером будет ошибка.
    lpStatus->fwDevice = WFS_IDC_DEVONLINE;
    lpStatus->fwMedia = st ? state.translateMedia() : WFS_IDC_MEDIANOTPRESENT;
    // У считывателей нет корзины для захваченных карт.
    lpStatus->fwRetainBin = WFS_IDC_RETAINNOTSUPP;
    // Модуль безопасноси отсутствует
    lpStatus->fwSecurity = WFS_IDC_SECNOTSUPP;
    // Количество возвращенных карт. Так как карта вставляется и вытаскивается руками,
    // то данный параметр не имеет смысла.
    //TODO Хотя, может быть, можно будет его отслеживать как количество вытащенных карт.
    lpStatus->usCards = 0;
    lpStatus->fwChipPower = st ? state.translateChipPower() : WFS_IDC_CHIPNOCARD;
    //TODO: Добавить lpszExtra со всеми параметрами, полученными от PC/SC.
    return std::make_pair(lpStatus, st);
}
std::pair<WFSIDCCAPS*, Status> Service::getCaps() const {
    WFSIDCCAPS* lpCaps = xfsAlloc<WFSIDCCAPS>();

    // Получаем поддерживаемые картой протоколы.
    DWORD types;
    DWORD len = sizeof(DWORD);
    Status st = SCardGetAttrib(hCard, SCARD_ATTR_PROTOCOL_TYPES, (BYTE*)&types, &len);
    log("SCardGetAttrib(SCARD_ATTR_PROTOCOL_TYPES)", st);

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
    // Виды поддерживаемых устройством протоколов -- все возможные.
    lpCaps->fwChipProtocols = WFS_IDC_CHIPT0 | WFS_IDC_CHIPT1;
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
    //TODO: Получить реальные возможности считывателя. Пока предполагаем, что все возможности есть.
    lpCaps->fwChipPower = WFS_IDC_CHIPPOWERCOLD | WFS_IDC_CHIPPOWERWARM | WFS_IDC_CHIPPOWEROFF;
    //TODO: Добавить lpszExtra со всеми параметрами, полученными от PC/SC.
    return std::make_pair(lpCaps, st);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Service::asyncRead(DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    namespace bc = boost::chrono;
    bc::steady_clock::time_point now = bc::steady_clock::now();
    pcsc.addTask(Task(now + bc::milliseconds(dwTimeOut), hService, hWnd, ReqID));
}
std::pair<WFSIDCCARDDATA*, Status> Service::read() const {
    WFSIDCCARDDATA* data = xfsAlloc<WFSIDCCARDDATA>();
    // data->lpbData содержит ATR (Answer To Reset), прочитанный с чипа
    data->wDataSource = WFS_IDC_CHIP;
    //TODO: Статус прочитанных данных необходимо выставлять в соответствии со статусом,
    // который вернула SCardGetAttrib.
    data->wStatus = WFS_IDC_DATAOK;
    // Получаем ATR (Answer To Reset).
    Status st = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, data->lpbData, &data->ulDataLength);
    return std::make_pair(data, st);
}
std::pair<WFSIDCCHIPIO*, Status> Service::transmit(WFSIDCCHIPIO* input) const {
    assert(input != NULL);

    WFSIDCCHIPIO* result = xfsAlloc<WFSIDCCHIPIO>();
    result->wChipProtocol = input->wChipProtocol;

    SCARD_IO_REQUEST ioRq = {0};//TODO: Получить протокол.
    Status st = SCardTransmit(hCard,
        &ioRq, input->lpbChipData, input->ulChipDataLength,
        NULL, result->lpbChipData, &result->ulChipDataLength
    );
    log("SCardTransmit", st);

    return std::make_pair(result, st);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Service::log(std::string operation, Status st) const {
    std::stringstream ss;
    ss << operation << " card reader '" << readerName << "': " << st;
    WFMOutputTraceData((LPSTR)ss.str().c_str());
}