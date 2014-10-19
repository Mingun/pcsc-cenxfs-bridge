#ifndef PCSC_CENXFS_BRIDGE_SETTINGS_H
#define PCSC_CENXFS_BRIDGE_SETTINGS_H

#pragma once

#include <string>

class Settings
{
public:
    /// �������� ������ ����������.
    std::string providerName;
    /// �������� �����������, � ������� ������ �������� ���������.
    std::string readerName;
    /// ������� ����������� ��������� ���������, ��� ����, ��� ���������.
    /// ������� 0 -- ��������� �� ���������.
    int traceLevel;
public:
    Settings(const char* serviceName, int traceLevel);
};

#endif // PCSC_CENXFS_BRIDGE_SETTINGS_H