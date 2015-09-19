#include "Task.h"

#include <sstream>
#include <cassert>

#include "Service.h"
#include "XFS/Result.h"

void Task::complete(HRESULT result) const {
    XFS::Result(ReqID, serviceHandle(), result).attach((WFSIDCCARDDATA**)0).send(hWnd, WFS_EXECUTE_COMPLETE);
}
HSERVICE Task::serviceHandle() const {
    return mService.handle();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool TaskContainer::addTask(const Task::Ptr& task) {
    boost::lock_guard<boost::recursive_mutex> lock(tasksMutex);

    std::pair<TaskList::iterator, bool> r = tasks.insert(task);
    // Вставляться должны уникальные по ReqID элементы.
    assert(r.second == true && "[TaskContainer::addTask] Adding task with existing <ServiceHandle, ReqID>");
    assert(!tasks.empty() &&  "[TaskContainer::addTask] Task insertion was not performed");
    // Получаем первый индекс -- по времени дедлайна
    typedef TaskList::nth_index<0>::type Index0;

    Index0& byDeadline = tasks.get<0>();
    return *byDeadline.begin() == task;
}
bool TaskContainer::cancelTask(HSERVICE hService, REQUESTID ReqID) {
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
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
DWORD TaskContainer::getTimeout() const {
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
void TaskContainer::processTimeouts(bc::steady_clock::time_point now) {
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
void TaskContainer::notifyChanges(const SCARD_READERSTATE& state) {
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