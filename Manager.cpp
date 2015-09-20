#include "Manager.h"

#include "Service.h"
#include "Settings.h"

#include "XFS/Logger.h"

#include <sstream>
#include <cassert>

/// Открывает соединение к менеджеру подсистемы PC/SC.
Manager::Manager()
    : stopRequested(false)
{
    // Запускаем поток ожидания изменений от системы PC/SC -- добавление и удаление
    // считывателей и карточек.
    waitChangesThread.reset(new boost::thread(&Manager::waitChangesRun, this));
}
/// Закрывает соединение к менеджеру подсистемы PC/SC.
Manager::~Manager() {
    // Запрашиваем остановку потока.
    stopRequested = true;
    // Сигнализируем о том, что необходимо прервать ожидание
    SCardCancel(context());
    // Ожидаем, пока дойдет.
    waitChangesThread->join();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Manager::waitChangesRun() {
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
DWORD Manager::getReadersAndWaitChanges(DWORD readersState) {
    DWORD readersCount = 0;
    // Определяем доступные считыватели: сначало количество, затем сами считыватели.
    PCSC::Status st = SCardListReaders(context(), NULL, NULL, &readersCount);
    {XFS::Logger() << "SCardListReaders(get readers count): " << st;}

    // Получаем имена доступных считывателей. Все имена расположены в одной строке,
    // разделены символом '\0' в в конце списка также символ '\0' (т.о. в конце массива
    // идет подряд два '\0').
    std::vector<char> readerNames(readersCount);
    if (readersCount != 0) {
        st = SCardListReaders(context(), NULL, &readerNames[0], &readersCount);
        XFS::Logger() << "SCardListReaders(get readers): " << st;
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
bool Manager::waitChanges(std::vector<SCARD_READERSTATE>& readers) {
    // Данная функция блокирует выполнение до тех пор, пока не произойдет событие.
    // Ждем его до бесконечности.
    PCSC::Status st = SCardGetStatusChange(context(), tasks.getTimeout(), &readers[0], (DWORD)readers.size());
    {XFS::Logger() << "SCardGetStatusChange: " << st;}
    // Если изменение вызвано таймаутом операции, выкидываем из очереди ожидания все
    // задачи, чей таймаут уже наступил
    if (st.value() == SCARD_E_TIMEOUT) {
        tasks.processTimeouts(bc::steady_clock::now());
    }
    bool readersChanged = false;
    bool first = true;
    for (std::vector<SCARD_READERSTATE>::iterator it = readers.begin(); it != readers.end(); ++it) {
        // Если что-то изменилось, уведомляем об этом всех заинтересованных.
        if (it->dwEventState & SCARD_STATE_CHANGED) {
            // Первый элемент в списке -- объект, через который приходят
            // уведомления об изменениях самих устройств.
            if (first) {
                readersChanged = true;
            }
            notifyChanges(*it);
        }
        // Cообщаем PC/SC, что мы знаем текущее состояние
        it->dwCurrentState = it->dwEventState;
        first = false;
    }
    return readersChanged;
}
void Manager::notifyChanges(const SCARD_READERSTATE& state) {
    // Сначала уведомляем подписанных слушателей об изменениях, и только затем
    // пытаемся завершить задачи.
    services.notifyChanges(state);
    tasks.notifyChanges(state);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Manager::addTask(const Task::Ptr& task) {
    if (tasks.addTask(task)) {
        // Прерываем ожидание потока на SCardGetStatusChange, т.к. ожидать теперь нужно
        // до нового таймаута. Ожидание с новым таймаутом начнется автоматически.
        PCSC::Status st = SCardCancel(context());
        XFS::Logger() << "SCardCancel(addTask): " << st;
    }
}
bool Manager::cancelTask(HSERVICE hService, REQUESTID ReqID) {
    if (tasks.cancelTask(hService, ReqID)) {
        // Прерываем ожидание потока на SCardGetStatusChange, т.к. ожидать теперь нужно
        // до нового таймаута. Ожидание с новым таймаутом начнется автоматически.
        PCSC::Status st = SCardCancel(context());
        XFS::Logger() << "SCardCancel(cancelTask): " << st;
        return true;
    }
    return false;
}