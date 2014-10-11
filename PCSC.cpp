#include "PCSC.h"

#include <sstream>
#include <cassert>

#include "Service.h"
#include "PCSCReaderState.h"

/// ��������� ���������� � ��������� ���������� PC/SC.
PCSC::PCSC() {
    // ������� ��������.
    Status st = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    log("SCardEstablishContext", st);
    // ��������� ����� �������� ��������� �� ������� PC/SC -- ���������� � ��������
    // ������������ � ��������.
    waitThread.reset(new boost::thread(&PCSC::run, this));
}
/// ��������� ���������� � ��������� ���������� PC/SC.
PCSC::~PCSC() {
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
        assert(it->second != NULL);
        delete it->second;
    }
    services.clear();
    Status st = SCardReleaseContext(hContext);
    log("SCardReleaseContext", st);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Service& PCSC::create(HSERVICE hService, const std::string& readerName) {
    Service* service = new Service(hService, readerName);
    services.insert(std::make_pair(hService, service));
    return *service;
}
Service& PCSC::get(HSERVICE hService) {
    assert(isValid(hService));
    Service* service = services.find(hService)->second;
    assert(service != NULL);
    return *service;
}
void PCSC::remove(HSERVICE hService) {
    assert(isValid(hService));
    ServiceMap::iterator it = services.find(hService);

    assert(it->second != NULL);
    // �������� PC/SC ���������� ���������� � ����������� Card.
    delete it->second;
    services.erase(it);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool PCSC::addSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
    ServiceMap::iterator it = services.find(hService);
    if (it == services.end()) {
        return false;
    }
    assert(it->second != NULL);
    it->second->add(hWndReg, dwEventClass);
    return true;
}
bool PCSC::removeSubscriber(HSERVICE hService, HWND hWndReg, DWORD dwEventClass) {
    ServiceMap::iterator it = services.find(hService);
    if (it == services.end()) {
        return false;
    }
    assert(it->second != NULL);
    it->second->remove(hWndReg, dwEventClass);
    return true;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PCSC::run() {
    WFMOutputTraceData("Dispatch thread runned");
    do {
        getReadersAndWaitChanges();
    } while (true);
    WFMOutputTraceData("Dispatch thread stopped");
}
void PCSC::getReadersAndWaitChanges() {
    DWORD readersCount;
    // ���������� ��������� �����������: ������� ����������, ����� ���� �����������.
    Status st = SCardListReaders(hContext, NULL, NULL, &readersCount);
    log("SCardListReaders(get readers count)", st);

    std::vector<char> readerNames(readersCount);
    st = SCardListReaders(hContext, NULL, &readerNames[0], &readersCount);
    log("SCardListReaders(get readers)", st);

    std::vector<SCARD_READERSTATE> readers(1+readersCount);
    // ����������� �� ����������� ������, ����������, ��� ���������� ����������
    // ���������/������� ������������.
    readers[0].szReader = "\\\\?PnP?\\Notification";
    readers[0].dwCurrentState = SCARD_STATE_UNAWARE;

    // ��������� ��������� ��� �������� ������� �� ��������� ������������.
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
    // ������� ������� �� ������������. ���� �� ���������� ����������,
    // �� ���������� ��������. ��������� ���� � ������ ��������� ��������
    // �� ��������� ����� ����� � run.
    while (!waitChanges(readers));
}
bool PCSC::waitChanges(std::vector<SCARD_READERSTATE>& readers) {
    // ������ ������� ��������� ���������� �� ��� ���, ���� �� ���������� �������.
    // ���� ��� �� �������������.
    Status st = SCardGetStatusChange(hContext, INFINITE, &readers[0], (DWORD)readers.size());
    log("SCardGetStatusChange", st);
    bool readersChanged = false;
    bool first = true;
    for (std::vector<SCARD_READERSTATE>::iterator it = readers.begin(); it != readers.end(); ++it) {
        // C������� PC/SC, ��� �� ����� ������� ���������
        it->dwCurrentState = it->dwEventState;
        // ���� ���-�� ����������, ���������� �� ���� ���� ����������������.
        if (it->dwEventState & SCARD_STATE_CHANGED) {
            // ������ ������� � ������ -- ������, ����� ������� ��������
            // ����������� �� ���������� ����� ���������.
            if (first) {
                readersChanged = true;
            }
            notifyChanges(*it);
        }
        first = false;
    }
    return readersChanged;
}
void PCSC::notifyChanges(SCARD_READERSTATE& state) const {
    for (ServiceMap::const_iterator it = services.begin(); it != services.end(); ++it) {
        it->second->notify(state);
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PCSC::log(std::string operation, Status st) {
    std::stringstream ss;
    ss << operation << ": " << st;
    WFMOutputTraceData((LPSTR)ss.str().c_str());
}
