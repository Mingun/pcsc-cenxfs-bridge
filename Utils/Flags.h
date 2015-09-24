#ifndef PCSC_CENXFS_BRIDGE_Utils_Flags_H
#define PCSC_CENXFS_BRIDGE_Utils_Flags_H

#pragma once

#include "Utils/CTString.h"

#include <cassert>
// Для std::size_t
#include <cstddef>
#include <iomanip>
#include <vector>

/** Класс для типобезопасного представления флагов, с возможностью преобразования флагов в
    текстовый вид.
@tparam T Тип для хранения значений флагов. Должен быть числовым типом.
@tparam Derived Реальный тип флагов, унаследованный от данного класса.
@tparam Count Количество флагов, которое может хранится в данном типе.
*/
template<typename T, class Derived, std::size_t Count = sizeof(T)*8>
class Flags {
public:
    /// Нижележащий целочисленный тип, в котором хранятся флаги.
    typedef T type;
public:
    /// Количество флагов, которое может хранится в данном типе.
    static const std::size_t count = Count;
protected:
    T mValue;
    template<class OS>
    friend inline OS& operator<<(OS& os, Flags<T, Derived, Count> f) {
        // Сначала сохраняем флаги форматирования, т.к. сейчас будем их менять.
        std::ios_base::fmtflags ff = os.flags();
        // Каждый байт представляется двумя 16-ричными цифрами. sizeof дает размер в байтах.
        os << "0x" << std::hex << std::setfill('0') << std::setw(2*sizeof(f.mValue)) << f.mValue << " (";
        std::vector<CTString> names = f.derived().flagNames();
        bool first = true;
        for (std::vector<CTString>::const_iterator it = names.begin(); it != names.end(); ++it) {
            // Пустые значения не добавляем в список.
            if (!it->isValid())
                continue;
            if (!first) {
                os << " | ";
            }
            os << *it;
            first = false;
        }
        os << ")";
        // Восстанавливаем флаги форматирования
        os.flags(ff);
        return os;
    }
protected:
    Flags(T value) : mValue(value) {}
public:
    /// Получает текущую битовую маску установленных флагов, как число нижележащего типа.
    inline T value() const { return this->mValue; }
    /// Получает количество установленных флагов.
    std::size_t size() const {
        std::size_t r = 0;
        for (std::size_t i = 0; i < Count; ++i) {
            if (this->mValue & (1 << i)) {
                ++r;
            }
        }
        return r;
    }
protected:
    /** Получает список названий флагов, установленных в данный момент.
    @tparam N
        Размер массива с названиями флагов. Если его размер превышает количество битов,
        которое может хранится в типе `T` (`sizeof(T)*8`), то лишние (с конца) элементы
        массива не используются.

    @param names
        Массив с названиями флагов. Первый элемент массива содержит название для случая, когда
        ни один флаг не установлен. Последующие элементы содержат названия для каждого флага,
        начиная с младшего.

    @return
        Массив названий установленных флагов. Размер массива равен количеству битов,
        которое содержится в типе `T` (`sizeof(T)*8`).
    */
    template<std::size_t N>
    inline std::vector<CTString> flagNames(const CTString (&names)[N]) const {
        const std::size_t size = N-1 < sizeof(T)*8 ? N-1 : sizeof(T)*8;

        std::vector<CTString> result(sizeof(T)*8);
        if (this->mValue == (T)0) {
            result[0] = names[0];
        }
        for (std::size_t i = 0; i < size; ++i) {
            if (this->mValue & (1 << i)) {
                result[i+1] = names[i+1];
            }
        }
        return result;
    }
private:
    inline const Derived& derived() const { return *((Derived*)this); }
};

#endif // PCSC_CENXFS_BRIDGE_Utils_Flags_H