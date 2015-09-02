#include "Service.h"

#include "XFS/Memory.h"
#include "Manager.h"
#include "PCSC/MediaStatus.h"
#include "PCSC/ReaderState.h"
#include "PCSC/Events.h"

#include <string>
#include <sstream>
#include <cassert>
// Для std::memcpy
#include <cstring>
// Для работы с текущим временем, для получения времени дедлайна.
#include <boost/chrono/chrono.hpp>

// CEN/XFS API -- Для WFMOutputTraceData
#include <xfsadmin.h>

class CardReadTask : public Task {
    /// Данные, которые должны быть прочитаны.
    XFS::ReadFlags mFlags;
public:
    CardReadTask(bc::steady_clock::time_point deadline, Service& service, HWND hWnd, REQUESTID ReqID, XFS::ReadFlags flags)
        : Task(deadline, service, hWnd, ReqID), mFlags(flags) {}
    virtual bool match(const SCARD_READERSTATE& state) const {
        if (state.dwEventState & SCARD_STATE_PRESENT) {

            // Подключаемся к карточке. Kalignite после события о том, что карточка вставлена, снова
            // опрашивает состояние устройства (Service::getStatus) и если в нем карты нет (т.е. мы не
            // выполним подключение), то он пытается прочитать треки еще раз, в резултате чего падает с сообщением:
            // KXCardReader: Attempted to re-read an unread track on a swipe or dip card reader.On these devices, tracks can only be read on the initial accept.
            PCSC::Status st = mService.open();

            // Снимаем флаг, который мы обрабатываем отдельно.
            XFS::ReadFlags flags = XFS::ReadFlags(mFlags.value() & ~WFS_IDC_CHIP);
            // Данный вызов вернет заполненный нулями массив под два указателя на WFSIDCCARDDATA.
            // В первом элементе будет наш результат, во всех последующих -- прочие запрошенные
            // данные (пустые), в поледнем NULL -- признак конца массива.
            //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
            WFSIDCCARDDATA** result = XFS::allocArr<WFSIDCCARDDATA*>(flags.size() + 2);
            result[0] = translate(state);

            for (std::size_t i = 0; i < sizeof(XFS::ReadFlags::type)*8; ++i) {
                XFS::ReadFlags::type flag = (1 << i);
                if (flags.value() & flag) {
                    //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
                    WFSIDCCARDDATA* data = XFS::alloc<WFSIDCCARDDATA>();
                    data->wDataSource = flag;
                    data->wStatus = WFS_IDC_DATAMISSING;
                    result[i+1] = data;
                }
            }
            // Уведомляем поставщика задачи, что она выполнена.
            XFS::Result(ReqID, serviceHandle(), WFS_SUCCESS).data(result).send(hWnd, WFS_EXECUTE_COMPLETE);
            // Задача обработана, можно удалять из списка.
            return true;
        }
        return false;
    }
private:
    static WFSIDCCARDDATA* translate(const SCARD_READERSTATE& state) {
        //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
        WFSIDCCARDDATA* data = XFS::alloc<WFSIDCCARDDATA>();
        // data->lpbData содержит ATR (Answer To Reset), прочитанный с чипа
        data->wDataSource = WFS_IDC_CHIP;
        data->wStatus = WFS_IDC_DATAOK;
        data->ulDataLength = state.cbAtr;
        data->lpbData = XFS::allocArr<BYTE>(state.cbAtr);
        std::memcpy(data->lpbData, state.rgbAtr, state.cbAtr);

        return data;
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
Service::Service(Manager& pcsc, HSERVICE hService, const Settings& settings)
    : pcsc(pcsc)
    , hService(hService)
    , hCard(0)
    , dwActiveProtocol(0)
    , mSettings(settings)
{
    std::string json = "Settings: " + settings.toJSONString();
    WFMOutputTraceData((LPSTR)json.c_str());
}
Service::~Service() {
    if (hCard != 0) {
        close();
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
PCSC::Status Service::open() {
    assert(hCard == 0 && "Must open only one card at one service");
    PCSC::Status st = SCardConnect(pcsc.context(), mSettings.readerName.c_str(), SCARD_SHARE_SHARED,
        // У нас нет предпочитаемого протокола, работаем с тем, что дают
        SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        // Получаем хендл карты и выбранный протокол.
        &hCard, &dwActiveProtocol
    );
    log("SCardConnect", st);
    return st;
}
PCSC::Status Service::close() {
    assert(hCard != 0 && "Attempt disconnect from non-connected card");
    // При закрытии соединения ничего не делаем с карточкой, оставляем ее в считывателе.
    PCSC::Status st = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    log("SCardDisconnect", st);
    hCard = 0;
    return st;
}

PCSC::Status Service::lock() {
    PCSC::Status st = SCardBeginTransaction(hCard);
    log("SCardBeginTransaction", st);

    return st;
}
PCSC::Status Service::unlock() {
    // Заканчиваем транзакцию, ничего не делаем с картой.
    PCSC::Status st = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
    log("SCardEndTransaction", st);

    return st;
}
/// Уведомляет всех слушателей обо всех произошедших изменениях со считывателями.
void Service::notify(const SCARD_READERSTATE& state) const {
    /*if (state.dwEventState & SCARD_STATE_) {
        EventNotifier::notify(WFS_SYSTEM_EVENT, PCSC::DeviceDetected(*this, state));
    }*/
    if (state.dwEventState & SCARD_STATE_EMPTY) {
        EventNotifier::notify(WFS_SERVICE_EVENT, PCSC::CardRemoved(*this));
    }
    if (state.dwEventState & SCARD_STATE_PRESENT) {
        EventNotifier::notify(WFS_EXECUTE_EVENT, PCSC::CardInserted(*this));
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::pair<WFSIDCSTATUS*, PCSC::Status> Service::getStatus() {
    // Состояние считывателя.
    PCSC::MediaStatus state;
    DWORD nameLen;
    DWORD atrLen;
    // Если карточки не будет в считывателе, то вернется ошибка и в ответ мы дадим WFS_IDC_MEDIANOTPRESENT
    PCSC::Status st = SCARD_S_SUCCESS;
    if (hCard != 0) {
        st = SCardStatus(hCard,
            // Имя получать не будем, тем не менее длину получить требуется, NULL недопустим.
            NULL, &nameLen,
            // Небольшой хак допустим, у нас прозрачная обертка, ничего лишнего.
            (DWORD*)&state, &dwActiveProtocol,
            // ATR получать не будем, тем не менее длину получить требуется, NULL недопустим.
            NULL, &atrLen
        );
        log("SCardStatus", st);
    }
    bool hasCard = hCard != 0 && st;
    //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
    WFSIDCSTATUS* lpStatus = XFS::alloc<WFSIDCSTATUS>();
    // Набор флагов, определяющих состояние устройства. Наше устройство всегда на связи,
    // т.к. в противном случае при открытии сессии с PC/SC драйвером будет ошибка.
    lpStatus->fwDevice = WFS_IDC_DEVONLINE;
    lpStatus->fwMedia = hasCard ? state.translateMedia() : WFS_IDC_MEDIANOTPRESENT;
    // У считывателей нет корзины для захваченных карт.
    lpStatus->fwRetainBin = WFS_IDC_RETAINNOTSUPP;
    // Модуль безопасноси отсутствует
    lpStatus->fwSecurity = WFS_IDC_SECNOTSUPP;
    // Количество возвращенных карт. Так как карта вставляется и вытаскивается руками,
    // то данный параметр не имеет смысла.
    //TODO Хотя, может быть, можно будет его отслеживать как количество вытащенных карт.
    lpStatus->usCards = 0;
    lpStatus->fwChipPower = hasCard ? state.translateChipPower() : WFS_IDC_CHIPNOCARD;
    //TODO: Добавить lpszExtra со всеми параметрами, полученными от PC/SC.
    return std::make_pair(lpStatus, st);
}
std::pair<WFSIDCCAPS*, PCSC::Status> Service::getCaps() const {
    //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
    WFSIDCCAPS* lpCaps = XFS::alloc<WFSIDCCAPS>();

    // Получаем поддерживаемые картой протоколы.
    DWORD types;
    DWORD len = sizeof(DWORD);
    PCSC::Status st = SCARD_S_SUCCESS;
    if (hCard != 0) {
        st = SCardGetAttrib(hCard, SCARD_ATTR_PROTOCOL_TYPES, (BYTE*)&types, &len);
        log("SCardStatus", st);
    }

    // Устройство является считывателем карт.
    lpCaps->wClass = WFS_SERVICE_CLASS_IDC;
    // Карта вставляется рукой и может быть вытащена в любой момент.
    lpCaps->fwType = WFS_IDC_TYPEDIP;
    // Устройство содержит только считыватель карт.
    lpCaps->bCompound = FALSE;
    // Какие треки могут быть прочитаны -- никакие, только чип.
    // Так как Kalignite не желает работать, если считыватель не умеет читать хоть какой-то
    // трек, то сообщаем, что умеем читать самый востребованный, чтобы удовлетворить Kaliginte.
    lpCaps->fwReadTracks = mSettings.reportReadTrack2 ? WFS_IDC_TRACK2 : WFS_IDC_NOTSUPP;
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
void Service::asyncRead(DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID, XFS::ReadFlags forRead) {
    namespace bc = boost::chrono;
    bc::steady_clock::time_point now = bc::steady_clock::now();
    pcsc.addTask(Task::Ptr(new CardReadTask(now + bc::milliseconds(dwTimeOut), *this, hWnd, ReqID, forRead)));
}
std::pair<WFSIDCCARDDATA**, PCSC::Status> Service::read() const {
    //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
    WFSIDCCARDDATA* data = XFS::alloc<WFSIDCCARDDATA>();
    // data->lpbData содержит ATR (Answer To Reset), прочитанный с чипа
    data->wDataSource = WFS_IDC_CHIP;
    //TODO: Статус прочитанных данных необходимо выставлять в соответствии со статусом,
    // который вернула SCardGetAttrib.
    data->wStatus = WFS_IDC_DATAOK;
    // Получаем ATR (Answer To Reset). Сначала длину, потом сами данные.
    PCSC::Status st = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, NULL, &data->ulDataLength);
    log("SCardGetAttrib(?, SCARD_ATTR_ATR_STRING, NULL, ?)", st);
    data->lpbData = XFS::allocArr<BYTE>(data->ulDataLength);
    st = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, data->lpbData, &data->ulDataLength);
    log("SCardGetAttrib(?, SCARD_ATTR_ATR_STRING, ?, ?)", st);
    // Данный вызов вернет заполненный нулями массив под два указателя на WFSIDCCARDDATA.
    // В первом элементе будет наш результат, во втором NULL -- признак конца массива.
    //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
    WFSIDCCARDDATA** result = XFS::allocArr<WFSIDCCARDDATA*>(2);
    result[0] = data;
    return std::make_pair(result, st);
}
std::pair<WFSIDCCHIPIO*, PCSC::Status> Service::transmit(WFSIDCCHIPIO* input) const {
    assert(input != NULL && "Service::transmit: No input from XFS subsystem");
    assert(hCard != 0 && "Service::transmit: No card in reader");

    //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
    WFSIDCCHIPIO* result = XFS::alloc<WFSIDCCHIPIO>();
    result->wChipProtocol = input->wChipProtocol;

    SCARD_IO_REQUEST ioRq = {0};//TODO: Получить протокол.
    PCSC::Status st = SCardTransmit(hCard,
        &ioRq, input->lpbChipData, input->ulChipDataLength,
        NULL, result->lpbChipData, &result->ulChipDataLength
    );
    log("SCardTransmit", st);

    return std::make_pair(result, st);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Service::log(std::string operation, PCSC::Status st) const {
    std::stringstream ss;
    ss << operation << " card reader '" << mSettings.readerName << "': " << st;
    WFMOutputTraceData((LPSTR)ss.str().c_str());
}