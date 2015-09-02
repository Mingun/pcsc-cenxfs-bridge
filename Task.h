#ifndef PCSC_CENXFS_BRIDGE_Task_H
#define PCSC_CENXFS_BRIDGE_Task_H

#pragma once

#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp> 
#include <boost/shared_ptr.hpp>

// Контейнер для хранения задач на чтение карточки.
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <boost/chrono/chrono.hpp>

// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include "XFS/Result.h"

namespace mi = boost::multi_index;
namespace bc = boost::chrono;

class Service;

/** Класс для хранения информации, необходимой для посылки EXECUTE событий после
    наступления оных окну, которое были указаны в качестве приемника при вызове
    функции `WFPExecute`.
*/
class Task {
public:
    /// Время, когда истекает таймаут для данной задачи.
    bc::steady_clock::time_point deadline;
    /// Сервис, который создал эту задачу.
    Service& mService;
    /// Окно, которое получит уведомление о завершении задачи.
    HWND hWnd;
    /// Трекинговый номер данной задачи, который будет предоставлен в уведомлении окну `hWnd`.
    /// Должен быть уникален для каждой задачи.
    REQUESTID ReqID;
public:
    typedef boost::shared_ptr<Task> Ptr;
public:
    Task(bc::steady_clock::time_point deadline, Service& service, HWND hWnd, REQUESTID ReqID)
        : deadline(deadline), mService(service), hWnd(hWnd), ReqID(ReqID) {}
    inline bool operator<(const Task& other) const {
        return deadline < other.deadline;
    }
    inline bool operator==(const Task& other) const {
        return &mService == &other.mService && ReqID == other.ReqID;
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
    /// Уведомляет XFS-слушателя о завершении ожидания.
    /// @param result Код ответа для завершения.
    void complete(HRESULT result) const {
        XFS::Result(ReqID, serviceHandle(), result).send(hWnd, WFS_EXECUTE_COMPLETE);
    }
    /// Вызывается, если запрос был отменен вызовом WFPCancelAsyncRequest.
    inline void cancel() const { complete(WFS_ERR_CANCELED); }
    inline void timeout() const { complete(WFS_ERR_TIMEOUT); }
    HSERVICE serviceHandle() const;
};
/// Содержит список задач и методы для их потокобезопасного добавления, отмены и обработки.
class TaskContainer {
    typedef mi::multi_index_container<
        Task::Ptr,
        mi::indexed_by<
            // Сортировка по CardWaitTask::operator< -- по времени дедлайна, для выбора
            // задач, чей таймаут подошел. В один момент времени может завершиться несколько
            // задач, поэтому индекс неуникальный.
            mi::ordered_non_unique<mi::identity<Task> >,
            // Сортировка по less<REQUESTID> по ReqID -- для удаления отмененных задач,
            // но ReqID уникален в пределах сервиса.
            mi::ordered_unique<mi::composite_key<
                Task,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(Task, HSERVICE, serviceHandle),
                mi::member<Task, REQUESTID, &Task::ReqID>
            > >
        >
    > TaskList;
private:
    /// Список задач, упорядоченый по возрастанию времени дедлайна.
    /// Чем раньше дедлайн, тем ближе задача к голове списка.
    TaskList tasks;
    /// Мьютекс для защиты `tasks` от одновременной модификации.
    mutable boost::recursive_mutex tasksMutex;
public:
    /** Добавляет задачу в очередь, возвращает `true`, если задача первая в очереди
        и дедлайн необходимо скорректировать, чтобы не пропустить дедлайн данной задачи.
    @param task
        Новая задача.

    @return
        Если дедлайн новой задачи окажется спереди всех прочих задач, то метод возвращает
        `true`. Это означает, что нужно обновить таймаут ожидания событий от считывателей,
        чтобы не пропустить таймаут данной задачи. Если время ближайшего дедлайна не меняется,
        возвращает `false`.
    */
    bool addTask(const Task::Ptr& task);
    /** Отменяет задачу, созданную указанным сервис-провайдером и имеющую указанный
        трекинговый номер.
    @param hService
        Сервис, поставивший задачу в очередб на выполнение и теперь желающий отменить ее.
    @param ReqID
        Номер задачи, присвоенный ей XFS менеджером.

    @return
        `true`, если задача была найдена (в этом случае она будет отменена), иначе `false`,
        что означает, что задачи с такими параметрами не существует в очереди задач.
    */
    bool cancelTask(HSERVICE hService, REQUESTID ReqID);

    /// Вычисляет таймаут до ближайшего дедлайна потокобезопасным способом.
    DWORD getTimeout() const;
    /** Удаляет из списка все задачи, чье время дедлайна раньше или равно указанному
        и сигнализирует зарегистрированным в задаче слушателем о наступлении таймаута.
    @param now
        Время, для которого рассматривается, наступил таймаут или нет.
    */
    void processTimeouts(bc::steady_clock::time_point now);
    /** Уведомляет все задачи об изменении в считывателе. В результате некоторые задачи могут завершиться.
    @param state
        Состояние изменившегося считывателя, в том числе это может быть изменение
        количества считывателей.
    */
    void notifyChanges(const SCARD_READERSTATE& state);
};
#endif PCSC_CENXFS_BRIDGE_Task_H