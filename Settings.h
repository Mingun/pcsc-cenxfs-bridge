#ifndef PCSC_CENXFS_BRIDGE_Settings_H
#define PCSC_CENXFS_BRIDGE_Settings_H

#pragma once

#include <string>

class Settings
{
public:
    class Track2 {
    public:
        bool report;
        bool fromChip;
        std::string value;
    public:
        Track2() : report(false), fromChip(false) {}
    };
public:
    /// Название самого провайдера.
    std::string providerName;
    /// Название считывателя, с которым должен работать провайдер.
    std::string readerName;
    /// Уровень подробности выводимых сообщений, чем выше, тем подробнее.
    /// Уровень 0 -- сообщения не выводятся.
    int traceLevel;
    /// Kalignite требует, чтобы считыватель умел читать треки 2, поэтому мы имеем
    /// такую настройку.
    Track2 track2;
public:
    Settings(const char* serviceName, int traceLevel);

    std::string toJSONString() const;
};

#endif // PCSC_CENXFS_BRIDGE_Settings_H