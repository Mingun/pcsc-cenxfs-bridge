#include "PCSC.h"

#include <sstream>
#include <cassert>

#include "Service.h"
#include "PCSCReaderState.h"

/// Открывает соединение к менеджеру подсистемы PC/SC.
PCSC::PCSC() : stopRequested(false) {
    // Создаем контекст.
    Status st = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    log("SCardEstablishContext", st);
    // Запускаем поток ожидания изменений от системы PC/SC -- добавление и удаление
    // считывателей и карточек.
    waitChangesThread.reset(new boost::thread(&PCSC::waitChangesRun, this));
}
/// Закрывает соединение к менеджеру подсистемы PC/SC.
PCSC::~PCSC() {
    // Запрашиваем остановку потока.
    stopRequested = true;
    // Сигнализируем о том, что необходимо прервать ожидание
    SCardCancel(hContext);
    // Ожидаем, пока дойдет.
    waitChangesThread->join();
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
    // Первоначально состояние неизвестное нам.
    DWORD readersState = SCARD_STATE_UNAWARE;
    while (!stopRequested) {
        // На входе текущее состояние считывателей -- на выходе новое состояние.
        readersState = getReadersAndWaitChanges(readersState);
    }
    WFMOutputTraceData("Reader changes dispatch thread stopped");
}
DWORD PCSC::getTimeout() const {
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

    // Если имеются задачи, то ожидаем до их таймаута, в противном случае до бесконечности.
    if (!tasks.empty()) {
        // Время до ближайшего таймаута.
        bc::steady_clock::duration dur = (*tasks.begin())->deadline - bc::steady_clock::now();
        // Преобразуем его в миллисекунды
        return (DWORD)bc::duration_cast<bc::milliseconds>(dur).count();
    }
    return INFINITE;
}

/// Получаем список имен считывателей из строки со всеми именами, разделенными символом '\0'.
std::vector<const char*> getReaderNames(const std::vector<char>& readerNames) {
    std::stringstream ss;
    ss << std::string("Avalible readers:\n");
    std::size_t i = 0;
    std::size_t k = 0;
    std::vector<const char*> names;
    const std::size_t size = readerNames.size();
    while (i < size) {
        const char* name = &readerNames[i];
        ss << ++k << ": " << name << std::endl;

        names.push_back(name);
        // Ещем начало следующей строки.
        while (i < size && readerNames[i] != '\0') ++i;
        // Пропускаем '\0'.
        ++i;
    }
    WFMOutputTraceData((LPSTR)ss.str().c_str());
    return names;
}
DWORD PCSC::getReadersAndWaitChanges(DWORD readersState) {
    DWORD readersCount = 0;
    // Определяем доступные считыватели: сначало количество, затем сами считыватели.
    Status st = SCardListReaders(hContext, NULL, NULL, &readersCount);
    log("SCardListReaders(get readers count)", st);

    // Получаем имена доступных считывателей. Все имена расположены в одной строке,
    // разделены символом '\0' в в конце списка также символ '\0' (т.о. в конце массива
    // идет подряд два '\0').
    std::vector<char> readerNames(readersCount);
    if (readersCount != 0) {
        st = SCardListReaders(hContext, NULL, &readerNames[0], &readersCount);
        log("SCardListReaders(get readers)", st);
    }

    std::vector<const char*> names = getReaderNames(readerNames);

    // Готовимся к ожиданию событий от всех обнаруженных считывателей и
    // изменению их количества.
    std::vector<SCARD_READERSTATE> readers(1 + names.size());
    // Считыватель со специальным именем, означающем, что необходимо мониторить
    // появление/пропажу считывателей.
    readers[0].szReader = "\\\\?PnP?\\Notification";
    readers[0].dwCurrentState = readersState;

    // Заполняем структуры для ожидания событий от найденных считывателей.
    for (std::size_t i = 0; i < names.size(); ++i) {
        readers[1 + i].szReader = names[i];
    }

    // Ожидаем событий от считывателей. Если их количество обновилось,
    // то прекращаем ожидание. Повторный вход в данную процедуру случится
    // на следующем витке цикла в run.
    while (!waitChanges(readers)) {
        if (stopRequested) {
            // Не важно, что возвращать, нам лишь бы выйти.
            return 0;
        }
    }
    // Возвращаем текущее состояние наблюдателя за считывателями.
    return readers[0].dwCurrentState;
}
bool PCSC::waitChanges(std::vector<SCARD_READERSTATE>& readers) {
    // Данная функция блокирует выполнение до тех пор, пока не произойдет событие.
    // Ждем его до бесконечности.
    Status st = SCardGetStatusChange(hContext, getTimeout(), &readers[0], (DWORD)readers.size());
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
void PCSC::notifyChanges(SCARD_READERSTATE& state) {
    // Сначала уведомляем подписанных слушателей об изменениях, и только затем
    // пытаемся завершить задачи.
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
        it->second->notify(state);
    }

    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);
    for (TaskList::iterator it = tasks.begin(); it != tasks.end();) {
        // Если задача ожидала этого события, то удаляем ее из списка.
        if ((*it)->match(state)) {
            it = tasks.erase(it);
            continue;
        }
        ++it;
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PCSC::addTask(const Task::Ptr& task) {
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
bool PCSC::addTaskImpl(const Task::Ptr& task) {
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

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
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

    // Получаем второй индекс -- по трекинговому номеру
    typedef TaskList::nth_index<1>::type Index1;
    Index1& byID = tasks.get<1>();
    Index1::iterator it = byID.find(boost::make_tuple(hService, ReqID));
    if (it == byID.end()) {
        return false;
    }
    // Сигнализируем зарегистрированным слушателем о том, что задача отменена.
    (*it)->cancel();
    byID.erase(it);
    return true;
}
void PCSC::processTimeouts(bc::steady_clock::time_point now) {
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

    // Получаем первый индекс -- по времени дедлайна
    typedef TaskList::nth_index<0>::type Index0;
    Index0& byDeadline = tasks.get<0>();
    Index0::iterator begin = byDeadline.begin();
    Index0::iterator it = begin;
    for (; it != byDeadline.end(); ++it) {
        // Извлекаем все задачи, чей таймаут наступил. Так как все задачи упорядочены по
        // времени таймаута (чем раньше таймаут, тем ближе к голове очереди), то первая
        // задача, таймаут которой еще не наступил, прерывает цепочку таймаутов.
        if ((*it)->deadline > now) {
            break;
        }
        // Сигнализируем зарегистрированным слушателем о том, что произошел таймаут.
        (*it)->timeout();
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
