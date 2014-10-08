#ifndef PCSC_CENXFS_BRIDGE_Result_H
#define PCSC_CENXFS_BRIDGE_Result_H

#pragma once

#include <cassert>

// Для REQUESTID, HSERVICE, HRESULT, LONG и DWORD
#include <windef.h>
// Для PostMessage
#include <winuser.h>
// Для GetSystemTime
#include <WinBase.h>
// Определения для ридеров карт (Identification card unit (IDC)), для WFMAllocateBuffer и WFSRESULT
#include <XFSIDC.h>

#include "Utils.h"
#include "PCSCStatus.h"

class MsgType : public Enum<DWORD, MsgType> {
    typedef Enum<DWORD, MsgType> _Base;
public:
    MsgType(DWORD value) : _Base(value) {}

    CTString name() const {
        static CTString names1[] = {
            CTString("WFS_OPEN_COMPLETE"      ), // (WM_USER + 1)
            CTString("WFS_CLOSE_COMPLETE"     ), // (WM_USER + 2)
            CTString("WFS_LOCK_COMPLETE"      ), // (WM_USER + 3)
            CTString("WFS_UNLOCK_COMPLETE"    ), // (WM_USER + 4)
            CTString("WFS_REGISTER_COMPLETE"  ), // (WM_USER + 5)
            CTString("WFS_DEREGISTER_COMPLETE"), // (WM_USER + 6)
            CTString("WFS_GETINFO_COMPLETE"   ), // (WM_USER + 7)
            CTString("WFS_EXECUTE_COMPLETE"   ), // (WM_USER + 8)
        };
        static CTString names2[] = {
            CTString("WFS_EXECUTE_EVENT"      ), // (WM_USER + 20)
            CTString("WFS_SERVICE_EVENT"      ), // (WM_USER + 21)
            CTString("WFS_USER_EVENT"         ), // (WM_USER + 22)
            CTString("WFS_SYSTEM_EVENT"       ), // (WM_USER + 23)
        };
        static CTString names3[] = {
            CTString("WFS_TIMER_EVENT"        ), // (WM_USER + 100)
        };
        CTString result;
        if (_Base::name(names1, result)) {
            return result;
        }
        if (_Base::name(names2, result)) {
            return result;
        }
        return _Base::name(names3);
    }
};

class Result {
    WFSRESULT* pResult;
public:
    Result(REQUESTID ReqID, HSERVICE hService, Status result) {
        init(ReqID, hService, result.translate());
    }
    Result(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
        init(ReqID, hService, result);
    }
public:// Заполнение результатов команд WFPGetInfo
    /// Прикрепляет к результату указанные данные статуса.
    inline Result& data(WFSIDCSTATUS* data) {
        assert(pResult != NULL);
        pResult->u.dwCommandCode = WFS_INF_IDC_STATUS;
        pResult->lpBuffer = data;
        return *this;
    }
    /// Прикрепляет к результату указанные данные возможностей устройства.
    inline Result& data(WFSIDCCAPS* data) {
        assert(pResult != NULL);
        pResult->u.dwCommandCode = WFS_INF_IDC_CAPABILITIES;
        pResult->lpBuffer = data;
        return *this;
    }
public:// Заполнение результатов команд WFPExecute
    /// Прикрепляет к результату указанные данные возможностей устройства.
    inline Result& data(WFSIDCCARDDATA* data) {
        assert(pResult != NULL);
        pResult->u.dwCommandCode = WFS_CMD_IDC_READ_RAW_DATA;
        pResult->lpBuffer = data;
        return *this;
    }
    /// Прикрепляет к результату указанные данные возможностей устройства.
    inline Result& data(WFSIDCCHIPIO* data) {
        assert(pResult != NULL);
        pResult->u.dwCommandCode = WFS_CMD_IDC_CHIP_IO;
        pResult->lpBuffer = data;
        return *this;
    }
public:
    /// Прикрепляет к результату указанные данные возможностей устройства.
    inline Result& data(WFSDEVSTATUS* data) {
        assert(pResult != NULL);
        pResult->u.dwEventID = WFS_SYSE_DEVICE_STATUS;
        pResult->lpBuffer = data;
        return *this;
    }
    void send(HWND hWnd, DWORD messageType) {
        PostMessage(hWnd, messageType, NULL, (LPARAM)pResult);
    }
private:
    inline void init(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
        pResult = xfsAlloc<WFSRESULT>();
        pResult->RequestID = ReqID;
        pResult->hService = hService;
        pResult->hResult = result;
        GetSystemTime(&pResult->tsTimestamp);
    }
};

#endif // PCSC_CENXFS_BRIDGE_Result_H