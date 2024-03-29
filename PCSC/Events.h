#ifndef PCSC_CENXFS_BRIDGE_PCSC_Events_H
#define PCSC_CENXFS_BRIDGE_PCSC_Events_H

#pragma once

#include "Service.h"

#include "PCSC/ReaderState.h"

#include "XFS/Logger.h"
// Для XFS::Str
#include "XFS/Memory.h"
#include "XFS/Result.h"

// PC/CS API -- для SCARD_READERSTATE
#include <winscard.h>
// Для GetComputerNameEx
#include <winbase.h>

namespace PCSC {
    /// Базовый класс для всех событий, генерируемых подсистемой PC/SC и транслируемых в XFS.
    class Event {
    protected:
        const Service& mService;
    protected:
        Event(const Service& service) : mService(service) {}

        inline XFS::Result success() const {
            // Поля ReqID и hResult не нужны для сервисных событий
            return XFS::Result(0, mService.handle(), WFS_SUCCESS);
        }
    };

    /// Функтор, создающий результат уведомления о вставке карты каждому заинтересованному слушателю.
    class CardInserted : public Event {
    public:
        CardInserted(const Service& service) : Event(service) {}
        XFS::Result operator()() const {
            XFS::Logger() << "Create CardInserted event";
            return success().cardInserted();
        }
    };
    /// Функтор, создающий результат уведомления о удалении карты каждому заинтересованному слушателю.
    class CardRemoved : public Event {
    public:
        CardRemoved(const Service& service) : Event(service) {}
        XFS::Result operator()() const {
            XFS::Logger() << "Create CardRemoved event";
            return success().cardRemoved();
        }
    };
    /// Функтор, создающий результат уведомления о появлении нового устройства каждому заинтересованному слушателю.
    class DeviceDetected : public Event {
        /// Состояние вновь подключившигося устройства.
        /// TODO: Сделать копию объекта? Вдруг считыватель исчезнет прежде, чем успеем послать сообщение?
        const SCARD_READERSTATE& state;
    public:
        DeviceDetected(const Service& service, const SCARD_READERSTATE& state) : Event(service), state(state) {}
        XFS::Result operator()() const {
            XFS::Logger() << "Create DeviceDetected event";
            //TODO: Возможно, необходимо выделять память через WFSAllocateMore
            WFSDEVSTATUS* status = XFS::alloc<WFSDEVSTATUS>();
            // Имя физичеcкого устройства, чье состояние изменилось
            status->lpszPhysicalName = (LPSTR)XFS::Str(state.szReader).begin();

            DWORD len = 0;
            // Сначала получаем размер буфера (включает размер для завершающего 0)
            GetComputerNameEx(ComputerNameNetBIOS, NULL, &len);
            // Рабочая станция, на которой запущен сервис.
            status->lpszWorkstationName = XFS::allocArr<CHAR>(len);
            GetComputerNameEx(ComputerNameNetBIOS, status->lpszWorkstationName, &len);
            status->dwState = PCSC::ReaderState(state.dwEventState).translate();
            return success().attach(status);
        }
    };
} // namespace PCSC

#endif PCSC_CENXFS_BRIDGE_PCSC_Events_H