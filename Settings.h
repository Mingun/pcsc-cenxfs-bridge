#ifndef PCSC_CENXFS_BRIDGE_SETTINGS_H
#define PCSC_CENXFS_BRIDGE_SETTINGS_H

#pragma once

#include <string>

class Settings
{
public:
    /// Ќазвание самого провайдера.
    std::string providerName;
    /// Ќазвание считывател€, с которым должен работать провайдер.
    std::string readerName;
    /// ”ровень подробности выводимых сообщений, чем выше, тем подробнее.
    /// ”ровень 0 -- сообщени€ не вывод€тс€.
    int traceLevel;
public:
    Settings(const char* serviceName, int traceLevel);
};

#endif // PCSC_CENXFS_BRIDGE_SETTINGS_H