#include "Service.h"

#include "Utils.h"
#include "PCSCMediaStatus.h"
#include "PCSCReaderState.h"

#include <sstream>
#include <cassert>

/// �������, ��������� ��������� ����������� � ������� ����� ������� ����������������� ���������.
class CardInserted {
    HSERVICE hService;
public:
    CardInserted(HSERVICE hService) : hService(hService) {}
    Result operator()() const {
        return Result(0, hService, WFS_SUCCESS).cardInserted();
    }
};
/// �������, ��������� ��������� ����������� � �������� ����� ������� ����������������� ���������.
class CardRemoved {
    HSERVICE hService;
public:
    CardRemoved(HSERVICE hService) : hService(hService) {}
    Result operator()() const {
        return Result(0, hService, WFS_SUCCESS).cardRemoved();
    }
};
/// �������, ��������� ��������� ����������� � ��������� ������ ���������� ������� ����������������� ���������.
class DeviceDetected {
    HSERVICE hService;
    SCARD_READERSTATE& state;
public:
    DeviceDetected(HSERVICE hService, SCARD_READERSTATE& state) : hService(hService), state(state) {}
    Result operator()() const {
        WFSDEVSTATUS* status = xfsAlloc<WFSDEVSTATUS>();
        // ��� ������c���� ����������, ��� ��������� ����������
        strcpy(status->lpszPhysicalName, state.szReader);
        // ������� �������, �� ������� ������� ������.
        status->lpszWorkstationName = NULL;//TODO: ��������� ��� ������� �������.
        status->dwState = ReaderState(state.dwEventState).translate();
        return Result(0, hService, WFS_SUCCESS).data(status);
    }
};

Status Service::open(SCARDCONTEXT hContext) {
    assert(hCard == 0 && "Must open only one card at one service");
    Status st = SCardConnect(hContext, readerName.c_str(), SCARD_SHARE_SHARED,
        // � ��� ��� ��������������� ���������, �������� � ���, ��� ����
        SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        // �������� ����� ����� � ��������� ��������.
        &hCard, &dwActiveProtocol
    );
    log("SCardConnect", st);
    return st;
}
Status Service::close() {
    assert(hCard != 0 && "Attempt disconnect from non-connected card");
    // ��� �������� ���������� ������ �� ������ � ���������, ��������� �� � �����������.
    Status st = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    log("SCardDisconnect", st);
    hCard = 0;
    return st;
}

Status Service::lock() {
    Status st = SCardBeginTransaction(hCard);
    log("SCardBeginTransaction", st);

    return st;
}
Status Service::unlock() {
    // ����������� ����������, ������ �� ������ � ������.
    Status st = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
    log("SCardEndTransaction", st);

    return st;
}
/// ���������� ���� ���������� ��� ���� ������������ ���������� �� �������������.
void Service::notify(SCARD_READERSTATE& state) const {
    /*if (state.dwEventState & SCARD_STATE_) {
        EventNotifier::notify(WFS_SYSTEM_EVENT, DeviceDetected(hService, state));
    }*/
    if (state.dwEventState & SCARD_STATE_EMPTY) {
        EventNotifier::notify(WFS_SERVICE_EVENT, CardRemoved(hService));
    }
    if (state.dwEventState & SCARD_STATE_PRESENT) {
        EventNotifier::notify(WFS_EXECUTE_EVENT, CardInserted(hService));
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::pair<WFSIDCSTATUS*, Status> Service::getStatus() {
    // ��������� �����������.
    MediaStatus state;
    DWORD nameLen;
    DWORD atrLen;
    Status st = SCardStatus(hCard,
        // ��� �������� �� �����, ��� �� ����� ����� �������� ���������, NULL ����������.
        NULL, &nameLen,
        // ��������� ��� ��������, � ��� ���������� �������, ������ �������.
        (DWORD*)&state, &dwActiveProtocol,
        // ATR �������� �� �����, ��� �� ����� ����� �������� ���������, NULL ����������.
        NULL, &atrLen
    );
    log("SCardStatus: ", st);

    WFSIDCSTATUS* lpStatus = xfsAlloc<WFSIDCSTATUS>();
    // ����� ������, ������������ ��������� ����������. ���� ���������� ������ �� �����,
    // �.�. � ��������� ������ ��� �������� ����� � PC/SC ��������� ����� ������.
    lpStatus->fwDevice = WFS_IDC_DEVONLINE;
    lpStatus->fwMedia = st ? state.translateMedia() : WFS_IDC_MEDIANOTPRESENT;
    // � ������������ ��� ������� ��� ����������� ����.
    lpStatus->fwRetainBin = WFS_IDC_RETAINNOTSUPP;
    // ������ ����������� �����������
    lpStatus->fwSecurity = WFS_IDC_SECNOTSUPP;
    // ���������� ������������ ����. ��� ��� ����� ����������� � ������������� ������,
    // �� ������ �������� �� ����� ������.
    //TODO ����, ����� ����, ����� ����� ��� ����������� ��� ���������� ���������� ����.
    lpStatus->usCards = 0;
    lpStatus->fwChipPower = st ? state.translateChipPower() : WFS_IDC_CHIPNOCARD;
    //TODO: �������� lpszExtra �� ����� �����������, ����������� �� PC/SC.
    return std::make_pair(lpStatus, st);
}
std::pair<WFSIDCCAPS*, Status> Service::getCaps() {
    WFSIDCCAPS* lpCaps = xfsAlloc<WFSIDCCAPS>();

    // �������� �������������� ������ ���������.
    DWORD types;
    DWORD len = sizeof(DWORD);
    Status st = SCardGetAttrib(hCard, SCARD_ATTR_PROTOCOL_TYPES, (BYTE*)&types, &len);
    log("SCardGetAttrib(SCARD_ATTR_PROTOCOL_TYPES)", st);

    // ���������� �������� ������������ ����.
    lpCaps->wClass = WFS_SERVICE_CLASS_IDC;
    // ����� ����������� ����� � ����� ���� �������� � ����� ������.
    lpCaps->fwType = WFS_IDC_TYPEDIP;
    // ���������� �������� ������ ����������� ����.
    lpCaps->bCompound = FALSE;
    // ����� ����� ����� ���� ��������� -- �������, ������ ���.
    lpCaps->fwReadTracks = WFS_IDC_NOTSUPP;
    // ����� ����� ����� ���� �������� -- �������, ������ ���.
    lpCaps->fwWriteTracks = WFS_IDC_NOTSUPP;
    // ���� �������������� ����������� ���������� -- ��� ���������.
    lpCaps->fwChipProtocols = WFS_IDC_CHIPT0 | WFS_IDC_CHIPT1;
    // ������������ ���������� ����, ������� ���������� ����� ���������. ������ 0, �.�. �� �����������.
    lpCaps->usCards = 0;
    // ��� ������ ������������. �� ��������������.
    lpCaps->fwSecType = WFS_IDC_SECNOTSUPP;
    // ������ ��� ���� ����� ������ ��� ���������������� ������������, �� �� ������������.
    lpCaps->fwPowerOnOption = WFS_IDC_NOACTION;
    lpCaps->fwPowerOffOption = WFS_IDC_NOACTION;
    // ����������� ���������������� Flux Sensor. �� ������ ������ ������, ��� �� �����.
    //TODO: ��� ����� Flux Sensor?
    lpCaps->bFluxSensorProgrammable = FALSE;
    // ����� �� ������ ������/������ �� �����, ����� ��� ��� ���� �������� (��� ���� ��� ������������
    // �������). ��� ��� � ��� �� ���������������� �����������, �� �� ����� �� �����.
    lpCaps->bReadWriteAccessFollowingEject = FALSE;
    // �������� ��������� ����, ������� ���������� ��������� ��� ��������������� �������������
    // �������. ��� ��� ������/������ ������ �� ��������������, �� �� ������������.
    lpCaps->fwWriteMode = WFS_IDC_NOTSUPP;
    // ����������� ����������� �� ���������� �������� ����.
    //TODO: �������� �������� ����������� �����������. ���� ������������, ��� ��� ����������� ����.
    lpCaps->fwChipPower = WFS_IDC_CHIPPOWERCOLD | WFS_IDC_CHIPPOWERWARM | WFS_IDC_CHIPPOWEROFF;
    //TODO: �������� lpszExtra �� ����� �����������, ����������� �� PC/SC.
    return std::make_pair(lpCaps, st);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Service::asyncRead(DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID) {
    
}
std::pair<WFSIDCCARDDATA*, Status> Service::read() {
    WFSIDCCARDDATA* data = xfsAlloc<WFSIDCCARDDATA>();
    // data->lpbData �������� ATR (Answer To Reset), ����������� � ����
    data->wDataSource = WFS_IDC_CHIP;
    //TODO: ������ ����������� ������ ���������� ���������� � ������������ �� ��������,
    // ������� ������� SCardGetAttrib.
    data->wStatus = WFS_IDC_DATAOK;
    // �������� ATR (Answer To Reset).
    Status st = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, data->lpbData, &data->ulDataLength);
    return std::make_pair(data, st);
}
std::pair<WFSIDCCHIPIO*, Status> Service::transmit(WFSIDCCHIPIO* input) {
    assert(input != NULL);

    WFSIDCCHIPIO* result = xfsAlloc<WFSIDCCHIPIO>();
    result->wChipProtocol = input->wChipProtocol;

    SCARD_IO_REQUEST ioRq = {0};//TODO: �������� ��������.
    Status st = SCardTransmit(hCard,
        &ioRq, input->lpbChipData, input->ulChipDataLength,
        NULL, result->lpbChipData, &result->ulChipDataLength
    );
    log("SCardTransmit", st);

    return std::make_pair(result, st);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Service::log(std::string operation, Status st) const {
    std::stringstream ss;
    ss << operation << " card reader '" << readerName << "': " << st;
    WFMOutputTraceData((LPSTR)ss.str().c_str());
}