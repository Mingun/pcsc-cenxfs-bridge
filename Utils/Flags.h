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
protected:
    T mValue;
    template<class OS>
    friend inline OS& operator<<(OS& os, Flags<T, Derived> f) {
        os << "0x" << std::hex << std::setfill('0') << std::setw(2*sizeof(f.mValue)) << " (";
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
        return os << ')';
    }
protected:
    Flags(T value) : mValue(value) {}
public:
    inline T value() const { return mValue; }
protected:
    template<std::size_t N>
    inline std::vector<CTString> flagNames(CTString (&names)[N]) const {
        std::vector<CTString> result(N);
        if (mValue == (T)0) {
            result[0] = names[0];
        }
        for (std::size_t i = 1; i < N; ++i) {
            if (mValue & (1 << i)) {
                result[i] = names[i];
            }
        }
        return result;
    }
private:
    inline Derived& derived() const { return *((Derived*)this); }
};

#endif // PCSC_CENXFS_BRIDGE_Utils_Flags_H