#include "Service.h"

#include "XFS/Memory.h"
#include "Manager.h"
#include "PCSC/MediaStatus.h"
#include "PCSC/ReaderState.h"
#include "PCSC/ProtocolTypes.h"
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
        DWORD added = state.dwCurrentState ^ state.dwEventState;
        // Если в указанном бите есть изменения и он был установлен, генерируем событие вставки карты.
        if (added & SCARD_STATE_PRESENT) {
            WFSIDCCARDDATA** result = mService.wrap(translate(state), mFlags);
            // Уведомляем поставщика задачи, что она выполнена.
            XFS::Result(ReqID, serviceHandle(), WFS_SUCCESS).data(result).send(hWnd, WFS_EXECUTE_COMPLETE);
            // Задача обработана, можно удалять из списка.
            return true;
        }
        return false;
    }
private:
    static WFSIDCCARDDATA* translate(const SCARD_READERSTATE& state) {
        //TODO: Возможно, необходимо выделять память через WFSAllocateMore
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Service::Service(Manager& pcsc, HSERVICE hService, const Settings& settings)
    : pcsc(pcsc)
    , hService(hService)
    , hCard(0)
    , dwActiveProtocol(0)
    , mSettings(settings)
{
    std::string json = "[PCSC] Settings: " + settings.toJSONString();
    WFMOutputTraceData((LPSTR)json.c_str());
    // Сразу пытаемся открыть карту, вдруг она уже в считывателе.
    // Это нужно для того, чтобы функция getStatus корректно возвращали признак
    // наличия карточки.
    open();
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
void Service::notify(const SCARD_READERSTATE& state) {
    DWORD added = (state.dwCurrentState ^ state.dwEventState) & state.dwEventState;
    /*if (added & SCARD_STATE_) {
        EventNotifier::notify(WFS_SYSTEM_EVENT, PCSC::DeviceDetected(*this, state));
    }*/
    if (added & SCARD_STATE_EMPTY) {
        EventNotifier::notify(WFS_SERVICE_EVENT, PCSC::CardRemoved(*this));
        if (hCard != 0) {
            close();
        }
    }
    if (added & SCARD_STATE_PRESENT) {
        EventNotifier::notify(WFS_EXECUTE_EVENT, PCSC::CardInserted(*this));
        open();
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
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
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
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
    WFSIDCCAPS* lpCaps = XFS::alloc<WFSIDCCAPS>();

    // Получаем поддерживаемые картой протоколы.
    PCSC::ProtocolTypes types;
    DWORD len = sizeof(DWORD);
    PCSC::Status st = SCARD_S_SUCCESS;
    if (hCard != 0) {
        st = SCardGetAttrib(hCard, SCARD_ATTR_PROTOCOL_TYPES, (BYTE*)&types, &len);
        log("SCardStatus", st);
    }
    bool hasCard = hCard != 0 && st;
    // Устройство является считывателем карт.
    lpCaps->wClass = WFS_SERVICE_CLASS_IDC;
    // Карта вставляется рукой и может быть вытащена в любой момент.
    lpCaps->fwType = WFS_IDC_TYPEDIP;
    // Устройство содержит только считыватель карт.
    lpCaps->bCompound = FALSE;
    // Какие треки могут быть прочитаны -- никакие, только чип.
    // Так как Kalignite не желает работать, если считыватель не умеет читать хоть какой-то
    // трек, то сообщаем, что умеем читать самый востребованный, чтобы удовлетворить Kaliginte.
    lpCaps->fwReadTracks = mSettings.track2.report ? WFS_IDC_TRACK2 : WFS_IDC_NOTSUPP;
    // Какие треки могут быть записаны -- никакие, только чип.
    lpCaps->fwWriteTracks = WFS_IDC_NOTSUPP;
    // Виды поддерживаемых устройством протоколов -- все возможные.
    lpCaps->fwChipProtocols = hasCard ? types.translate() : (WFS_IDC_CHIPT0 | WFS_IDC_CHIPT1);
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
    // В считывателе нет карты, начинаем ожидание, пока вставят. В противном случае
    // информацию можно послать сразу.
    if (hCard == 0) {
        namespace bc = boost::chrono;
        bc::steady_clock::time_point now = bc::steady_clock::now();
        pcsc.addTask(Task::Ptr(new CardReadTask(now + bc::milliseconds(dwTimeOut), *this, hWnd, ReqID, forRead)));
    } else {
        WFSIDCCARDDATA** result = wrap(readChip(), forRead);
        // Уведомляем поставщика задачи, что она выполнена.
        XFS::Result(ReqID, handle(), WFS_SUCCESS).data(result).send(hWnd, WFS_EXECUTE_COMPLETE);
    }
}
WFSIDCCARDDATA* Service::readChip() const {
    assert(hCard != 0 && "Attempt read ATR when card not in the reader");
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
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

    return data;
}
WFSIDCCARDDATA* Service::readTrack2() const {
    assert(hCard != 0 && "Attempt read TRACK2 when card not in the reader");
    assert(mSettings.track2.report == true && "Attempt read TRACK2 when setting Track2.Report is false");

    std::size_t size = mSettings.track2.value.size();
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
    WFSIDCCARDDATA* data = XFS::alloc<WFSIDCCARDDATA>();
    data->wDataSource  = WFS_IDC_TRACK2;
    data->wStatus      = size != 0 ? WFS_IDC_DATAOK : WFS_IDC_DATAMISSING;
    if (size != 0) {
        data->ulDataLength = size;
        data->lpbData      = XFS::allocArr<BYTE>(size);
        std::memcpy(data->lpbData, mSettings.track2.value.c_str(), size);
    }
    return data;
}

WFSIDCCARDDATA** Service::wrap(WFSIDCCARDDATA* iccData, XFS::ReadFlags forRead) const {
    assert((forRead.value() & WFS_IDC_CHIP) && "Service::wrap: Chip data not requested");
    // Снимаем флаг, который мы обрабатываем отдельно.
    // XFS::ReadFlags flags = XFS::ReadFlags(forRead.value() & ~WFS_IDC_CHIP);
    // Данный вызов вернет заполненный нулями массив под два указателя на WFSIDCCARDDATA.
    // В первом элементе будет наш результат, во всех последующих -- прочие запрошенные
    // данные (пустые), в поледнем NULL -- признак конца массива.
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
    WFSIDCCARDDATA** result = XFS::allocArr<WFSIDCCARDDATA*>(forRead.size() + 1);
    // result[0] = iccData;

    std::size_t j = 0;
    for (std::size_t i = 0; i < sizeof(XFS::ReadFlags::type)*8; ++i) {
        XFS::ReadFlags::type flag = (1 << i);
        if (forRead.value() & flag) {
            if (flag == WFS_IDC_CHIP) {
                result[j] = iccData;
            } else
            // Kalignite требует, чтобы track2 мог читаться устройством, иначе он падает.
            // Поэтому, если такая информация запрошена и у нас в настройках сказано ее отдать,
            // то эмулируем ее наличие.
            if (flag == WFS_IDC_TRACK2 && mSettings.track2.report) {
                result[j] = readTrack2();
            } else {
                //TODO: Возможно, необходимо выделять память через WFSAllocateMore
                WFSIDCCARDDATA* data = XFS::alloc<WFSIDCCARDDATA>();
                data->wDataSource = flag;
                data->wStatus = WFS_IDC_DATASRCNOTSUPP;
                result[j] = data;
            }
            ++j;
        }
    }
    return result;
}
std::pair<WFSIDCCHIPIO*, PCSC::Status> Service::transmit(WFSIDCCHIPIO* input) const {
    assert(input != NULL && "Service::transmit: No input from XFS subsystem");
    assert(hCard != 0 && "Service::transmit: No card in reader");

    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
    WFSIDCCHIPIO* result = XFS::alloc<WFSIDCCHIPIO>();
    result->wChipProtocol = input->wChipProtocol;

    std::size_t inputSize = input->ulChipDataLength;
    if (mSettings.workarounds.correctChipIO && input->wChipProtocol == WFS_IDC_CHIPT0) {
        // Команду получения результата Kalignite передает правильно, без ненужного довеска.
        // Эта комана состоит всего из 4 байт, т.е. даже не содержит поля со своей длиной.
        // Так как он в принципе формирует данную команду, непонятно, зачем же он для других
        // команд передает лишний байт.
        if (inputSize > sizeof(SCARD_T0_COMMAND)) {
            const SCARD_T0_COMMAND* cmd = (SCARD_T0_COMMAND*)input->lpbChipData;
            // bP3 содержит размер передаваемых чипу данных
            inputSize = sizeof(SCARD_T0_COMMAND) + cmd->bP3;
        }
    }
    // 2 байта на код ответа, остальное -- на сам ответ чипа.
    result->ulChipDataLength = 256 + 2;
    // TODO: Сколько памяти выделять под буфер? Для протокола T0 нужно минимум 2 под код ответа.
    result->lpbChipData = XFS::allocArr<BYTE>(result->ulChipDataLength);
    //TODO: Убедится в выравнивании! Необходимо выравнивание на двойное слово!
    SCARD_IO_REQUEST ioRq = {input->wChipProtocol, sizeof(SCARD_IO_REQUEST)};
    PCSC::Status st = SCardTransmit(hCard,
        &ioRq, input->lpbChipData, inputSize,
        NULL, result->lpbChipData, &result->ulChipDataLength
    );
    log("SCardTransmit", st);

    return std::make_pair(result, st);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Service::log(std::string operation, PCSC::Status st) const {
    std::stringstream ss;
    ss << std::string("[PCSC='") << mSettings.readerName << "'] " << operation << ": " << st;
    WFMOutputTraceData((LPSTR)ss.str().c_str());
}