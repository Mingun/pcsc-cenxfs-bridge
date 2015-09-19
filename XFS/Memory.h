#ifndef PCSC_CENXFS_BRIDGE_XFS_Memory_H
#define PCSC_CENXFS_BRIDGE_XFS_Memory_H

#pragma once

#include <cassert>
// Для std::size_t
#include <cstddef>
// Для strlen и strncpy
#include <cstring>
// Для WFMAllocateBuffer
#include <xfsadmin.h>

namespace XFS {
    /** Функция, выделяющая память под структуру указанного типа менеджером памяти XFS.
        Память, выделенная данной функцией, должна быть освобождена вызовом функции
        `WFMFreeBuffer` (если указатель отдается вызывающему, это делает клиент DLL).
    @tparam T Тип структуры, для которой выделяется память. Конструктор не вызывается.
    */
    template<typename T>
    static T* alloc() {
        T* result = 0;
        HRESULT h = WFMAllocateBuffer((ULONG)sizeof(T), WFS_MEM_ZEROINIT, (void**)&result);
        assert(h >= 0 && "Cannot allocate memory");
        return result;
    }
    template<typename T>
    static T* alloc(const T& value) {
        T* result = 0;
        HRESULT h = WFMAllocateBuffer((ULONG)sizeof(T), WFS_MEM_ZEROINIT, (void**)&result);
        assert(h >= 0 && "Cannot allocate memory");
        *result = value;
        return result;
    }
    /** Функция, выделяющая память под массив структур указанного типа менеджером памяти XFS.
        Память, выделенная данной функцией, должна быть освобождена вызовом функции
        `WFMFreeBuffer` (если указатель отдается вызывающему, это делает клиент DLL).
    @tparam T Тип структуры, для которой выделяется память. Конструктор не вызывается.
    */
    template<typename T>
    static T* allocArr(std::size_t size) {
        T* result = 0;
        HRESULT h = WFMAllocateBuffer((ULONG)(sizeof(T) * size), WFS_MEM_ZEROINIT, (void**)&result);
        assert(h >= 0 && "Cannot allocate memory");
        return result;
    }

    class Str {
        const char* mBegin;
        const char* mEnd;
    public:
        explicit Str(const char* str) {
            std::size_t len = std::strlen(str) + 1;
            char* data = allocArr<char>(len);
            std::strncpy(data, str, len);
            mBegin = data;
            mEnd = data + len;
        }

        inline std::size_t size() const { return mEnd - mBegin; }
        inline bool empty() const { return size() == 0; }
        inline bool isValid() const { return mBegin != 0; }
        inline const char* begin() const { return mBegin; }
        inline const char* end() const { return mEnd; }
    };
} // namespace XFS
#endif // PCSC_CENXFS_BRIDGE_XFS_Memory_H