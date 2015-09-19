#ifndef PCSC_CENXFS_BRIDGE_XFS_ReadFlags_H
#define PCSC_CENXFS_BRIDGE_XFS_ReadFlags_H

#pragma once

#include "Utils/CTString.h"
#include "Utils/Flags.h"

#include <vector>
// Для DWORD
#include <windef.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

namespace XFS {
    /** Класс для представления состояния устройств. Позволяет преобразовать состояния в XFS
        состояния и распечатать набор текущих флагов.
    */
    class ReadFlags : public Flags<DWORD, ReadFlags> {
        typedef Flags<DWORD, ReadFlags> _Base;
    public:
        ReadFlags(DWORD value) : _Base(value) {}

        const std::vector<CTString> flagNames() const {
            static CTString names[] = {
                // CTString("WFS_IDC_NOTSUPP"     ), // 0x0000
                CTString("WFS_IDC_TRACK1"      ), // 0x0001 Track 1 of the magnetic stripe will be read.
                CTString("WFS_IDC_TRACK2"      ), // 0x0002 Track 2 of the magnetic stripe will be read.
                CTString("WFS_IDC_TRACK3"      ), // 0x0004 Track 3 of the magnetic stripe will be read.
                CTString("WFS_IDC_CHIP"        ), // 0x0008 The chip will be read.
                CTString("WFS_IDC_SECURITY"    ), // 0x0010 A security check will be performed.
                // If the IDC Flux Sensor is programmable it will be disabled in order
                // to allow chip data to be read on cards which have no magnetic stripes.
                CTString("WFS_IDC_FLUXINACTIVE"), // 0x0020
                CTString("WFS_IDC_TRACK_WM"    ), // 0x8000
            };
            std::vector<CTString> result;
            int i = -1;
            ++i; if (value() & WFS_IDC_TRACK1      ) result.push_back(names[i]);
            ++i; if (value() & WFS_IDC_TRACK2      ) result.push_back(names[i]);
            ++i; if (value() & WFS_IDC_TRACK3      ) result.push_back(names[i]);
            ++i; if (value() & WFS_IDC_CHIP        ) result.push_back(names[i]);
            ++i; if (value() & WFS_IDC_SECURITY    ) result.push_back(names[i]);
            ++i; if (value() & WFS_IDC_FLUXINACTIVE) result.push_back(names[i]);
            ++i; if (value() & WFS_IDC_TRACK_WM    ) result.push_back(names[i]);
            return result;
        }
    };
} // namespace XFS
#endif // PCSC_CENXFS_BRIDGE_XFS_ReadFlags_H