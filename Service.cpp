#include "Service.h"

#include "Manager.h"

#include "PCSC/Events.h"
#include "PCSC/MediaStatus.h"
#include "PCSC/ProtocolTypes.h"
#include "PCSC/ReaderState.h"

#include "XFS/Logger.h"
#include "XFS/Memory.h"

#include <cassert>
// Для std::memcpy
#include <cstring>
#include <sstream>
#include <string>
// Для работы с текущим временем, для получения времени дедлайна.
#include <boost/chrono/chrono.hpp>

class Hex {
    const char* mBegin;
    const char* mEnd;
private:
    friend inline XFS::Logger& operator<<(XFS::Logger& os, const Hex& h) {
        os << std::hex << std::setfill('0');
        for (const char* it = h.mBegin; it < h.mEnd; ++it) {
            unsigned int value = *it & 0xFF;
            os << std::setw(2) << value << ' ';
        }
        return os;
    }
public:
    Hex(LPBYTE begin, ULONG count) : mBegin((const char*)begin), mEnd((const char*)begin + count) {}
};

class CardReadTask : public Task {
    /// Данные, которые должны быть прочитаны.
    XFS::ReadFlags mFlags;
public:
    CardReadTask(bc::steady_clock::time_point deadline, Service& service,
                HWND hWnd, REQUESTID ReqID, XFS::ReadFlags flags
    ) : Task(deadline, service, hWnd, ReqID), mFlags(flags) {
        XFS::Logger() << "Service " << service.handle() << ": Listen reader(s), read flags: " << flags;
    }
    virtual bool match(const SCARD_READERSTATE& state, bool deviceChange) const {
        // Если изменения нас не интересуют, выходим.
        if (!mService.match(state, deviceChange)) {
            return false;
        }
        DWORD added = (state.dwCurrentState ^ state.dwEventState) & state.dwEventState;
        // Если в указанном бите есть изменения и он был установлен, генерируем событие вставки карты.
        if (added & SCARD_STATE_PRESENT) {
            {XFS::Logger() << "Service " << mService.handle() << ": Card inserted to reader '" << state.szReader << "', read flags: " << mFlags; }

            WFSIDCCARDDATA** result = mService.wrap(translate(state), mFlags);
            // Уведомляем поставщика задачи, что она выполнена.
            XFS::Result(ReqID, serviceHandle(), WFS_SUCCESS).attach(result).send(hWnd, WFS_EXECUTE_COMPLETE);
            // Задача обработана, можно удалять из списка.
            return true;
        }
        return false;
    }
private:
    WFSIDCCARDDATA* translate(const SCARD_READERSTATE& state) const {
        //TODO: Возможно, необходимо выделять память через WFSAllocateMore
        WFSIDCCARDDATA* data = XFS::alloc<WFSIDCCARDDATA>();
        // data->lpbData содержит ATR (Answer To Reset), прочитанный с чипа
        data->wDataSource = WFS_IDC_CHIP;
        data->wStatus = WFS_IDC_DATAOK;
        data->ulDataLength = state.cbAtr;
        data->lpbData = XFS::allocArr<BYTE>(state.cbAtr);
        std::memcpy(data->lpbData, state.rgbAtr, state.cbAtr);
        {XFS::Logger() << "Service " << mService.handle() << ": ATR=" << Hex(data->lpbData, data->ulDataLength);}
        return data;
    }
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Service::Service(Manager& pcsc, HSERVICE hService, const Settings& settings)
    : pcsc(pcsc)
    , hService(hService)
    , hCard(0)
    , mActiveProtocol(0)
    , mBindedReaderName(settings.readerName)
    , mSettings(settings)
    , mInited(false)
{
}
Service::~Service() {
    if (hCard != 0) {
        close();
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
PCSC::Status Service::open(const char* readerName) {
    assert(hCard == 0 && "Must open only one card at one service");
    PCSC::Status st = SCardConnect(pcsc.context(), readerName,
        mSettings.exclusive ? SCARD_SHARE_EXCLUSIVE : SCARD_SHARE_SHARED,
        // У нас нет предпочитаемого протокола, работаем с тем, что дают
        SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        // Получаем хендл карты и выбранный протокол.
        &hCard, (DWORD*)&mActiveProtocol
    );
    {
        XFS::Logger()
            << "SCardConnect(hContext=" << pcsc.context()
            << ", szReader=" << readerName << ", ..., hCard=&" << hCard
            << ", dwActiveProtocol=&" << mActiveProtocol << ") = " << st;
    }
    if (st) {
        // Если открытие совершилось корректно, то запоминаем имя текущего считывателя.
        mBindedReaderName = readerName;
        XFS::Logger() << "Service " << handle() << " binded to reader '" << mBindedReaderName << "'";
    }
    return st;
}
PCSC::Status Service::close() {
    assert(hCard != 0 && "Attempt disconnect from non-connected card");
    // При закрытии соединения ничего не делаем с карточкой, оставляем ее в считывателе.
    PCSC::Status st = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    {XFS::Logger() << "SCardDisconnect(hCard=" << hCard << ", SCARD_LEAVE_CARD) = " << st; }
    hCard = 0;
    // Сбрасываем привязку на привязку из настроек. Таким образом, если в настойках
    // не указано конкретного считывателя, то прявязка будет пустая и сервис привяжется
    // к первому считывателю, в котором он обнаружит карточку. Если же конкретный считыватель
    // будет указан, то сервис будет игнорировать все события, кроме как от этого считывателя.
    mBindedReaderName = mSettings.readerName;
    return st;
}

PCSC::Status Service::lock() {
    PCSC::Status st = SCardBeginTransaction(hCard);
    {XFS::Logger() << "SCardBeginTransaction(hCard=" << hCard << ") = " << st; }

    return st;
}
PCSC::Status Service::unlock() {
    // Заканчиваем транзакцию, ничего не делаем с картой.
    PCSC::Status st = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
    {XFS::Logger() << "SCardEndTransaction(hCard=" << hCard << ", SCARD_LEAVE_CARD) = " << st; }

    return st;
}
bool Service::match(const SCARD_READERSTATE& state, bool deviceChange) {
    // Изменения в количестве считывателей нас не интересуют.
    if (deviceChange) {
        return false;
    }
    // Один сервис может наблюдать только за одним считывателем за раз.
    // В зависимости от настроек, это может быть как один и тот же считыватель,
    // так и тот считыватель, в который первым была вставлена карточка. Поэтому,
    // если в данный момент мы уже работаем с какой-то карточкой, то игнорируем все
    // уведомления от остальных считывателей.
    if (!mBindedReaderName.empty() && mBindedReaderName != state.szReader) {
        return false;
    }
    return true;
}

void Service::notify(const SCARD_READERSTATE& state, bool deviceChange) {
    // Если изменения нас не интересуют, выходим.
    if (!match(state, deviceChange)) {
        return;
    }
    DWORD added = (state.dwCurrentState ^ state.dwEventState) & state.dwEventState;
    // В том случае, если сервис только что был добавлен, то реально изменений в
    // считывателях, скорее всего, не будет. Однако нам требуется как-то узнать текущее
    // состояние считывателей и разослать заинтересованным уведомления, поэтому на первый
    // раз мы исследуем новое состояние, а не изменения.
    //
    // Хотя стоит заметить, что с уведомлением подписчиков будут гонки. Прерывание ожидания
    // изменений, которое приводи к вызову этой функции, выполняется в момент добавления
    // сервиса, когда вызывающая сторона еще не могла зарегистрировать в нем каких-либо
    // слушателей. Тем не менее, из-за многопоточной природы она может успеть это сделать,
    // до того, как проснется поток ожидания изменений и сможет рапортовать о них через эту
    // функцию.
    DWORD forCheck = mInited ? added : state.dwEventState;
    mInited = true;
    {XFS::Logger() << "Service::notify: reader=" << state.szReader << ", state=" << PCSC::ReaderState(state.dwEventState) << ", added=" << PCSC::ReaderState(added); }
    /*if (forCheck & SCARD_STATE_) {
        EventNotifier::notify(WFS_SYSTEM_EVENT, PCSC::DeviceDetected(*this, state));
    }*/
    if (forCheck & SCARD_STATE_EMPTY) {
        EventNotifier::notify(WFS_SERVICE_EVENT, PCSC::CardRemoved(*this));
        if (hCard != 0) {
            close();
        }
    }
    if (forCheck & SCARD_STATE_PRESENT) {
        open(state.szReader);
        EventNotifier::notify(WFS_EXECUTE_EVENT, PCSC::CardInserted(*this));
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::pair<WFSIDCSTATUS*, PCSC::Status> Service::getStatus() {
    // Состояние считывателя.
    PCSC::MediaStatus state;
    DWORD nameLen = 0;
    DWORD atrLen = 0;
    // Если карточки не будет в считывателе, то вернется ошибка и в ответ мы дадим WFS_IDC_MEDIANOTPRESENT
    PCSC::Status st = SCARD_S_SUCCESS;
    if (hCard != 0) {
        st = SCardStatus(hCard,
            // Имя получать не будем, тем не менее длину получить требуется, NULL недопустим.
            NULL, &nameLen,
            // Небольшой хак допустим, у нас прозрачная обертка, ничего лишнего.
            (DWORD*)&state, (DWORD*)&mActiveProtocol,
            // ATR получать не будем, тем не менее длину получить требуется, NULL недопустим.
            NULL, &atrLen
        );
        {XFS::Logger() << "SCardStatus(hCard=" << hCard << ", ..., state=&" << state << ", dwActiveProtocol=&" << mActiveProtocol << ", ...) = " << st; }
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
        {XFS::Logger() << "SCardGetAttrib(hCard=" << hCard << ", attr=SCARD_ATTR_PROTOCOL_TYPES, types=&" << types << "...) = " << st; }
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
    lpCaps->fwReadTracks = mSettings.workarounds.track2.report ? WFS_IDC_TRACK2 : WFS_IDC_NOTSUPP;
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
    // Возможности считывателя по управлению питанием чипа.
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
        XFS::Result(ReqID, handle(), WFS_SUCCESS).attach(result).send(hWnd, WFS_EXECUTE_COMPLETE);
    }
}
std::pair<DWORD, BYTE*> Service::readATR() const {
    assert(hCard != 0 && "Service::readATR: Attempt read ATR when card not in the reader");

    {XFS::Logger() << "Read ATR (hCard=" << hCard << ')'; }
    
    std::pair<DWORD, BYTE*> result;

    // Получаем ATR (Answer To Reset). Сначала длину, потом сами данные.
    PCSC::Status st = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, NULL, &result.first);
    {XFS::Logger() << "SCardGetAttrib(hCard=" << hCard << ", SCARD_ATTR_ATR_STRING, ..., size=&" << result.first << ") = " << st; }
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
    result.second = XFS::allocArr<BYTE>(result.first);
    st = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, result.second, &result.first);
    {
        XFS::Logger l;
        l << "SCardGetAttrib(hCard=" << hCard << ", SCARD_ATTR_ATR_STRING, atr=&["
          << Hex(result.second, result.first) << "], size=&" << result.first << ") = " << st;
    }
    return result;
}
WFSIDCCARDDATA* Service::readChip() const {
    assert(hCard != 0 && "Service::readChip: Attempt read ATR when card not in the reader");

    {XFS::Logger() << "Read chip (hCard=" << hCard << ')'; }
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
    WFSIDCCARDDATA* data = XFS::alloc<WFSIDCCARDDATA>();
    // data->lpbData содержит ATR (Answer To Reset), прочитанный с чипа
    data->wDataSource = WFS_IDC_CHIP;
    //TODO: Статус прочитанных данных необходимо выставлять в соответствии со статусом,
    // который вернула SCardGetAttrib.
    data->wStatus = WFS_IDC_DATAOK;

    std::pair<DWORD, BYTE*> atr = readATR();
    data->ulDataLength = atr.first;
    data->lpbData = atr.second;
    return data;
}
WFSIDCCARDDATA* Service::readTrack2() const {
    assert(hCard != 0 && "Attempt read TRACK2 when card not in the reader");
    assert(mSettings.workarounds.track2.report == true && "Attempt read TRACK2 when setting Workarounds.Track2.Report is false");

    {XFS::Logger() << "Read track2 (hCard=" << hCard << ')'; }
    std::size_t size = mSettings.workarounds.track2.value.size();
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
    WFSIDCCARDDATA* data = XFS::alloc<WFSIDCCARDDATA>();
    data->wDataSource  = WFS_IDC_TRACK2;
    data->wStatus      = size != 0 ? WFS_IDC_DATAOK : WFS_IDC_DATAMISSING;
    if (size != 0) {
        data->ulDataLength = size;
        data->lpbData      = XFS::allocArr<BYTE>(size);
        std::memcpy(data->lpbData, mSettings.workarounds.track2.value.c_str(), size);
    }
    return data;
}

WFSIDCCARDDATA** Service::wrap(WFSIDCCARDDATA* iccData, XFS::ReadFlags forRead) const {
    assert((forRead.value() & WFS_IDC_CHIP) && "Service::wrap: Chip data not requested");
    // Данный вызов вернет заполненный нулями массив под два указателя на WFSIDCCARDDATA.
    // В поледнем элементе NULL -- признак конца массива.
    //TODO: Возможно, необходимо выделять память через WFSAllocateMore
    WFSIDCCARDDATA** result = XFS::allocArr<WFSIDCCARDDATA*>(forRead.size() + 1);

    std::size_t j = 0;
    for (std::size_t i = 0; i < XFS::ReadFlags::count; ++i) {
        XFS::ReadFlags::type flag = (1 << i);
        if (forRead.value() & flag) {
            {XFS::Logger() << "Read " << XFS::ReadFlags(flag); }
            if (flag == WFS_IDC_CHIP) {
                result[j] = iccData;
            } else
            // Kalignite требует, чтобы track2 мог читаться устройством, иначе он падает.
            // Поэтому, если такая информация запрошена и у нас в настройках сказано ее отдать,
            // то эмулируем ее наличие.
            if (flag == WFS_IDC_TRACK2 && mSettings.workarounds.track2.report) {
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
std::pair<WFSIDCCHIPIO*, PCSC::Status> Service::transmit(const WFSIDCCHIPIO* input) const {
    assert(input != NULL && "Service::transmit: No input from XFS subsystem");
    assert(hCard != 0 && "Service::transmit: No card in reader");

    {
        XFS::Logger l;
        l << "Service::transmit(input): dwActiveProtocol=" << mActiveProtocol
             << ", protocol=" << input->wChipProtocol
             << ", len=" << input->ulChipDataLength
             << ", data=[" << Hex(input->lpbChipData, input->ulChipDataLength)
             << ']';
    }
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
    {XFS::Logger() << "SCardTransmit(hCard=" << hCard << ", ...) = " << st; }
    {
        XFS::Logger l;
        l << "Service::transmit(result): len=" << result->ulChipDataLength
          << ", data=[" << Hex(result->lpbChipData, result->ulChipDataLength)
         << ']';
    }

    return std::make_pair(result, st);
}
std::pair<WFSIDCCHIPPOWEROUT*, PCSC::Status> Service::reset(XFS::ResetAction action) const {
    assert(hCard != 0 && "Service::reset: No card in the reader");

    PCSC::Status st = SCardReconnect(
        hCard,
        mSettings.exclusive ? SCARD_SHARE_EXCLUSIVE : SCARD_SHARE_SHARED,
        // Текущий активный протокол должен быть в числе запрошенных, иначе
        // функция вернет ошибку.
        mActiveProtocol.value() | SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        action.translate(),
        (DWORD*)&mActiveProtocol
    );
    {XFS::Logger() << "SCardReconnect(hCard=" << hCard << ", ..., dwActiveProtocol=&" << mActiveProtocol << ") = " << st; }

    std::pair<DWORD, BYTE*> atr = readATR();
    WFSIDCCHIPPOWEROUT* result = XFS::alloc<WFSIDCCHIPPOWEROUT>();
    result->ulChipDataLength = atr.first;
    result->lpbChipData = atr.second;
    return std::make_pair(result, st);
}