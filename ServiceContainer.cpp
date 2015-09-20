#include "ServiceContainer.h"

#include "Service.h"
#include "Settings.h"

#include "XFS/Logger.h"

#include <cassert>

ServiceContainer::~ServiceContainer() {
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
        assert(it->second != NULL && "Internal error: no service data while cleanup");
        delete it->second;
    }
    services.clear();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Service& ServiceContainer::create(Manager& manager, HSERVICE hService, const Settings& settings) {
    assert(!isValid(hService) && "Try to create already registered service");
    Service* service = new Service(manager, hService, settings);
    services.insert(std::make_pair(hService, service));
    return *service;
}
Service& ServiceContainer::get(HSERVICE hService) {
    assert(isValid(hService) && "Try to get not registered service");
    Service* service = services.find(hService)->second;
    assert(service != NULL && "Internal error: no service data for valid service handle while get service");
    return *service;
}
void ServiceContainer::remove(HSERVICE hService) {
    assert(isValid(hService) && "Try to remove not registered service");
    ServiceMap::iterator it = services.find(hService);

    assert(it->second != NULL && "Internal error: no service data for valid service handle while remove service");
    // Закрытие PC/SC соединения происходит в деструкторе Service.
    delete it->second;
    services.erase(it);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool ServiceContainer::addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
    ServiceMap::iterator it = services.find(hService);
    if (it == services.end()) {
        return false;
    }
    assert(it->second != NULL && "Internal error: no service data for valid service handle while add subscribe");
    it->second->add(hWndReg, dwEventClass);
    return true;
}
bool ServiceContainer::removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
    ServiceMap::iterator it = services.find(hService);
    if (it == services.end()) {
        return false;
    }
    assert(it->second != NULL && "Internal error: no service data for valid service handle while remove subscribe");
    it->second->remove(hWndReg, dwEventClass);
    return true;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ServiceContainer::notifyChanges(const SCARD_READERSTATE& state) {
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
        assert(it->second != NULL && "Internal error: no service data while do notification");
        it->second->notify(state);
    }
}