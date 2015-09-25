#ifndef PCSC_CENXFS_BRIDGE_XFS_ResetAction_H
#define PCSC_CENXFS_BRIDGE_XFS_ResetAction_H

#pragma once

#include "Utils/CTString.h"
#include "Utils/Flags.h"

#include <vector>
// Для WORD
#include <windef.h>
// PC/CS API
#include <winscard.h>
// Определения для ридеров карт (Identification card unit (IDC))
#include <XFSIDC.h>

namespace XFS {
    /** Класс для представления состояния устройств. Позволяет преобразовать состояния в XFS
        состояния и распечатать набор текущих флагов.
    */
    class ResetAction : public Flags<WORD, ResetAction> {
        typedef Flags<WORD, ResetAction> _Base;
    public:
        ResetAction(WORD value) : _Base(value) {}

        DWORD translate() const {
            // Хоть это и флаги, в месте использования этой функции будет установлен только один флаг.
            switch (value()) {
                case WFS_IDC_CHIPPOWERCOLD: return SCARD_UNPOWER_CARD;// Power down the card and reset it (Cold Reset).
                case WFS_IDC_CHIPPOWERWARM: return SCARD_RESET_CARD;// Reset the card (Warm Reset).
                case WFS_IDC_CHIPPOWEROFF : return SCARD_LEAVE_CARD;// Do not do anything special on reconnect.
            }
            return SCARD_LEAVE_CARD;
        }

        const std::vector<CTString> flagNames() const {
            static CTString names[] = {
                CTString("WFS_IDC_CHIPPOWERCOLD"),// 0x0002
                CTString("WFS_IDC_CHIPPOWERWARM"),// 0x0004
                CTString("WFS_IDC_CHIPPOWEROFF" ),// 0x0008
            };
            std::vector<CTString> result;
            int i = -1;
            ++i; if (value() & WFS_IDC_CHIPPOWERCOLD) result.push_back(names[i]);
            ++i; if (value() & WFS_IDC_CHIPPOWERWARM) result.push_back(names[i]);
            ++i; if (value() & WFS_IDC_CHIPPOWEROFF ) result.push_back(names[i]);
            return result;
        }
    };
} // namespace XFS
#endif // PCSC_CENXFS_BRIDGE_XFS_ResetAction_H