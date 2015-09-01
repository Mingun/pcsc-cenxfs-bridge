#ifndef PCSC_CENXFS_BRIDGE_Utils_Flags_H
#define PCSC_CENXFS_BRIDGE_Utils_Flags_H

#pragma once

#include <vector>
// Для std::size_t
#include <cstddef>
#include <iomanip>

#include "Utils/CTString.h"

template<typename T, class Derived>
class Flags {
public:
    typedef T type;
protected:
    T mValue;
    template<class OS>
    friend inline OS& operator<<(OS& os, Flags<T, Derived> f) {
        std::ios_base::fmtflags ff = os.flags();
        os << "0x" << std::hex << std::setfill('0') << std::setw(2*sizeof(f.mValue)) << f.mValue << " (";
        std::vector<CTString> names = f.derived().flagNames();
        bool first = true;
        for (std::vector<CTString>::const_iterator it = names.begin(); it != names.end(); ++it) {
            // Пустые значения не добавляем в список.
            if (!it->isValid())
                continue;
            if (!first) {
                os << ", ";
            }
            os << *it;
            first = false;
        }
        os.flags(ff);
        return os << ')';
    }
protected:
    Flags(T value) : mValue(value) {}
public:
    inline T value() const { return mValue; }
protected:
    template<std::size_t N>
    inline std::vector<CTString> flagNames(CTString (&names)[N]) const {
        const std::size_t size = N-1 < sizeof(T)*8 ? N-1 : sizeof(T)*8;

        std::vector<CTString> result(size);
        if (mValue == (T)0) {
            result[0] = names[0];
        }
        for (std::size_t i = 0; i < size; ++i) {
            if (mValue & (1 << i)) {
                result[i+1] = names[i+1];
            }
        }
        return result;
    }
private:
    inline const Derived& derived() const { return *((Derived*)this); }
};

#endif // PCSC_CENXFS_BRIDGE_Utils_Flags_H