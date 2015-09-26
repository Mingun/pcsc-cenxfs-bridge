#ifndef PCSC_CENXFS_BRIDGE_ServiceContainer_H
#define PCSC_CENXFS_BRIDGE_ServiceContainer_H

#pragma once

#include <map>

// PC/CS API -- для SCARD_READERSTATE
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC)) -- для HSERVICE
#include <XFSIDC.h>

class Manager;
class Service;
class Settings;
class ServiceContainer {
    /// Тип для отображения сервисов XFS на карты PC/SC.
    typedef std::map<HSERVICE, Service*> ServiceMap;
private:
    /// Список карт, открытых для взаимодействия с системой XFS.
    ServiceMap services;
public:
    ~ServiceContainer();
public:
    /** Проверяет, что указаный хендл сервиса является корректным хендлом карточки. */
    inline bool isValid(HSERVICE hService) const {
        return services.find(hService) != services.end();
    }
    /** @return true, если в контейнере не зарегистрировано ни одного сервиса. */
    inline bool isEmpty() const { return services.empty(); }

    Service& create(Manager& manager, HSERVICE hService, const Settings& settings);
    Service& get(HSERVICE hService);
    void remove(HSERVICE hService);
public:// Подписка на события и генерация событий
    /** Добавляет указанное окно к подписчикам на указанные события от указанного сервиса.
    @return `false`, если указанный `hService` не зарегистрирован в объекте, иначе `true`.
    */
    bool addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass);
    bool removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass);
public:
    /// Уведомляет все сервисы о произошедших изменениях со считывателями.
    void notifyChanges(const SCARD_READERSTATE& state, bool deviceChange);
};

#endif // PCSC_CENXFS_BRIDGE_ServiceContainer_H