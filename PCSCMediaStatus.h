#ifndef PCSC_CENXFS_BRIDGE_MediaStatus_H
#define PCSC_CENXFS_BRIDGE_MediaStatus_H

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
class MediaStatus {
    DWORD value;
    
    template<class OS>
    friend OS& operator<<(OS& os, MediaStatus s);
public:
    MediaStatus() : value(0) {}
    MediaStatus(DWORD value) : value(value) {}
    WORD translateMedia() {
        WORD result = 0;
        // Карта отсутствует в устройстве
        if (value & SCARD_ABSENT) {
            // Media is not present in the device and not at the entering position.
            result |= WFS_IDC_MEDIANOTPRESENT;
        }
        // Карта присутствует в устройстве, однако она не в рабочей позиции
        if (value & SCARD_PRESENT) {
            // Media is present in the device, not in the entering
            // position and not jammed. On the Latched DIP device,
            // this indicates that the card is present in the device and
            // the card is unlatched.
            result |= WFS_IDC_MEDIAPRESENT;
        }
        // Карта присутствует в устройстве, но на ней нет питания
        if (value & SCARD_SWALLOWED) {
            // Media is at the entry/exit slot of a motorized device
            result |= WFS_IDC_MEDIAENTERING;
        }
        // Питание на карту подано, но считыватель не знает режим работы карты
        if (value & SCARD_POWERED) {
            //result |= ;
        }
        // Карта была сброшена (reset) и ожидает согласование PTS.
        if (value & SCARD_NEGOTIABLE) {
            //result |= ;
        }
        // Карта была сброшена (reset) и установлен specific протокол общения.
        if (value & SCARD_SPECIFIC) {
            //result |= ;
        }
        return result;
    }
    WORD translateChipPower() {
        WORD result = 0;
        // Карта отсутствует в устройстве
        if (value & SCARD_ABSENT) {
            // There is no card in the device.
            result |= WFS_IDC_CHIPNOCARD;
        }
        // Карта присутствует в устройстве, однако она не в рабочей позиции
        if (value & SCARD_PRESENT) {
            // The chip is present, but powered off (i.e. not contacted).
            result |= WFS_IDC_CHIPPOWEREDOFF;
        }
        // Карта присутствует в устройстве, но на ней нет питания
        if (value & SCARD_SWALLOWED) {
            // The chip is present, but powered off (i.e. not contacted).
            result |= WFS_IDC_CHIPPOWEREDOFF;
        }
        // Питание на карту подано, но считыватель не знает режим работы карты
        if (value & SCARD_POWERED) {
            //result |= ;
        }
        // Карта была сброшена (reset) и ожидает согласование PTS.
        if (value & SCARD_NEGOTIABLE) {
            // The state of the chip cannot be determined with the device in its current state.
            result |= WFS_IDC_CHIPUNKNOWN;
        }
        // Карта была сброшена (reset) и установлен specific протокол общения.
        if (value & SCARD_SPECIFIC) {
            // The chip is present, powered on and online (i.e.
            // operational, not busy processing a request and not in an
            // error state).
            result |= WFS_IDC_CHIPONLINE;
        }
        return result;
    }

    const std::vector<CTString> flagNames() {
        static CTString names[] = {
            CTString("SCARD_ABSENT"    ),
            CTString("SCARD_PRESENT"   ),
            CTString("SCARD_SWALLOWED" ),
            CTString("SCARD_POWERED"   ),
            CTString("SCARD_NEGOTIABLE"),
            CTString("SCARD_SPECIFIC"  ),
        };
        const std::size_t size = sizeof(names)/sizeof(names[0]);
        std::vector<CTString> result(size);
        int i = -1;
        result[++i] = (value & SCARD_ABSENT    ) ? names[i] : CTString();
        result[++i] = (value & SCARD_PRESENT   ) ? names[i] : CTString();
        result[++i] = (value & SCARD_SWALLOWED ) ? names[i] : CTString();
        result[++i] = (value & SCARD_POWERED   ) ? names[i] : CTString();
        result[++i] = (value & SCARD_NEGOTIABLE) ? names[i] : CTString();
        result[++i] = (value & SCARD_SPECIFIC  ) ? names[i] : CTString();
        return result;
    }
};
template<class OS>
inline OS& operator<<(OS& os, MediaStatus s) {
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

#endif // PCSC_CENXFS_BRIDGE_MediaStatus_H