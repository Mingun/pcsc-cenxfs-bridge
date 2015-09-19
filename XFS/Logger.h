#ifndef PCSC_CENXFS_BRIDGE_XFS_Logger_H
#define PCSC_CENXFS_BRIDGE_XFS_Logger_H

#pragma once

#include <sstream>

// Для WFMOutputTraceData.
#include <xfsadmin.h>

namespace XFS {
    /** Перенаправляет весь вывод в XFS трассу. Логгирование осуществляется в деструкторе.
    */
    class Logger {
        std::ostringstream ss;

    public:
        Logger() { ss << "[PCSC] "; }
        ~Logger() { WFMOutputTraceData((LPSTR)ss.str().c_str()); }

    public:
        template<typename T>
        Logger& operator<<(T value) { ss << value; return *this; }
        Logger& operator<<(const char* value) {
            if (value) {
                ss << '"' << value << '"';
            } else {
                ss << "NULL";
            }
            return *this;
        }
    };
} // namespace XFS
#endif // PCSC_CENXFS_BRIDGE_XFS_Logger_H