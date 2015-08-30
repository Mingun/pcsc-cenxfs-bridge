#ifndef PCSC_CENXFS_BRIDGE_PCSC_MediaStatus_H
#define PCSC_CENXFS_BRIDGE_PCSC_MediaStatus_H

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

namespace PCSC {
    /** Класс для представления состояния устройств. Позволяет преобразовать состояния в XFS
        состояния и распечатать набор текущих флагов.
    @par 
        В Linux-е данное значение является битовой маской значений, в то время, как
        в Windows это перечисление
    */
    class MediaStatus : public Enum<DWORD, MediaStatus> {
        typedef Enum<DWORD, MediaStatus> _Base;
    public:
        MediaStatus() : _Base(0) {}
        MediaStatus(DWORD value) : _Base(value) {}
        WORD translateMedia() {
            switch (mValue) {
            // Карта отсутствует в устройстве
            case SCARD_ABSENT: {
                // Media is not present in the device and not at the entering position.
                return WFS_IDC_MEDIANOTPRESENT;
            }
            // Карта присутствует в устройстве, однако она не в рабочей позиции
            case SCARD_PRESENT: {
                // Media is present in the device, not in the entering
                // position and not jammed. On the Latched DIP device,
                // this indicates that the card is present in the device and
                // the card is unlatched.
                return WFS_IDC_MEDIAPRESENT;
            }
            // Карта присутствует в устройстве, но на ней нет питания
            case SCARD_SWALLOWED: {
                // Media is at the entry/exit slot of a motorized device
                return WFS_IDC_MEDIAENTERING;
            }
            // Питание на карту подано, но считыватель не знает режим работы карты
            case SCARD_POWERED:
            // Карта была сброшена (reset) и ожидает согласование PTS.
            case SCARD_NEGOTIABLE:
            // Карта была сброшена (reset) и установлен specific протокол общения.
            case SCARD_SPECIFIC: break;
            }// switch (mValue)
            return 0;
        }
        WORD translateChipPower() {
            switch (mValue) {
            // Карта отсутствует в устройстве
            case SCARD_ABSENT: {
                // There is no card in the device.
                return WFS_IDC_CHIPNOCARD;
            }
            // Карта присутствует в устройстве, однако она не в рабочей позиции
            case SCARD_PRESENT: {
                // The chip is present, but powered off (i.e. not contacted).
                return WFS_IDC_CHIPPOWEREDOFF;
            }
            // Карта присутствует в устройстве, но на ней нет питания
            case SCARD_SWALLOWED: {
                // The chip is present, but powered off (i.e. not contacted).
                return WFS_IDC_CHIPPOWEREDOFF;
            }
            // Питание на карту подано, но считыватель не знает режим работы карты
            case SCARD_POWERED: {
                return 0;
            }
            // Карта была сброшена (reset) и ожидает согласование PTS.
            case SCARD_NEGOTIABLE: {
                // The state of the chip cannot be determined with the device in its current state.
                return WFS_IDC_CHIPUNKNOWN;
            }
            // Карта была сброшена (reset) и установлен specific протокол общения.
            case SCARD_SPECIFIC: {
                // The chip is present, powered on and online (i.e.
                // operational, not busy processing a request and not in an
                // error state).
                return WFS_IDC_CHIPONLINE;
            }
            }// switch (mValue)
            return 0;
        }

        const CTString name() {
            static CTString names[] = {
                CTString("SCARD_UNKNOWN"   ), // 0x00000000
                CTString("SCARD_ABSENT"    ), // 0x00000001
                CTString("SCARD_PRESENT"   ), // 0x00000002
                CTString("SCARD_SWALLOWED" ), // 0x00000003
                CTString("SCARD_POWERED"   ), // 0x00000004
                CTString("SCARD_NEGOTIABLE"), // 0x00000005
                CTString("SCARD_SPECIFIC"  ), // 0x00000006
            };
            return _Base::name(names);
        }
    };
} // namespace PCSC
#endif // PCSC_CENXFS_BRIDGE_PCSC_MediaStatus_H