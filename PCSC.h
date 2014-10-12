#ifndef PCSC_CENXFS_BRIDGE_PCSC_H
#define PCSC_CENXFS_BRIDGE_PCSC_H

#pragma once

#include <string>
#include <vector>
#include <map>

#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio/io_service.hpp>

// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include "PCSCStatus.h"

// Линкуемся с библиотекой реализации стандарта PC/SC в Windows
#pragma comment(lib, "winscard.lib")

class Service;
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
    /// Сервис ввода-вывода, диспетчерезующий события от подсистемы PC/SC об изменениях
    /// в надобе устройств и их состоянии.
    boost::asio::io_service ioService;
    /// Так как, как только у `ioService` не окажется ожидающих выполнения обработчиков
    /// таймаутов, он прекратит свою работу и его поток умрет, то необходимо предотвратить
    /// данный сценарий. Данный объект говорит `ioService`, что у него всегда есть какая-то
    /// работа, таким образом, поток никогда не умрет, пока сервис не будет остановлен явно.
    boost::asio::io_service::work work;
    /// Поток для выполнения ожидания изменения в оборудовании.
    boost::shared_ptr<boost::thread> waitThread;
public:
    /// Открывает соединение к менеджеру подсистемы PC/SC.
    PCSC();
    /// Закрывает соединение к менеджеру подсистемы PC/SC.
    ~PCSC();
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    inline bool isValid(HSERVICE hService) {
        return services.find(hService) != services.end();
    }
    Service& create(HSERVICE hService, const std::string& readerName);
    Service& get(HSERVICE hService);
    void remove(HSERVICE hService);
public:// Подписка на события и генерация событий
    /** Добавляет указанное окно к подписчикам на указанные события от указанного сервиса.
    @return `false`, если указанный `hService` не зарегистрирован в объекте, иначе `true`.
    */
    bool addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass);
    bool removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass);
private:
    /// Блокирует выполнение, пока поток не будет остановлен. Необходимо
    /// запускать из своего потока.
    void run();
    void getReadersAndWaitChanges();
    /** Данная блокирует выполнение до тех пор, пока не получит событие об изменении состояния
        физических устройств, поэтому она должна вызываться в отдельном потоке. После наступления
        события она отсылает соответствующие события всем заинтересованным слушателям подсистемы XFS.
    @param readers Отслеживаемые считыватели. Первый элемент отслеживает изменения устройств.
    @return `true`, если произошли изменения в количестве считывателей, `false` иначе.
    */
    bool waitChanges(std::vector<SCARD_READERSTATE>& readers);
    void notifyChanges(SCARD_READERSTATE& state) const;
private:
    static void log(std::string operation, Status st);
};

#endif // PCSC_CENXFS_BRIDGE_PCSC_H