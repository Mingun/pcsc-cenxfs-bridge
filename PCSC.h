#ifndef PCSC_CENXFS_BRIDGE_PCSC_H
#define PCSC_CENXFS_BRIDGE_PCSC_H

#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <cassert>

// CEN/XFS API
#include <xfsspi.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include "Card.h"
#include "PCSCStatus.h"
#include "ReaderState.h"

/** Класс, в конструкторе инициализирующий подсистему PC/SC, а в деструкторе закрывающий ее.
    Необходимо Создать ровно один экземпляр данного класса при загрузке DLL и уничтожить его
    при выгрузке. Наиболее просто это делается, путем объявления глобальной переменной данного
    класса.
*/
class PCSC {
public:
    /// Тип для отображения сервисов XFS на карты PC/SC.
    typedef std::map<HSERVICE, Card*> CardMap;
public:
    /// Контекст подсистемы PC/SC.
    SCARDCONTEXT hContext;
    /// Список карт, открытых для взаимодействия с системой XFS.
    CardMap cards;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    PCSC() {
        // Создаем контекст.
        Status st = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
        log("SCardEstablishContext", st);
    }
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~PCSC() {
        for (CardMap::const_iterator it = cards.begin(); it != cards.end(); ++it) {
            assert(it->second != NULL);
            delete it->second;
        }
        cards.clear();
        Status st = SCardReleaseContext(hContext);
        log("SCardReleaseContext", st);
    }
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    bool isValid(HSERVICE hService) {
        return cards.find(hService) != cards.end();
    }
    Card& open(HSERVICE hService, const char* name) {
        // Конструктор устанавливает соединение с подсистемой PC/SC.
        Card* card = new Card(hService, hContext, name);
        cards.insert(std::make_pair(hService, card));
        return *card;
    }
    Card& get(HSERVICE hService) {
        assert(isValid(hService));
        Card* card = cards.find(hService)->second;
        assert(card != NULL);
        return *card;
    }
    void close(HSERVICE hService) {
        assert(isValid(hService));
        CardMap::iterator it = cards.find(hService);

        assert(it->second != NULL);
        // Закрытие PC/SC соединения происхоидт в деструкторе Card.
        delete it->second;
        cards.erase(it);
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

        for (std::vector<char>::const_iterator it = readerNames.begin(); it != readerNames.end(); ++it) {
            //it
        }
    }
    /** Данная блокирует выполнение до тех пор, пока не получит событие об изменении состояния
        физических устройств, поэтому она должна вызываться в отдельном потоке. После наступления
        события она отсылает соответствующие события всем заинтересованным слушателям подсистемы XFS.
    */
    void waitChanges(std::vector<SCARD_READERSTATE>& readers) {
        // Данная функция блокирует выполнение до тех пор, пока не произойдет событие.
        // Ждем его до бесконечности.
        Status st = SCardGetStatusChange(hContext, INFINITE, &readers[0], readers.size());
        log("SCardGetStatusChange", st);

        if (!st) {
            return;
        }
        for (std::vector<SCARD_READERSTATE>::const_iterator it = readers.begin(); it != readers.end(); ++it) {
            WFSDEVSTATUS* status = xfsAlloc<WFSDEVSTATUS>();
            // Имя физичеcкого устройства, чье состояние изменилось
            strcpy(status->lpszPhysicalName, it->szReader);
            // Рабочая станция, на которой запущен сервис.
            status->lpszWorkstationName = NULL;//TODO: Заполнить имя рабочей станции.
            status->dwState = ReaderState(it->dwEventState).translate();

            //Result(0, hService, 0).data(status).send();
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