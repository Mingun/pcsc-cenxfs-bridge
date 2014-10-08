#ifndef PCSC_CENXFS_BRIDGE_ReaderState_H
#define PCSC_CENXFS_BRIDGE_ReaderState_H

#pragma once

#include <vector>
// Для std::size_t
#include <cstddef>
// Для DWORD
#include <windef.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

#include "Utils.h"

/** Класс для представления состояния устройств. Позволяет преобразовать состояния в XFS
    состояния и распечатать набор текущих флагов.
*/
class ReaderState {
    DWORD value;
    
    template<class OS>
    friend OS& operator<<(OS& os, ReaderState s);
public:
    ReaderState(DWORD value) : value(value) {}
    DWORD translate() {
        DWORD result = 0;
        // Считыватель должен быть проигнорирован
        if (value & SCARD_STATE_IGNORE) {
        }
        // Состояние, которое известно менеджеру ресурсов, отличатет от того, что известно нам.
        if (value & SCARD_STATE_CHANGED) {
        }
        // Данный считыватель не распознался менеджером ресурсов. Также в этом случае стоят флаги
        // SCARD_STATE_CHANGED и SCARD_STATE_IGNORE
        if (value & SCARD_STATE_UNKNOWN) {
        }
        // Актуальное состояние считывателя получить нет возможности. Если флаг стоит, все последующие флаги сброшены.
        if (value & SCARD_STATE_UNAVAILABLE) {
        }
        // В считывателе нет карточки. Если флаг стоит, все последующие флаги сброшены.
        if (value & SCARD_STATE_EMPTY) {
        }
        // В считывателе есть карточка. Если флаг стоит, все последующие флаги сброшены.
        if (value & SCARD_STATE_PRESENT) {
        }
        // ATR карты в считывателе совпадает с ATR одной из целевых карт.
        // Флаг SCARD_STATE_PRESENT также стоит.
        // Данный флаг выставляется только функцией SCardLocateCards.
        if (value & SCARD_STATE_ATRMATCH) {
        }
        // Карточка в считывателе эксклюзивно захвачена другим прилоением. Флаг SCARD_STATE_PRESENT также стоит.
        if (value & SCARD_STATE_EXCLUSIVE) {
        }
        // Карточка уже используется другими приложениями, но может быть использована совместно,
        // если открыть ее в shared-режиме. Флаг SCARD_STATE_PRESENT также стоит.
        if (value & SCARD_STATE_INUSE) {
        }
        // Карточка в считывателе не отвечает.
        if (value & SCARD_STATE_MUTE) {
        }
        if (value & SCARD_STATE_UNPOWERED) {
        }
        return result;
    }

    const std::vector<CTString> flagNames() {
        static CTString names[] = {
            CTString("SCARD_STATE_UNAWARE"    ),// 0x0000    App wants status.
            CTString("SCARD_STATE_IGNORE"     ),// 0x0001    Ignore this reader.
            CTString("SCARD_STATE_CHANGED"    ),// 0x0002    State has changed.
            CTString("SCARD_STATE_UNKNOWN"    ),// 0x0004    Reader unknown.
            CTString("SCARD_STATE_UNAVAILABLE"),// 0x0008    Status unavailable.
            CTString("SCARD_STATE_EMPTY"      ),// 0x0010    Card removed.
            CTString("SCARD_STATE_PRESENT"    ),// 0x0020    Card inserted.
            CTString("SCARD_STATE_ATRMATCH"   ),// 0x0040    ATR matches card.
            CTString("SCARD_STATE_EXCLUSIVE"  ),// 0x0080    Exclusive Mode.
            CTString("SCARD_STATE_INUSE"      ),// 0x0100    Shared Mode.
            CTString("SCARD_STATE_MUTE"       ),// 0x0200    Unresponsive card.
            CTString("SCARD_STATE_UNPOWERED"  ),// 0x0400    Unpowered card.
        };
        const std::size_t size = sizeof(names)/sizeof(names[0]);
        std::vector<CTString> result(size);
        int i = -1;
        result[++i] = (value & SCARD_STATE_UNAWARE    ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_IGNORE     ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_CHANGED    ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_UNKNOWN    ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_UNAVAILABLE) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_EMPTY      ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_PRESENT    ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_ATRMATCH   ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_EXCLUSIVE  ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_INUSE      ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_MUTE       ) ? names[i] : CTString();
        result[++i] = (value & SCARD_STATE_UNPOWERED  ) ? names[i] : CTString();
        return result;
    }
};
template<class OS>
inline OS& operator<<(OS& os, ReaderState s) {
    os << "0x" << std::hex << std::setfill('0') << std::setw(2*sizeof(s.value)) << '(';
    std::vector<CTString> names = s.flagNames();
    bool first = true;
    for (std::vector<CTString>::const_iterator it = names.begin(); it != names.end(); ++it) {
        if (!it->isValid())
            continue;
        if (!first) {
            os << ", ";
        }
        os << *it;
        first = false;
    }
    return os << ')';
}

#endif // PCSC_CENXFS_BRIDGE_ReaderState_H