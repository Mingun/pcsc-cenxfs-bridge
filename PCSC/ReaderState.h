#ifndef PCSC_CENXFS_BRIDGE_PCSC_ReaderState_H
#define PCSC_CENXFS_BRIDGE_PCSC_ReaderState_H

#pragma once

#include "Utils/CTString.h"
#include "Utils/Flags.h"

// Для std::size_t
#include <cstddef>
#include <vector>
// Для DWORD
#include <windef.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

namespace PCSC {
    /** Класс для представления состояния устройств в структуре SCARD_READERSTATE.
        Позволяет преобразовать состояния в XFS состояния и распечатать набор текущих флагов.
    */
    class ReaderState : public Flags<DWORD, ReaderState> {
        typedef Flags<DWORD, ReaderState> _Base;
    public:
        ReaderState(DWORD value) : _Base(value) {}
        DWORD translate() const {
            DWORD result = 0;
            // Считыватель должен быть проигнорирован
            if (mValue & SCARD_STATE_IGNORE) {
            }
            // Состояние, которое известно менеджеру ресурсов, отличатет от того, что известно нам.
            if (mValue & SCARD_STATE_CHANGED) {
            }
            // Данный считыватель не распознался менеджером ресурсов. Также в этом случае стоят флаги
            // SCARD_STATE_CHANGED и SCARD_STATE_IGNORE
            if (mValue & SCARD_STATE_UNKNOWN) {
            }
            // Актуальное состояние считывателя получить нет возможности. Если флаг стоит, все последующие флаги сброшены.
            if (mValue & SCARD_STATE_UNAVAILABLE) {
            }
            // В считывателе нет карточки. Если флаг стоит, все последующие флаги сброшены.
            if (mValue & SCARD_STATE_EMPTY) {
            }
            // В считывателе есть карточка. Если флаг стоит, все последующие флаги сброшены.
            if (mValue & SCARD_STATE_PRESENT) {
            }
            // ATR карты в считывателе совпадает с ATR одной из целевых карт.
            // Флаг SCARD_STATE_PRESENT также стоит.
            // Данный флаг выставляется только функцией SCardLocateCards.
            if (mValue & SCARD_STATE_ATRMATCH) {
            }
            // Карточка в считывателе эксклюзивно захвачена другим прилоением. Флаг SCARD_STATE_PRESENT также стоит.
            if (mValue & SCARD_STATE_EXCLUSIVE) {
            }
            // Карточка уже используется другими приложениями, но может быть использована совместно,
            // если открыть ее в shared-режиме. Флаг SCARD_STATE_PRESENT также стоит.
            if (mValue & SCARD_STATE_INUSE) {
            }
            // Карточка в считывателе не отвечает.
            if (mValue & SCARD_STATE_MUTE) {
            }
            if (mValue & SCARD_STATE_UNPOWERED) {
            }
            return result;
        }

        const std::vector<CTString> flagNames() const {
            static CTString names[] = {
                CTString("SCARD_STATE_UNAWARE"    ),// 0x0000    Приложение не знает статус и хочет его узнать
                CTString("SCARD_STATE_IGNORE"     ),// 0x0001    Не получать уведомления об этом считывателе
                CTString("SCARD_STATE_CHANGED"    ),// 0x0002    В состоянии что-то изменилось
                CTString("SCARD_STATE_UNKNOWN"    ),// 0x0004    От PC/SC: Менеджер PC/SC не знает о считывателе с таким именем
                CTString("SCARD_STATE_UNAVAILABLE"),// 0x0008    От PC/SC: Статус считывателя получить не удалось
                CTString("SCARD_STATE_EMPTY"      ),// 0x0010    От PC/SC: Карточка была удалена
                CTString("SCARD_STATE_PRESENT"    ),// 0x0020    От PC/SC: Карточка была вставлена
                CTString("SCARD_STATE_ATRMATCH"   ),// 0x0040    От PC/SC: В считывателе карточка с искомым ATR
                CTString("SCARD_STATE_EXCLUSIVE"  ),// 0x0080    Карточка монопольно используется другим приложением
                CTString("SCARD_STATE_INUSE"      ),// 0x0100    Карточка используется другим(и) приложением(и), но мы также можем ее использовать.
                CTString("SCARD_STATE_MUTE"       ),// 0x0200    Unresponsive card.
                CTString("SCARD_STATE_UNPOWERED"  ),// 0x0400    Unpowered card.
            };
            return _Base::flagNames(names);
        }
    };
} // namespace PCSC
#endif // PCSC_CENXFS_BRIDGE_PCSC_ReaderState_H