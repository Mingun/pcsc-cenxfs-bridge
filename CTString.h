#ifndef PCSC_CENXFS_BRIDGE_CTString_H
#define PCSC_CENXFS_BRIDGE_CTString_H

#pragma once

/// Класс строки времени компиляции. Позволяет эффективно получать длину строки
/// от строк, заданных в коде.
class CTString {
    const char* begin;
    const char* end;
public:
    CTString() : begin(NULL), end(NULL) {}
    template<std::size_t N>
    CTString(const char (&data)[N]) : begin(data), end(data + N) {}

    std::size_t size() const { return end - begin; }
    bool empty() const { return size() == 0; }
    bool isValid() const { return begin != NULL; }
};

#endif // PCSC_CENXFS_BRIDGE_CTString_H