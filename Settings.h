#ifndef PCSC_CENXFS_BRIDGE_SETTINGS_H
#define PCSC_CENXFS_BRIDGE_SETTINGS_H

#pragma once

#include <string>

class Settings
{
public:
    /// Название самого провайдера.
    std::string providerName;
    /// Название считывателя, с которым должен работать провайдер.
    std::string readerName;
    /// Уровень подробности выводимых сообщений, чем выше, тем подробнее.
    /// Уровень 0 -- сообщения не выводятся.
    int traceLevel;
public:
    Settings(const char* serviceName, int traceLevel);
};

#endif // PCSC_CENXFS_BRIDGE_SETTINGS_H