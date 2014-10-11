#ifndef PCSC_CENXFS_BRIDGE_ServiceImpl_H
#define PCSC_CENXFS_BRIDGE_ServiceImpl_H

#pragma once

#include <sstream>
#include <cassert>

// ��������� ��� �������� ����� �� ������ ��������.
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
    /// �����, ����� �������� ������� ��� ������ ������.
    bc::steady_clock::time_point deadline;
    /// ����, ������� ������� ����������� � ���������� ������.
    HWND hWnd;
    /// ����������� ����� ������ ������, ������� ����� ������������ � ����������� ���� `hWnd`.
    /// ������ ���� �������� ��� ������ ������.
    REQUESTID ReqID;
    /// ������, ������� ������ ��� ������.
    Service& service;
public:
    Task(bc::steady_clock::time_point deadline, HWND hWnd, REQUESTID ReqID, Service& service)
        : deadline(deadline), hWnd(hWnd), ReqID(ReqID), service(service) {}
    /// ���� �������� ������������ ������ ��� �� ��������������� �������������,
    /// ������-�� ����� �� ���������. ��������, ��-�� ������� ������ � �������� ����?
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
    /// ����������, ����� �������� ���� ��������� � ������ ���� ���������.
    void done() const {
        std::pair<WFSIDCCARDDATA*, Status> r = service.read();
        Result(ReqID, service.handle(), r.second).data(r.first).send(hWnd, WFS_EXECUTE_COMPLETE);
    }
    /// ����������, ���� ������ ��� ������� ������� WFPCancelAsyncRequest.
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
            // ���������� �� CardWaitTask::operator< -- �� ������� ��������, ��� ������
            // �����, ��� ������� �������.
            mi::ordered_non_unique<mi::identity<Task> >,
            // ���������� �� less<REQUESTID> �� ReqID -- ��� �������� ���������� �����.
            mi::ordered_unique<mi::member<Task, REQUESTID, &Task::ReqID> >
        >
    > TaskList;
private:
    /// ������ �����, ������������ �� ����������� ������� ��������.
    /// ��� ������ �������, ��� ����� ������ � ������ ������.
    TaskList tasks;
    ba::steady_timer timer;
    Service& service;
public:
    ServiceImpl(ba::io_service& ioService, Service& service)
        : timer(ioService), service(service) {}
    void addTask(const Task& task) {
        if (addTaskImpl(task)) {
            // ��������� ���� ������ ��������� ������ � �������, �� �������� �������
            // �� �� ����������� �����.
            startWait(task);
        }
    }
    bool cancelTask(REQUESTID ReqID) {
        // �������� ������ ������ -- �� ������������ ������
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
    /// ��������� ������ � �������, ���������� `true`, ���� ������ ������ � �������
    /// � ������� ���������� ���������������, ����� �� ���������� ������� ������ ������.
    bool addTaskImpl(const Task& task) {
        std::pair<TaskList::iterator, bool> r = tasks.insert(task);
        // ����������� ������ ���������� �� ReqID ��������.
        assert(r.second == false);
        assert(!tasks.empty());
        // �������� ������ ������ -- �� ������� ��������
        typedef TaskList::nth_index<0>::type Index0;
        Index0& byDeadline = tasks.get<0>();
        return *byDeadline.begin() == task;
    }

    /// ����������, ���������� ����� ��������� 
    void onTimeout(const bs::error_code& ec) {
        log("onTimeout", ec);
        // �� ����� ������ �������� � ������� ������ ���� ��� ������� ���� ������ --
        // ���� ��� ����-�� ������� ������ ���� ������.
        assert(!tasks.empty());
        // ���� �������� ���� ��������
        if (ec == ba::error::operation_aborted) {

        }
        // ��������� ��� ������, ��� ������� ��������. ��� ��� ��� ������ ����������� ��
        // ������� �������� (��� ������ �������, ��� ����� � ������ �������), �� ������
        // ������, ������� ������� ��� �� ��������, ��������� ������� ���������.
        bc::steady_clock::time_point now = bc::steady_clock::now();

        // �������� ������ ������ -- �� ������� ��������
        typedef TaskList::nth_index<0>::type Index0;
        Index0& byDeadline = tasks.get<0>();
        // �������� �� ���� ���������, ��� ������� ��� �������� � ������������� �� ����.
        Index0::iterator begin = byDeadline.begin();
        Index0::iterator it = begin;
        for (; it != byDeadline.end(); ++it) {
            if (it->deadline > now) {
                break;
            }
            // ������������� ������������������ ���������� � ���, ��� ��������� �������.
            it->timeout();
        }
        // ������� ����������ts ��������� ������.
        byDeadline.erase(begin, it);

        // ���� � ������� ��� �������� ������, �� � ������ �� ��� ������� ��������
        // ������. �� ����� ������� � ����� �������.
        if (!byDeadline.empty()) {
            startWait(*byDeadline.begin());
        }
    }
    void startWait(const Task& task) {
        bs::error_code ec;
        timer.expires_at(task.deadline, ec);
        // ��������� ������, ���� ����.
        log("timer.expires_at", ec);
        // ��� ��� ���������� ����� �������� ����������� ��������, ��
        // �������� �� ������.
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