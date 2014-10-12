#include "PCSC.h"

#include <sstream>
#include <cassert>

#include "Service.h"
#include "PCSCReaderState.h"

/// Открывает соединение к менеджеру подсистемы PC/SC.
PCSC::PCSC() {
    // Создаем контекст.
    Status st = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    log("SCardEstablishContext", st);
    // Запускаем поток ожидания изменений от системы PC/SC -- добавление и удаление
    // считывателей и карточек.
    waitChangesThread.reset(new boost::thread(&PCSC::waitChangesRun, this));
}
/// Закрывает соединение к менеджеру подсистемы PC/SC.
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
    Service* service = new Service(*this, hService, readerName);
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
    // Закрытие PC/SC соединения происхоидт в деструкторе Card.
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
void PCSC::waitChangesRun() {
    WFMOutputTraceData("Reader changes dispatch thread runned");
    while (true) {
        getReadersAndWaitChanges();
    }
    WFMOutputTraceData("Reader changes dispatch thread stopped");
}
void PCSC::getReadersAndWaitChanges() {
    DWORD readersCount;
    // Определяем доступные считыватели: сначало количество, затем сами считыватели.
    Status st = SCardListReaders(hContext, NULL, NULL, &readersCount);
    log("SCardListReaders(get readers count)", st);
    // Получаем имена доступных считывателей. Все имена расположены в одной строке,
    // разделены символом '\0' в в конце списка также символ '\0' (т.о. в конце массива
    // идеет подряд два '\0').
    std::vector<char> readerNames(readersCount);
    st = SCardListReaders(hContext, NULL, &readerNames[0], &readersCount);
    log("SCardListReaders(get readers)", st);

    // Готовимся к ожиданию событий от всех обнаруженных считывателей и
    // изменению их количества.
    std::vector<SCARD_READERSTATE> readers(1+readersCount);
    // Считыватель со специальным именем, означающем, что необходимо мониторить
    // появление/пропажу считывателей.
    readers[0].szReader = "\\\\?PnP?\\Notification";
    readers[0].dwCurrentState = SCARD_STATE_UNAWARE;

    // Заполняем структуры для ожидания событий от найденных считывателей.
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
    // Ожидаем событий от считывателей. Если их количество обновилось,
    // то прекращаем ожидание. Повторный вход в данную процедуру случится
    // на следующем витке цикла в run.
    while (!waitChanges(readers));
}
bool PCSC::waitChanges(std::vector<SCARD_READERSTATE>& readers) {
    DWORD dwTimeout = INFINITE;
    // Если имеются задачи, то ожидаем до их таймаута, в противном случае до бесконечности.
    if (!tasks.empty()) {
        // Время до ближайшего таймаута.
        bc::steady_clock::duration dur = tasks.begin()->deadline - bc::steady_clock::now();
        // Преобразуем его в миллисекунды
        dwTimeout = (DWORD)bc::duration_cast<bc::milliseconds>(dur).count();
    }
    // Данная функция блокирует выполнение до тех пор, пока не произойдет событие.
    // Ждем его до бесконечности.
    Status st = SCardGetStatusChange(hContext, dwTimeout, &readers[0], (DWORD)readers.size());
    log("SCardGetStatusChange", st);
    // Если изменение вызвано таймаутом операции, выкидываем из очереди ожидания все
    // задачи, чей таймаут уже наступил
    if (st.value() == SCARD_E_TIMEOUT) {
        processTimeouts(bc::steady_clock::now());
    }
    bool readersChanged = false;
    bool first = true;
    for (std::vector<SCARD_READERSTATE>::iterator it = readers.begin(); it != readers.end(); ++it) {
        // Cообщаем PC/SC, что мы знаем текущее состояние
        it->dwCurrentState = it->dwEventState;
        // Если что-то изменилось, уведомляем об этом всех заинтересованных.
        if (it->dwEventState & SCARD_STATE_CHANGED) {
            // Первый элемент в списке -- объект, через который приходят
            // уведомления об изменениях самих устройств.
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
void PCSC::addTask(const Task& task) {
    if (addTaskImpl(task)) {
        // Прерываем ожидание потока на SCardGetStatusChange, т.к. ожидать теперь нужно
        // до нового таймаута. Ожидание с новым таймаутом начнется автоматически.
        Status st = SCardCancel(hContext);
        log("SCardCancel(addTask)", st);
    }
}
bool PCSC::cancelTask(HSERVICE hService, REQUESTID ReqID) {
    if (cancelTaskImpl(hService, ReqID)) {
        // Прерываем ожидание потока на SCardGetStatusChange, т.к. ожидать теперь нужно
        // до нового таймаута. Ожидание с новым таймаутом начнется автоматически.
        Status st = SCardCancel(hContext);
        log("SCardCancel(cancelTask)", st);
        return true;
    }
    return false;
}
bool PCSC::addTaskImpl(const Task& task) {
    std::pair<TaskList::iterator, bool> r = tasks.insert(task);
    // Вставляться должны уникальные по ReqID элементы.
    assert(r.second == false);
    assert(!tasks.empty());
    // Получаем первый индекс -- по времени дедлайна
    typedef TaskList::nth_index<0>::type Index0;
    Index0& byDeadline = tasks.get<0>();
    return *byDeadline.begin() == task;
}
bool PCSC::cancelTaskImpl(HSERVICE hService, REQUESTID ReqID) {
    // Получаем второй индекс -- по трекинговому номеру
    typedef TaskList::nth_index<1>::type Index1;
    Index1& byID = tasks.get<1>();
    Index1::iterator it = byID.find(boost::make_tuple(hService, ReqID));
    if (it == byID.end()) {
        return false;
    }
    // Сигнализируем зарегистрированным слушателем о том, что задача отменена.
    it->cancel();
    byID.erase(it);
    return true;
}
void PCSC::processTimeouts(bc::steady_clock::time_point now) {
    // Получаем первый индекс -- по времени дедлайна
    typedef TaskList::nth_index<0>::type Index0;
    Index0& byDeadline = tasks.get<0>();
    Index0::iterator begin = byDeadline.begin();
    Index0::iterator it = begin;
    for (; it != byDeadline.end(); ++it) {
        // Извлекаем все задачи, чей таймаут наступил. Так как все задачи упорядочены по
        // времени таймаута (чем раньше таймаут, тем ближе к голове очереди), то первая
        // задача, таймаут которой еще не наступил, прерывает цепочку таймаутов.
        if (it->deadline > now) {
            break;
        }
        // Сигнализируем зарегистрированным слушателем о том, что произошел таймаут.
        it->timeout();
    }
    // Удаляем завершенные таймаутом задачи.
    byDeadline.erase(begin, it);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PCSC::log(std::string operation, Status st) {
    std::stringstream ss;
    ss << operation << ": " << st;
    WFMOutputTraceData((LPSTR)ss.str().c_str());
}
