#include "Manager.h"

#include "Service.h"
#include "Settings.h"

#include "XFS/Logger.h"

Manager::Manager() : readerChangesMonitor(*this) {}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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