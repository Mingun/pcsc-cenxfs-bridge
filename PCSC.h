#ifndef PCSC_CENXFS_BRIDGE_PCSC_H
#define PCSC_CENXFS_BRIDGE_PCSC_H

#pragma once

#include <string>
#include <vector>
#include <map>

#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>

// Контейнер для хранения задач на чтение карточки.
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <boost/chrono/chrono.hpp>

// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include "PCSCStatus.h"
#include "XFSResult.h"

// Линкуемся с библиотекой реализации стандарта PC/SC в Windows
#pragma comment(lib, "winscard.lib")

namespace mi = boost::multi_index;
namespace bc = boost::chrono;

class Task {
public:
    /// Время, когда истекает таймаут для данной задачи.
    bc::steady_clock::time_point deadline;
    /// Сервис, который создал эту задачу.
    HSERVICE hService;
    /// Окно, которое получит уведомление о завершении задачи.
    HWND hWnd;
    /// Трекинговый номер данной задачи, который будет предоставлен в уведомлении окну `hWnd`.
    /// Должен быть уникален для каждой задачи.
    REQUESTID ReqID;
public:
    typedef boost::shared_ptr<Task> Ptr;
public:
    Task(bc::steady_clock::time_point deadline, HSERVICE hService, HWND hWnd, REQUESTID ReqID)
        : deadline(deadline), hService(hService), hWnd(hWnd), ReqID(ReqID) {}
    inline bool operator<(const Task& other) const {
        return deadline < other.deadline;
    }
    inline bool operator==(const Task& other) const {
        return hService == other.hService && ReqID == other.ReqID;
    }
public:
    /** Проверяет, является ли указанное событие тем, что ожидает данная задача.
        Если функция вернет `true`, та данная задача будет считаться завершенной
        и будет исключена ис списка зарегистрированных задач. Если зачате требуется
        выполнить какие-то действия в случае успешного завершения, это надо сделать
        здесь.
    @param state
        Данные изменившегося состояния.
    */
    virtual bool match(const SCARD_READERSTATE& state) const = 0;
    inline void notify(HRESULT result, DWORD messageType) const {
        Result(ReqID, hService, result).send(hWnd, messageType);
    }
    /// Вызывается, если запрос был отменен вызовом WFPCancelAsyncRequest.
    inline void cancel() const {
        notify(WFS_ERR_CANCELED, WFS_EXECUTE_COMPLETE);
    }
    inline void timeout() const {
        notify(WFS_ERR_TIMEOUT, WFS_EXECUTE_COMPLETE);
    }
};
class Service;
/** Класс, в конструкторе инициализирующий подсистему PC/SC, а в деструкторе закрывающий ее.
    Необходимо Создать ровно один экземпляр данного класса при загрузке DLL и уничтожить его
    при выгрузке. Наиболее просто это делается, путем объявления глобальной переменной данного
    класса.
*/
class PCSC {
public:
    /// Тип для отображения сервисов XFS на карты PC/SC.
    typedef std::map<HSERVICE, Service*> ServiceMap;
    typedef mi::multi_index_container<
        Task::Ptr,
        mi::indexed_by<
            // Сортировка по CardWaitTask::operator< -- по времени дедлайна, для выбора
            // задач, чей таймаут подошел.
            mi::ordered_non_unique<mi::identity<Task> >,
            // Сортировка по less<REQUESTID> по ReqID -- для удаления отмененных задач,
            // но ReqID уникален в пределах сервиса.
            mi::ordered_unique<mi::composite_key<
                Task,
                mi::member<Task, HSERVICE , &Task::hService>,
                mi::member<Task, REQUESTID, &Task::ReqID>
            > >
        >
    > TaskList;
public:
    /// Контекст подсистемы PC/SC.
    SCARDCONTEXT hContext;
    /// Список карт, открытых для взаимодействия с системой XFS.
    ServiceMap services;
    /// Список задач, упорядоченый по возрастанию времени дедлайна.
    /// Чем раньше дедлайн, тем ближе задача к голове списка.
    TaskList tasks;
    /// Мьютекс для защиты `tasks` от одновременной модификации.
    mutable boost::recursive_mutex tasksMutex;
    /// Поток для выполнения ожидания изменения в оборудовании.
    boost::shared_ptr<boost::thread> waitChangesThread;
    /// Флаг, выставляемый основным потоком, когда возникнет необходимость остановить
    ///`waitChangesThread`.
    bool stopRequested;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    PCSC();
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~PCSC();
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    inline bool isValid(HSERVICE hService) {
        return services.find(hService) != services.end();
    }
    Service& create(HSERVICE hService, const std::string& readerName);
    Service& get(HSERVICE hService);
    void remove(HSERVICE hService);
    void addTask(const Task::Ptr& task);
    /** Отменяет задачу с указанный трекинговым номером, возвращает `true`, если задача с таким
        номером имелась в списке, иначе `false`.
    @param hService
        XFS-сервис, для которого отменяется задача.
    @param ReqID
        Уникальный идентификатор отменяемой задачи. В списке не бывает двух задач с
        одинаковым идентификатором в пределах одного `hService`.

    @return
        `true`, если задача с таким номером имелась в списке, иначе `false`.
    */
    bool cancelTask(HSERVICE hService, REQUESTID ReqID);
public:// Подписка на события и генерация событий
    /** Добавляет указанное окно к подписчикам на указанные события от указанного сервиса.
    @return `false`, если указанный `hService` не зарегистрирован в объекте, иначе `true`.
    */
    bool addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass);
    bool removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass);
private:
    /// Вычисляет таймаут до ближайшего дедлайна потокобезопасным способом.
    DWORD getTimeout() const;
    /// Блокирует выполнение, пока поток не будет остановлен. Необходимо
    /// запускать из своего потока.
    void waitChangesRun();
    void getReadersAndWaitChanges();
    
    /** Данная блокирует выполнение до тех пор, пока не получит событие об изменении состояния
        физических устройств, поэтому она должна вызываться в отдельном потоке. После наступления
        события она отсылает соответствующие события всем заинтересованным слушателям подсистемы XFS.
    @param readers Отслеживаемые считыватели. Первый элемент отслеживает изменения устройств.
    @return `true`, если произошли изменения в количестве считывателей, `false` иначе.
    */
    bool waitChanges(std::vector<SCARD_READERSTATE>& readers);
    void notifyChanges(SCARD_READERSTATE& state);
private:// Управление задачами
    /// Добавляет задачу в очередь, возвращает `true`, если задача первая в очереди
    /// и дедлайн необходимо скорректировать, чтобы не пропустить дедлайн данной задачи.
    bool addTaskImpl(const Task::Ptr& task);
    bool cancelTaskImpl(HSERVICE hService, REQUESTID ReqID);
    /** Удаляет из списка все задачи, чье время дедлайна раньше или равно указанному
        и сигнализирует зарегистрированным в задаче слушателем о наступлении таймаута.
    @param now
        Время, для которого рассматривается, наступил таймаут или нет.
    */
    void processTimeouts(bc::steady_clock::time_point now);
private:
    static void log(std::string operation, Status st);
};

#endif // PCSC_CENXFS_BRIDGE_PCSC_H