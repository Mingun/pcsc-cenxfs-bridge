#include "ReaderChangesMonitor.h"

#include "Manager.h"

#include "PCSC/ReaderState.h"
#include "PCSC/Status.h"

#include "XFS/Logger.h"

ReaderChangesMonitor::ReaderChangesMonitor(Manager& manager)
    : manager(manager), stopRequested(false)
{
    // Запускаем поток ожидания изменений.
    waitChangesThread.reset(new boost::thread(&ReaderChangesMonitor::run, this));
}
ReaderChangesMonitor::~ReaderChangesMonitor() {
    // Запрашиваем остановку потока.
    stopRequested = true;
    // Сигнализируем о том, что необходимо прервать ожидание
    SCardCancel(manager.context());
    // Ожидаем, пока дойдет.
    waitChangesThread->join();
}
void ReaderChangesMonitor::run() {
    {XFS::Logger() << "Reader changes dispatch thread runned";}
    // Первоначально состояние неизвестное нам.
    DWORD readersState = SCARD_STATE_UNAWARE;
    while (!stopRequested) {
        // На входе текущее состояние считывателей -- на выходе новое состояние.
        readersState = getReadersAndWaitChanges(readersState);
    }
    XFS::Logger() << "Reader changes dispatch thread stopped";
}
/// Получаем список имен считывателей из строки со всеми именами, разделенными символом '\0'.
std::vector<const char*> getReaderNames(const std::vector<char>& readerNames) {
    XFS::Logger l;
    l << "Avalible readers:";
    std::size_t i = 0;
    std::size_t k = 0;
    std::vector<const char*> names;
    const std::size_t size = readerNames.size();
    while (i < size) {
        const char* name = &readerNames[i];
        // Ищем начало следующей строки.
        while (i < size && readerNames[i] != '\0') ++i;
        // Пропускаем '\0'.
        ++i;

        if (i < size) {
            l << '\n' << ++k << ": " << name;
            names.push_back(name);
        }
    }
    return names;
}
DWORD ReaderChangesMonitor::getReadersAndWaitChanges(DWORD readersState) {
    DWORD readersCount = 0;
    // Определяем доступные считыватели: сначало количество, затем сами считыватели.
    PCSC::Status st = SCardListReaders(manager.context(), NULL, NULL, &readersCount);
    {XFS::Logger() << "SCardListReaders[count](count=&" << readersCount << "): " << st;}

    // Получаем имена доступных считывателей. Все имена расположены в одной строке,
    // разделены символом '\0' в в конце списка также символ '\0' (т.о. в конце массива
    // идет подряд два '\0').
    std::vector<char> readerNames(readersCount);
    if (readersCount != 0) {
        st = SCardListReaders(manager.context(), NULL, &readerNames[0], &readersCount);
        XFS::Logger() << "SCardListReaders[data](count=&" << readersCount << "): " << st;
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
bool ReaderChangesMonitor::waitChanges(std::vector<SCARD_READERSTATE>& readers) {
    // Данная функция блокирует выполнение до тех пор, пока не произойдет событие.
    // Ждем его до таймаута ближайшей задачи на ожидание вставки карты.
    PCSC::Status st = SCardGetStatusChange(manager.context(), manager.getTimeout(), &readers[0], (DWORD)readers.size());
    {XFS::Logger() << "SCardGetStatusChange: " << st;}
    // Если изменение вызвано таймаутом операции, выкидываем из очереди ожидания все
    // задачи, чей таймаут уже наступил
    if (st.value() == SCARD_E_TIMEOUT) {
        manager.processTimeouts(bc::steady_clock::now());
    }
    bool readersChanged = false;
    bool first = true;
    for (std::vector<SCARD_READERSTATE>::iterator it = readers.begin(); it != readers.end(); ++it) {
        {
        PCSC::ReaderState diff = PCSC::ReaderState(it->dwCurrentState ^ it->dwEventState);
        XFS::Logger() << "[" << it->szReader << "] old state = " << PCSC::ReaderState(it->dwCurrentState);
        XFS::Logger() << "[" << it->szReader << "] new state = " << PCSC::ReaderState(it->dwEventState);
        XFS::Logger() << "[" << it->szReader << "] diff = " << diff;
        // XFS::Logger() << "[" << it->szReader << "] added = " << PCSC::ReaderState(it->dwEventState & diff.value());
        // XFS::Logger() << "[" << it->szReader << "] removed = " << PCSC::ReaderState(it->dwCurrentState & diff.value());
        }

        // Если что-то изменилось, уведомляем об этом всех заинтересованных.
        if (it->dwEventState & SCARD_STATE_CHANGED) {
            // Первый элемент в списке -- объект, через который приходят
            // уведомления об изменениях самих устройств.
            if (first) {
                readersChanged = true;
            }
            manager.notifyChanges(*it);
        }
        // Cообщаем PC/SC, что мы знаем текущее состояние
        it->dwCurrentState = it->dwEventState;
        first = false;
    }
    return readersChanged;
}