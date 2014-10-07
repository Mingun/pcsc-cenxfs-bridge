#ifndef PCSC_CENXFS_BRIDGE_Result_H
#define PCSC_CENXFS_BRIDGE_Result_H

#pragma once

// Для REQUESTID, HSERVICE, HRESULT, LONG и DWORD
#include <windef.h>
// Для PostMessage
#include <winuser.h>
// Для GetSystemTime
#include <WinBase.h>
// Определения для ридеров карт (Identification card unit (IDC)), для WFMAllocateBuffer и WFSRESULT
#include <XFSIDC.h>

#include "PCSCStatus.h"

class Result {
    WFSRESULT* pResult;
public:
    Result(REQUESTID ReqID, HSERVICE hService, Status result) {
        init(ReqID, hService, result.translate());
    }
    Result(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
        init(ReqID, hService, result);
    }
    /// Прикрепляет к результату указанные данные статуса.
    inline Result& data(WFSIDCSTATUS* data) {
        assert(pResult != NULL);
        pResult->u.dwCategoryCode = WFS_INF_IDC_STATUS;
        pResult->lpBuffer = data;
        return *this;
    }
    /// Прикрепляет к результату указанные данные возможностей устройства.
    inline Result& data(WFSIDCCAPS* data) {
        assert(pResult != NULL);
        pResult->u.dwCategoryCode = WFS_INF_IDC_CAPABILITIES;
        pResult->lpBuffer = data;
        return *this;
    }
    void send(HWND hWnd, DWORD messageType) {
        PostMessage(hWnd, messageType, NULL, (LONG)pResult);
    }
private:
    inline void init(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
        // Выделяем память, заполняем ее нулями на всякий случай.
        WFMAllocateBuffer(sizeof(WFSRESULT), WFS_MEM_ZEROINIT, (void**)&pResult);
        pResult->RequestID = ReqID;
        pResult->hService = hService;
        pResult->hResult = result;
        GetSystemTime(&pResult->tsTimestamp);
    }
};

#endif // PCSC_CENXFS_BRIDGE_Result_H