#ifndef PCSC_CENXFS_BRIDGE_Manager_H
#define PCSC_CENXFS_BRIDGE_Manager_H

#pragma once

#include <string>
#include <vector>
#include <map>

#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/chrono/chrono.hpp>

// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include "PCSC/Status.h"
#include "XFS/Result.h"
#include "Task.h"

// Линкуемся с библиотекой реализации стандарта PC/SC в Windows
#pragma comment(lib, "winscard.lib")

class Service;
class Settings;
/** Класс, в конструкторе инициализирующий подсистему PC/SC, а в деструкторе закрывающий ее.
    Необходимо Создать ровно один экземпляр данного класса при загрузке DLL и уничтожить его
    при выгрузке. Наиболее просто это делается, путем объявления глобальной переменной данного
    класса.
*/
class Manager {
public:
    /// Тип для отображения сервисов XFS на карты PC/SC.
    typedef std::map<HSERVICE, Service*> ServiceMap;
private:
    /// Контекст подсистемы PC/SC.
    SCARDCONTEXT hContext;
    /// Список карт, открытых для взаимодействия с системой XFS.
    ServiceMap services;
    /// Поток для выполнения ожидания изменения в оборудовании.
    boost::shared_ptr<boost::thread> waitChangesThread;
    /// Флаг, выставляемый основным потоком, когда возникнет необходимость остановить
    ///`waitChangesThread`.
    bool stopRequested;

    /// Контейнер, управляющий асинхронными задачами на получение данных с карточки.
    TaskContainer tasks;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    Manager();
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~Manager();
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    inline bool isValid(HSERVICE hService) {
        return services.find(hService) != services.end();
    }
    /** @return true, если в менеджере не зарегистрировано ни одного сервиса. */
    inline bool isEmpty() const { return services.empty(); }

    Service& create(HSERVICE hService, const Settings& settings);
    Service& get(HSERVICE hService);
    void remove(HSERVICE hService);
public:// Управление задачами
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
public:// Доступ к внутренностям
    inline SCARDCONTEXT context() const { return hContext; }
private:// Опрос изменений
    /// Блокирует выполнение, пока поток не будет остановлен. Необходимо
    /// запускать из своего потока.
    void waitChangesRun();
    DWORD getReadersAndWaitChanges(DWORD readersState);

    /** Данная блокирует выполнение до тех пор, пока не получит событие об изменении состояния
        физических устройств, поэтому она должна вызываться в отдельном потоке. После наступления
        события она отсылает соответствующие события всем заинтересованным слушателям подсистемы XFS.
    @param readers Отслеживаемые считыватели. Первый элемент отслеживает изменения устройств.
    @return `true`, если произошли изменения в количестве считывателей, `false` иначе.
    */
    bool waitChanges(std::vector<SCARD_READERSTATE>& readers);
    void notifyChanges(const SCARD_READERSTATE& state);
private:
    static void log(std::string operation, PCSC::Status st);
};

#endif // PCSC_CENXFS_BRIDGE_Manager_H