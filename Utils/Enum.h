#ifndef PCSC_CENXFS_BRIDGE_Utils_Enum_H
#define PCSC_CENXFS_BRIDGE_Utils_Enum_H

#pragma once

// Для std::size_t
#include <cstddef>
#include <iomanip>

#include "Utils/CTString.h"

/** Класс для типобезопасного представления перечислений.
@tparam T Тип для хранения значений перечисления.
@tparam Derived Тип, унаследованный от данного класса.
*/
template<typename T, class Derived>
class Enum {
public:
    typedef T type;
protected:
    T mValue;
    template<class OS>
    friend inline OS& operator<<(OS& os, Enum<T, Derived> e) {
        os << e.derived().name() << " (0x"
           // На каждый байт требуется 2 символа.
           << std::hex << std::setw(2*sizeof(e.mValue)) << std::setfill('0')
           << e.mValue << ")";
        return os;
    }
protected:
    Enum(T value) : mValue(value) {}

public:
    inline T value() const { return mValue; }
protected:
    static bool name(const CTString* names, std::size_t begin, std::size_t end, T value, CTString& result) {
        if (value < (T)begin || value > (T)end) {
            result = CTString("<unknown>");
            return false;
        }
        result = names[value];
        return true;
    }
    template<std::size_t N>
    static inline bool name(CTString (&names)[N], T value, CTString& result) {
        return name(names, 0, N, value, result);
    }
    template<std::size_t N>
    static inline CTString name(CTString (&names)[N], T value) {
        CTString result;
        name(names, 0, N, value, result);
        return result;
    }
protected:
    inline bool name(const CTString* names, std::size_t begin, std::size_t end, CTString& result) const {
        return this->name(names, begin, end, this->mValue, result);
    }
    template<std::size_t N>
    inline bool name(CTString (&names)[N], CTString& result) const {
        return this->name(names, 0, N, this->mValue, result);
    }
    template<std::size_t N>
    inline CTString name(CTString (&names)[N]) const {
        CTString result;
        this->name(names, 0, N, this->mValue, result);
        return result;
    }
private:
    inline const Derived& derived() const { return *((Derived*)this); }
};

#endif // PCSC_CENXFS_BRIDGE_Utils_Enum_H