#ifndef PCSC_CENXFS_BRIDGE_XFS_ReadFlags_H
#define PCSC_CENXFS_BRIDGE_XFS_ReadFlags_H

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

#include "Utils/CTString.h"
#include "Utils/Flags.h"

namespace XFS {
    /** Класс для представления состояния устройств. Позволяет преобразовать состояния в XFS
        состояния и распечатать набор текущих флагов.
    */
    class ReadFlags {
        DWORD value;
        
        template<class OS>
        friend OS& operator<<(OS& os, ReadFlags s);
    public:
        ReadFlags(DWORD value) : value(value) {}

        const std::vector<CTString> flagNames() const {
            static CTString names[] = {
                CTString("WFS_IDC_TRACK1"      ), // Track 1 of the magnetic stripe will be read.
                CTString("WFS_IDC_TRACK2"      ), // Track 2 of the magnetic stripe will be read.
                CTString("WFS_IDC_TRACK3"      ), // Track 3 of the magnetic stripe will be read.
                CTString("WFS_IDC_CHIP"        ), // The chip will be read.
                CTString("WFS_IDC_SECURITY"    ), // A security check will be performed.
                // If the IDC Flux Sensor is programmable it will be disabled in order
                // to allow chip data to be read on cards which have no magnetic stripes.
                CTString("WFS_IDC_FLUXINACTIVE"),
                CTString("WFS_IDC_TRACK_WM"    ),
            };
            const std::size_t size = sizeof(names)/sizeof(names[0]);
            std::vector<CTString> result(size);
            int i = -1;
            result[++i] = (value & WFS_IDC_TRACK1      ) ? names[i] : CTString();
            result[++i] = (value & WFS_IDC_TRACK2      ) ? names[i] : CTString();
            result[++i] = (value & WFS_IDC_TRACK3      ) ? names[i] : CTString();
            result[++i] = (value & WFS_IDC_CHIP        ) ? names[i] : CTString();
            result[++i] = (value & WFS_IDC_SECURITY    ) ? names[i] : CTString();
            result[++i] = (value & WFS_IDC_FLUXINACTIVE) ? names[i] : CTString();
            result[++i] = (value & WFS_IDC_TRACK_WM    ) ? names[i] : CTString();
            return result;
        }
    };
    template<class OS>
    inline OS& operator<<(OS& os, ReadFlags s) {
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
} // namespace XFS
#endif // PCSC_CENXFS_BRIDGE_XFS_ReadFlags_H