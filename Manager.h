#ifndef PCSC_CENXFS_BRIDGE_Manager_H
#define PCSC_CENXFS_BRIDGE_Manager_H

#pragma once

#include "ServiceContainer.h"
#include "Task.h"

#include "PCSC/Context.h"
#include "PCSC/Status.h"

#include "XFS/Result.h"

#include <vector>

#include <boost/chrono/chrono.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>

// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

// Линкуемся с библиотекой реализации стандарта PC/SC в Windows
#pragma comment(lib, "winscard.lib")

class Service;
class Settings;
/** Класс, в конструкторе инициализирующий подсистему PC/SC, а в деструкторе закрывающий ее.
    Необходимо Создать ровно один экземпляр данного класса при загрузке DLL и уничтожить его
    при выгрузке. Наиболее просто это делается, путем объявления глобальной переменной данного
    класса.
*/
class Manager : public PCSC::Context {
private:
    /// Поток для выполнения ожидания изменения в оборудовании.
    boost::shared_ptr<boost::thread> waitChangesThread;
    /// Флаг, выставляемый основным потоком, когда возникнет необходимость остановить
    ///`waitChangesThread`.
    bool stopRequested;

    /// Контейнер, управляющий асинхронными задачами на получение данных с карточки.
    TaskContainer tasks;
    /// Список сервисов, открытых для взаимодействия с системой XFS.
    ServiceContainer services;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    Manager();
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~Manager();
public:// Управление сервисами
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    inline bool isValid(HSERVICE hService) const { return services.isValid(hService); }
    /** @return true, если в менеджере не зарегистрировано ни одного сервиса. */
    inline bool isEmpty() const { return services.isEmpty(); }

    inline Service& create(HSERVICE hService, const Settings& settings) {
        return services.create(*this, hService, settings);
    }
    inline Service& get(HSERVICE hService) { return services.get(hService); }
    inline void remove(HSERVICE hService) { services.remove(hService); }
public:// Подписка на события и генерация событий
    /** Добавляет указанное окно к подписчикам на указанные события от указанного сервиса.
    @return `false`, если указанный `hService` не зарегистрирован в объекте, иначе `true`.
    */
    inline bool addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
        return services.addSubscriber(hService, hWndReg, dwEventClass);
    }
    inline bool removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
        return services.removeSubscriber(hService, hWndReg, dwEventClass);
    }
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
};

#endif // PCSC_CENXFS_BRIDGE_Manager_H