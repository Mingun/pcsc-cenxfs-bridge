#include "PCSC.h"

#include <sstream>
#include <cassert>

#include "Service.h"
#include "PCSCReaderState.h"

/// ќткрывает соединение к менеджеру подсистемы PC/SC.
PCSC::PCSC() {
    // —оздаем контекст.
    Status st = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    log("SCardEstablishContext", st);
    // «апускаем поток ожидани€ изменений от системы PC/SC -- добавление и удаление
    // считывателей и карточек.
    waitThread.reset(new boost::thread(&PCSC::run, this));
}
/// «акрывает соединение к менеджеру подсистемы PC/SC.
PCSC::~PCSC() {
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
        assert(it->second != NULL);
        delete it->second;
    }
    services.clear();
    Status st = SCardReleaseContext(hContext);
    log("SCardReleaseContext", st);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Service& PCSC::create(HSERVICE hService, const std::string& readerName) {
    Service* service = new Service(hService, readerName);
    services.insert(std::make_pair(hService, service));
    return *service;
}
Service& PCSC::get(HSERVICE hService) {
    assert(isValid(hService));
    Service* service = services.find(hService)->second;
    assert(service != NULL);
    return *service;
}
void PCSC::remove(HSERVICE hService) {
    assert(isValid(hService));
    ServiceMap::iterator it = services.find(hService);

    assert(it->second != NULL);
    // «акрытие PC/SC соединени€ происхоидт в деструкторе Card.
    delete it->second;
    services.erase(it);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool PCSC::addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
    ServiceMap::iterator it = services.find(hService);
    if (it == services.end()) {
        return false;
    }
    assert(it->second != NULL);
    it->second->add(hWndReg, dwEventClass);
    return true;
}
bool PCSC::removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
    ServiceMap::iterator it = services.find(hService);
    if (it == services.end()) {
        return false;
    }
    assert(it->second != NULL);
    it->second->remove(hWndReg, dwEventClass);
    return true;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PCSC::run() {
    WFMOutputTraceData("Dispatch thread runned");
    do {
        getReadersAndWaitChanges();
    } while (true);
    WFMOutputTraceData("Dispatch thread stopped");
}
void PCSC::getReadersAndWaitChanges() {
    DWORD readersCount;
    // ќпредел€ем доступные считыватели: сначало количество, затем сами считыватели.
    Status st = SCardListReaders(hContext, NULL, NULL, &readersCount);
    log("SCardListReaders(get readers count)", st);

    std::vector<char> readerNames(readersCount);
    st = SCardListReaders(hContext, NULL, &readerNames[0], &readersCount);
    log("SCardListReaders(get readers)", st);

    std::vector<SCARD_READERSTATE> readers(1+readersCount);
    // —читыватель со специальным именем, означающем, что необходимо мониторить
    // по€вление/пропажу считывателей.
    readers[0].szReader = "\\\\?PnP?\\Notification";
    readers[0].dwCurrentState = SCARD_STATE_UNAWARE;

    // «аполн€ем структуры дл€ ожидани€ событий от найденных считывателей.
    std::size_t i = 0;
    std::vector<char>::const_iterator begin = readerNames.begin();
    for (std::vector<char>::const_iterator it = begin; it != readerNames.end() && i < readersCount;) {
        if (*it == '\0') {
            readers[1 + i++].szReader = static_cast<LPCSTR>(&*begin);
            begin = ++it;
            continue;
        }
        ++it;
    }
    // ќжидаем событий от считывателей. ≈сли их количество обновилось,
    // то прекращаем ожидание. ѕовторный вход в данную процедуру случитс€
    // на следующем витке цикла в run.
    while (!waitChanges(readers));
}
bool PCSC::waitChanges(std::vector<SCARD_READERSTATE>& readers) {
    // ƒанна€ функци€ блокирует выполнение до тех пор, пока не произойдет событие.
    // ∆дем его до бесконечности.
    Status st = SCardGetStatusChange(hContext, INFINITE, &readers[0], (DWORD)readers.size());
    log("SCardGetStatusChange", st);
    bool readersChanged = false;
    bool first = true;
    for (std::vector<SCARD_READERSTATE>::iterator it = readers.begin(); it != readers.end(); ++it) {
        // Cообщаем PC/SC, что мы знаем текущее состо€ние
        it->dwCurrentState = it->dwEventState;
        // ≈сли что-то изменилось, уведомл€ем об этом всех заинтересованных.
        if (it->dwEventState & SCARD_STATE_CHANGED) {
            // ѕервый элемент в списке -- объект, через который приход€т
            // уведомлени€ об изменени€х самих устройств.
            if (first) {
                readersChanged = true;
            }
            notifyChanges(*it);
        }
        first = false;
    }
    return readersChanged;
}
void PCSC::notifyChanges(SCARD_READERSTATE& state) const {
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
        it->second->notify(state);
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PCSC::log(std::string operation, Status st) {
    std::stringstream ss;
    ss << operation << ": " << st;
    WFMOutputTraceData((LPSTR)ss.str().c_str());
}
