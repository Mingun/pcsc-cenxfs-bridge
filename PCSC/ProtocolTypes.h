#ifndef PCSC_CENXFS_BRIDGE_PCSC_ProtocolTypes_H
#define PCSC_CENXFS_BRIDGE_PCSC_ProtocolTypes_H

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

namespace PCSC {
    class ProtocolTypes : public Flags<DWORD, ProtocolTypes> {
        typedef Flags<DWORD, ProtocolTypes> _Base;
    public:
        inline ProtocolTypes() : _Base(0) {}
        inline ProtocolTypes(DWORD flags) : _Base(flags) {}
        inline WORD translate() {
            WORD result = 0;
            if (value() & SCARD_PROTOCOL_T0) {// Нулевой бит -- протокол T0
                result |= WFS_IDC_CHIPT0;
            }
            if (value() & SCARD_PROTOCOL_T1) {// Первый бит -- протокол T1
                result |= WFS_IDC_CHIPT1;
            }
            return result;
        }
        const std::vector<CTString> flagNames() const {
            static CTString names[] = {
                CTString("<none>"           ),
                CTString("SCARD_PROTOCOL_T0"),
                CTString("SCARD_PROTOCOL_T1"),
            };
            return _Base::flagNames(names);
        }
    };
} // namespace PCSC
#endif // PCSC_CENXFS_BRIDGE_PCSC_ProtocolTypes_H