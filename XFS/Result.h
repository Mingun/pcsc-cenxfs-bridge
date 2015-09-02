#ifndef PCSC_CENXFS_BRIDGE_XFS_Result_H
#define PCSC_CENXFS_BRIDGE_XFS_Result_H

#pragma once

#include <string>
#include <sstream>
#include <cassert>

// Для REQUESTID, HSERVICE, HRESULT, LONG и DWORD
#include <windef.h>
// Для PostMessage
#include <winuser.h>
// Для GetSystemTime
#include <WinBase.h>
// Определения для ридеров карт (Identification card unit (IDC)), для WFMAllocateBuffer, WFMOutputTraceData и WFSRESULT
#include <XFSIDC.h>

#include "Utils/CTString.h"
#include "Utils/Enum.h"
#include "PCSC/Status.h"
#include "XFS/Status.h"
#include "XFS/Memory.h"

namespace XFS {
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
            if (_Base::name(names1, mValue - (WM_USER + 1), result)) {
                return result;
            }
            if (_Base::name(names2, mValue - (WM_USER + 20), result)) {
                return result;
            }
            return _Base::name(names3, mValue - (WM_USER + 100));
        }
    };

    /** Класс для представления результата выполнения XFS SPI-метода. */
    class Result {
        WFSRESULT* pResult;
    public:
        Result(REQUESTID ReqID, HSERVICE hService, PCSC::Status result) {
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
        inline Result& data(WFSIDCCARDDATA** data) {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwCommandCode = WFS_CMD_IDC_READ_RAW_DATA;
            pResult->lpBuffer = data;
            return *this;
        }
        /// Прикрепляет к результату указанные данные возможностей устройства.
        inline Result& data(WFSIDCCHIPIO* data) {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwCommandCode = WFS_CMD_IDC_CHIP_IO;
            pResult->lpBuffer = data;
            return *this;
        }
    public:
        /// Прикрепляет к результату указанные данные возможностей устройства.
        inline Result& data(WFSDEVSTATUS* data) {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_SYSE_DEVICE_STATUS;
            pResult->lpBuffer = data;
            return *this;
        }
    public:// События доступности карты в считывателе.
        /// Событие генерируется, когда считыватель обнаруживает, что в него вставлена карта.
        inline Result& cardInserted() {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_EXEE_IDC_MEDIAINSERTED;
            return *this;
        }
        /// Событие генерируется, когда считыватель обнаруживает, что из него вытащена карта.
        inline Result& cardRemoved() {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_SRVE_IDC_MEDIAREMOVED;
            return *this;
        }
        /// Генерируется, если карта была обнаружена в считывателе во время команды
        /// сброса (WFS_CMD_IDC_RESET). Так как карта обнаружена, считается, что она
        /// готова к чтению.
        /// TODO: Это может быть не так?
        inline Result& cardDetected() {
            assert(pResult != NULL);
            assert(pResult->lpBuffer == NULL && "Result already has data!");
            pResult->u.dwEventID = WFS_SRVE_IDC_MEDIADETECTED;
            //TODO: Возможно, необходимо выделять память черех WFSAllocateMore
            pResult->lpBuffer = XFS::alloc<DWORD>(WFS_IDC_CARDREADPOSITION);
            return *this;
        }
        void send(HWND hWnd, DWORD messageType) {
            assert(pResult != NULL);
            std::stringstream ss;
            ss << std::string("Result::send(hWnd=") << hWnd << ", type=" << MsgType(messageType)
               << ") with result " << Status(pResult->hResult) << " for ReqID=" << pResult->RequestID;
            WFMOutputTraceData((LPSTR)ss.str().c_str());
            PostMessage(hWnd, messageType, NULL, (LPARAM)pResult);
        }
    private:
        inline void init(REQUESTID ReqID, HSERVICE hService, HRESULT result) {
            pResult = XFS::alloc<WFSRESULT>();
            pResult->RequestID = ReqID;
            pResult->hService = hService;
            pResult->hResult = result;
            GetSystemTime(&pResult->tsTimestamp);

            assert(pResult->lpBuffer == NULL);
        }
    };
} // namespace XFS
#endif // PCSC_CENXFS_BRIDGE_XFS_Result_H