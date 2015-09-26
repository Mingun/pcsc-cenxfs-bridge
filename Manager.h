#ifndef PCSC_CENXFS_BRIDGE_Manager_H
#define PCSC_CENXFS_BRIDGE_Manager_H

#pragma once

#include "ReaderChangesMonitor.h"
#include "ServiceContainer.h"
#include "Task.h"

#include "PCSC/Context.h"
#include "PCSC/Status.h"

#include "XFS/Result.h"

#include <vector>

#include <boost/chrono/chrono.hpp>

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
    // Порядок следования полей важен, т.к. сначала будут разрушаться
    // те объекты, которые объявлены ниже. В первую очередь необходимо
    // завершить поток опроса изменений, затем выслать уведомления об
    // отмене всех задач и в конце удалить все сервисы.

    /// Список сервисов, открытых для взаимодействия с системой XFS.
    ServiceContainer services;
    /// Контейнер, управляющий асинхронными задачами на получение данных с карточки.
    TaskContainer tasks;
    /// Объект для слежения за состоянием считывателей и рассылки уведомлений,
    /// когда состояние меняется. При разрушении прекращает ожидание изменений.
    ReaderChangesMonitor readerChangesMonitor;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    Manager();
public:// Управление сервисами
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    inline bool isValid(HSERVICE hService) const { return services.isValid(hService); }
    /** @return true, если в менеджере не зарегистрировано ни одного сервиса. */
    inline bool isEmpty() const { return services.isEmpty(); }

    Service& create(HSERVICE hService, const Settings& settings);
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
private:// Функции для использования ReaderChangesMonitor
    friend class ReaderChangesMonitor;
    /// @copydoc TaskContainer::getTimeout
    inline DWORD getTimeout() const { return tasks.getTimeout(); }
    /// @copydoc TaskContainer::processTimeouts
    inline void processTimeouts(boost::chrono::steady_clock::time_point now) { tasks.processTimeouts(now); }
    /// @copydoc TaskContainer::notifyChanges
    void notifyChanges(const SCARD_READERSTATE& state);
};

#endif // PCSC_CENXFS_BRIDGE_Manager_H