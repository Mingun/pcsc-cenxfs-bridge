#ifndef PCSC_CENXFS_BRIDGE_PCSC_H
#define PCSC_CENXFS_BRIDGE_PCSC_H

#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <cassert>

// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include "Service.h"
#include "PCSCStatus.h"
#include "PCSCReaderState.h"

// Линкуемся с библиотекой реализации стандарта PC/SC в Windows
#pragma comment(lib, "winscard.lib")

/** Класс, в конструкторе инициализирующий подсистему PC/SC, а в деструкторе закрывающий ее.
    Необходимо Создать ровно один экземпляр данного класса при загрузке DLL и уничтожить его
    при выгрузке. Наиболее просто это делается, путем объявления глобальной переменной данного
    класса.
*/
class PCSC {
public:
    /// Тип для отображения сервисов XFS на карты PC/SC.
    typedef std::map<HSERVICE, Service*> ServiceMap;
public:
    /// Контекст подсистемы PC/SC.
    SCARDCONTEXT hContext;
    /// Список карт, открытых для взаимодействия с системой XFS.
    ServiceMap services;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    PCSC() {
        // Создаем контекст.
        Status st = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
        log("SCardEstablishContext", st);
    }
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~PCSC() {
        for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
            assert(it->second != NULL);
            delete it->second;
        }
        services.clear();
        Status st = SCardReleaseContext(hContext);
        log("SCardReleaseContext", st);
    }
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    bool isValid(HSERVICE hService) {
        return services.find(hService) != services.end();
    }
    Service& create(HSERVICE hService) {
        Service* service = new Service(hService);
        services.insert(std::make_pair(hService, service));
        return *service;
    }
    Service& get(HSERVICE hService) {
        assert(isValid(hService));
        Service* service = services.find(hService)->second;
        assert(service != NULL);
        return *service;
    }
    void remove(HSERVICE hService) {
        assert(isValid(hService));
        ServiceMap::iterator it = services.find(hService);

        assert(it->second != NULL);
        // Закрытие PC/SC соединения происхоидт в деструкторе Card.
        delete it->second;
        services.erase(it);
    }
public:// Подписка на события и генерация событий
    /** Добавляет указанное окно к подписчикам на указанные события от указанного сервиса.
    @return `false`, если указанный `hService` не зарегистрирован в объекте, иначе `true`.
    */
    bool addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
        ServiceMap::iterator it = services.find(hService);
        if (it == services.end()) {
            return false;
        }
        assert(it->second != NULL);
        it->second->add(hWndReg, dwEventClass);
        return true;
    }
    bool removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
        ServiceMap::iterator it = services.find(hService);
        if (it == services.end()) {
            return false;
        }
        assert(it->second != NULL);
        it->second->remove(hWndReg, dwEventClass);
        return true;
    }
private:
    void prepareReadersState() {
        DWORD readersCount;
        // Определяем доступные считыватели: сначало количество, затем сами считыватели.
        Status st = SCardListReaders(hContext, NULL, NULL, &readersCount);
        log("SCardListReaders(get readers count)", st);

        std::vector<char> readerNames(readersCount);
        st = SCardListReaders(hContext, NULL, &readerNames[0], &readersCount);
        log("SCardListReaders(get readers)", st);

        std::vector<SCARD_READERSTATE> readers(1+readersCount);
        // Считыватель со специальным именем, означающем, что необходимо мониторить
        // появление/пропажу считывателей.
        readers[0].szReader = "\\\\?PnP?\\Notification";
        readers[0].dwCurrentState = SCARD_STATE_UNAWARE;

        std::size_t i = 0;
        std::vector<char>::const_iterator begin = readerNames.begin();
        for (std::vector<char>::const_iterator it = begin; it != readerNames.end() && i < readersCount;) {
            if (*it == '\0') {
                readers[1 + i++].szReader = static_cast<LPCSTR>(&*begin);
                begin = ++it;
                continue;
            }
            ++it;
        }
        waitChanges(readers);
    }
    /** Данная блокирует выполнение до тех пор, пока не получит событие об изменении состояния
        физических устройств, поэтому она должна вызываться в отдельном потоке. После наступления
        события она отсылает соответствующие события всем заинтересованным слушателям подсистемы XFS.
    */
    void waitChanges(std::vector<SCARD_READERSTATE>& readers) {
        // Данная функция блокирует выполнение до тех пор, пока не произойдет событие.
        // Ждем его до бесконечности.
        Status st = SCardGetStatusChange(hContext, INFINITE, &readers[0], (DWORD)readers.size());
        log("SCardGetStatusChange", st);
        for (std::vector<SCARD_READERSTATE>::iterator it = readers.begin(); it != readers.end(); ++it) {
            // Cообщаем PC/SC, что мы знаем текущее состояние
            it->dwCurrentState = it->dwEventState;
            // Если что-то изменилось, уведомляем об этом всех заинтересованных.
            if (it->dwEventState & SCARD_STATE_CHANGED) {
                notifyChanges(*it);
            }
        }
    }
    void notifyChanges(SCARD_READERSTATE& state) const {
        DWORD dwState = state.dwCurrentState;
        for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
            it->second->notify(state);
        }
    }
private:
    static void log(std::string operation, Status st) {
        std::stringstream ss;
        ss << operation << ": " << st;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
};

#endif // PCSC_CENXFS_BRIDGE_PCSC_H