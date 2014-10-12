#ifndef PCSC_CENXFS_BRIDGE_ServiceImpl_H
#define PCSC_CENXFS_BRIDGE_ServiceImpl_H

#pragma once

#include <sstream>
#include <cassert>

// Контейнер для хранения задач на чтение карточки.
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/chrono/chrono.hpp>

namespace mi = boost::multi_index;
namespace ba = boost::asio;
namespace bc = boost::chrono;
namespace bs = boost::system;

class Task {
public:
    /// Время, когда истекает таймаут для данной задачи.
    bc::steady_clock::time_point deadline;
    /// Окно, которое получит уведомление о завершении задачи.
    HWND hWnd;
    /// Трекинговый номер данной задачи, который будет предоставлен в уведомлении окну `hWnd`.
    /// Должен быть уникален для каждой задачи.
    REQUESTID ReqID;
    /// Сервис, который создал эту задачу.
    Service& service;
public:
    Task(bc::steady_clock::time_point deadline, HWND hWnd, REQUESTID ReqID, Service& service)
        : deadline(deadline), hWnd(hWnd), ReqID(ReqID), service(service) {}
    /// Хотя оператор присваивания должен был бы сгенерироваться автоматически,
    /// почему-то этого не произошло. Возможно, из-за наличия ссылки в качестве поля?
    Task& operator=(const Task& other) {
        this->deadline = other.deadline;
        this->hWnd = other.hWnd;
        this->ReqID = other.ReqID;
        this->service = other.service;
        return *this;
    }
    inline bool operator<(const Task& other) const {
        return deadline < other.deadline;
    }
    inline bool operator==(const Task& other) const {
        return ReqID == other.ReqID;
    }
public:
    /// Вызывается, когда карточка была вставлена и должна быть прочитана.
    void done() const {
        std::pair<WFSIDCCARDDATA*, Status> r = service.read();
        Result(ReqID, service.handle(), r.second).data(r.first).send(hWnd, WFS_EXECUTE_COMPLETE);
    }
    /// Вызывается, если запрос был отменен вызовом WFPCancelAsyncRequest.
    void cancel() const {
        Result(ReqID, service.handle(), WFS_ERR_CANCELED).send(hWnd, WFS_EXECUTE_COMPLETE);
    }
    void timeout() const {
        Result(ReqID, service.handle(), WFS_ERR_TIMEOUT).send(hWnd, WFS_EXECUTE_COMPLETE);
    }
};

class ServiceImpl {
    typedef mi::multi_index_container<
        Task,
        mi::indexed_by<
            // Сортировка по CardWaitTask::operator< -- по времени дедлайна, для выбора
            // задач, чей таймаут подошел.
            mi::ordered_non_unique<mi::identity<Task> >,
            // Сортировка по less<REQUESTID> по ReqID -- для удаления отмененных задач.
            mi::ordered_unique<mi::member<Task, REQUESTID, &Task::ReqID> >
        >
    > TaskList;
private:
    /// Список задач, упорядоченый по возрастанию времени дедлайна.
    /// Чем раньше дедлайн, тем ближе задача к голове списка.
    TaskList tasks;
    ba::steady_timer timer;
    Service& service;
public:
    ServiceImpl(ba::io_service& ioService, Service& service)
        : timer(ioService), service(service) {}
    void addTask(const Task& task) {
        if (addTaskImpl(task)) {
            // Поскольку наша задача оказалась первой в очереди, то начинаем ожидать
            // до ее предельного срока.
            startWait(task);
        }
    }
    bool cancelTask(REQUESTID ReqID) {
        // Получаем второй индекс -- по трекинговому номеру
        typedef TaskList::nth_index<1>::type Index1;
        Index1& byID = tasks.get<1>();
        Index1::iterator it = byID.find(ReqID);
        if (it == byID.end()) {
            return false;
        }
        byID.erase(it);
        return true;
    }
private:
    /// Добавляет задачу в очередь, возвращает `true`, если задача первая в очереди
    /// и дедлайн необходимо скорректировать, чтобы не пропустить дедлайн данной задачи.
    bool addTaskImpl(const Task& task) {
        std::pair<TaskList::iterator, bool> r = tasks.insert(task);
        // Вставляться должны уникальные по ReqID элементы.
        assert(r.second == false);
        assert(!tasks.empty());
        // Получаем первый индекс -- по времени дедлайна
        typedef TaskList::nth_index<0>::type Index0;
        Index0& byDeadline = tasks.get<0>();
        return *byDeadline.begin() == task;
    }

    /// Обработчик, вызываемый после истечения 
    void onTimeout(const bs::error_code& ec) {
        log("onTimeout", ec);
        // На время вызова таймаута в очереди должна быть как минимум одна задача --
        // ведь для кого-то таймаут должен быть вызван.
        assert(!tasks.empty());
        // Если операция была отменена
        if (ec == ba::error::operation_aborted) {

        }
        // Извлекаем все задачи, чей таймаут наступил. Так как все задачи упорядочены по
        // времени таймаута (чем раньше таймаут, тем ближе к голове очереди), то первая
        // задача, таймаут которой еще не наступил, прерывает цепочку таймаутов.
        bc::steady_clock::time_point now = bc::steady_clock::now();

        // Получаем первый индекс -- по времени дедлайна
        typedef TaskList::nth_index<0>::type Index0;
        Index0& byDeadline = tasks.get<0>();
        // Проходим по всем элементам, чей таймаут уже наступил и сигнализируем об этом.
        Index0::iterator begin = byDeadline.begin();
        Index0::iterator it = begin;
        for (; it != byDeadline.end(); ++it) {
            if (it->deadline > now) {
                break;
            }
            // Сигнализируем зарегистрированным слушателем о том, что произошел таймаут.
            it->timeout();
        }
        // Удаляем завершенныts таймаутом задачи.
        byDeadline.erase(begin, it);

        // Если в очереди еще остались задачи, то у первой из них таймаут наступит
        // первым. До этого времени и будем ожидать.
        if (!byDeadline.empty()) {
            startWait(*byDeadline.begin());
        }
    }
    void startWait(const Task& task) {
        bs::error_code ec;
        timer.expires_at(task.deadline, ec);
        // Логгируем ошибку, если есть.
        log("timer.expires_at", ec);
        // Так как предыдущий вызов отменяет асинхронную операцию, то
        // начинаем ее заново.
        //timer.async_wait(boost::bind(&ServiceImpl::onTimeout, this));
    }
    static void log(const char* operation, const bs::error_code& ec) {
        if (!ec) {
            std::stringstream ss;
            ss << operation << ": " << ec;
            WFMOutputTraceData((LPSTR)ss.str().c_str());
        }
    }
};

#endif // PCSC_CENXFS_BRIDGE_ServiceImpl_H