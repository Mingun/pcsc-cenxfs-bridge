#include "Manager.h"

#include "Service.h"
#include "Settings.h"

#include "XFS/Logger.h"

Manager::Manager() : readerChangesMonitor(*this) {}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Service& Manager::create(HSERVICE hService, const Settings& settings) {
    Service& result = services.create(*this, hService, settings);
    // Прерываем ожидание потока на SCardGetStatusChange, т.к. необходимо доставить
    // новому сервису информацию о всех существующих в данный момент считывателях.
    readerChangesMonitor.cancel("Manager::create");
    return result;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Manager::notifyChanges(const SCARD_READERSTATE& state, bool deviceChange) {
    // Сначала уведомляем подписанных слушателей об изменениях, и только затем
    // пытаемся завершить задачи.
    services.notifyChanges(state, deviceChange);
    tasks.notifyChanges(state, deviceChange);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Manager::addTask(const Task::Ptr& task) {
    if (tasks.addTask(task)) {
        // Прерываем ожидание потока на SCardGetStatusChange, т.к. ожидать теперь нужно
        // до нового таймаута. Ожидание с новым таймаутом начнется автоматически.
        readerChangesMonitor.cancel("Manager::addTask");
    }
}
bool Manager::cancelTask(HSERVICE hService, REQUESTID ReqID) {
    if (tasks.cancelTask(hService, ReqID)) {
        // Прерываем ожидание потока на SCardGetStatusChange, т.к. ожидать теперь нужно
        // до нового таймаута. Ожидание с новым таймаутом начнется автоматически.
        readerChangesMonitor.cancel("Manager::cancelTask");
        return true;
    }
    return false;
}