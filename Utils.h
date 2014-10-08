#ifndef PCSC_CENXFS_BRIDGE_Utils_H
#define PCSC_CENXFS_BRIDGE_Utils_H

#pragma once
// Для WFMAllocateBuffer
#include <xfsadmin.h>

/// Класс строки времени компиляции. Позволяет эффективно получать длину строки
/// от строк, заданных в коде.
class CTString {
    const char* mBegin;
    const char* mEnd;
public:
    inline CTString() : mBegin(0), mEnd(0) {}
    template<std::size_t N>
    inline CTString(const char (&data)[N]) : mBegin(data), mEnd(data + N) {}

    inline std::size_t size() const { return mEnd - mBegin; }
    inline bool empty() const { return size() == 0; }
    inline bool isValid() const { return mBegin != 0; }
    inline const char* begin() const { return mBegin; }
    inline const char* end() const { return mEnd; }
};

template<class OS>
inline OS& operator<<(OS& os, CTString s) {
    if (s.isValid())
        return os << s.begin();
    return os;
}
/** Функция, выделяющая память под структуру указанного типа менеджером памяти XFS.
    Память, выделенная данной функцие, должна быть освобождена вызовом функции
    `WFMFreeBuffer` (если указатель отдается вызывающему, это делает клиент DLL).
@tparam T Тип структуры, для котороы выделяется память. Конструктор не вызывается.
*/
template<typename T>
static T* xfsAlloc() {
    T* result = 0;
    HRESULT h = WFMAllocateBuffer(sizeof(T), WFS_MEM_ZEROINIT, (void**)&result);
    assert(h >= 0 && "Cannot allocate memory");
    return result;
}

#endif // PCSC_CENXFS_BRIDGE_Utils_H