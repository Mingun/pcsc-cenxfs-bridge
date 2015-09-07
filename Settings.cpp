
#include "Settings.h"

#include <string>
#include <vector>
// XFS API для функций доступа к реестру.
#include <xfsconf.h>

#include "XFS/Logger.h"

class RegKey {
    HKEY hKey;
public:
    inline RegKey(HKEY root, const char* name) {
        HRESULT r = WFMOpenKey(root, (LPSTR)name, &hKey);

        XFS::Logger() << "RegKey::RegKey(root=" << root << ", name=" << name << ", hKey=&" << hKey << ") = "  << r;
    }
    inline ~RegKey() {
        HRESULT r = WFMCloseKey(hKey);

        XFS::Logger() << "WFMCloseKey(hKey=" << hKey << ") = " << r;
    }

    inline RegKey child(const char* name) const {
        return RegKey(hKey, name);
    }
    inline std::string value(const char* name = NULL) const {
        // Узнаем размер значения ключа.
        DWORD dwSize = 0;
        HRESULT r = WFMQueryValue(hKey, (LPSTR)name, NULL, &dwSize);

        {XFS::Logger() << "RegKey::value[size](name=" << name << ", size=&" << dwSize << ") = " << r;}
        // Используем вектор, т.к. он гарантирует непрерывность памяти под данные,
        // чего нельзя сказать в случае со string.
        // dwSize содержит длину строки без завершающего NULL, но он записывается в выходное значение.
        std::vector<char> value(dwSize+1);
        if (dwSize > 0) {
            dwSize = value.capacity();
            r = WFMQueryValue(hKey, (LPSTR)name, &value[0], &dwSize);
            {XFS::Logger() << "RegKey::value[value](name=" << name << ", value=&" << &value[0] << ", size=&" << dwSize << ") = " << r;}
        }
        std::string result = std::string(value.begin(), value.end()-1);

        XFS::Logger() << "RegKey::value(name=" << name << ") = " << result;
        return result;
    }
    inline DWORD dwValue(const char* name) const {
        // Узнаем размер значения ключа.
        DWORD result = 0;
        DWORD dwSize = sizeof(DWORD);
        HRESULT r = WFMQueryValue(hKey, (LPSTR)name, (LPSTR)&result, &dwSize);

        XFS::Logger() << "RegKey::value(name=" << name << ") = " << result;
        return result;
    }
    /// Отладочная функция для вывода в трассу всех дочерных ключей.
    void keys() const {
        {XFS::Logger() << "keys";}
        std::vector<char> keyName(256);
        for (DWORD i = 0; ; ++i) {
            DWORD size = keyName.capacity();
            HRESULT r = WFMEnumKey(hKey, i, &keyName[0], &size, NULL);
            if (r == WFS_ERR_CFG_NO_MORE_ITEMS) {
                break;
            }
            keyName[size] = '\0';

            XFS::Logger() << &keyName[0];
        }
    }
    /// Отладочная функция для вывода в трассу всех дочерных значений ключа.
    /// Значение ключа -- это пара (имя=значение).
    void values() const {
        {XFS::Logger() << "values";}
        // К сожалению, узнать конкретные длины заранее невозможно.
        std::vector<char> name(256);
        std::vector<char> value(256);
        for (DWORD i = 0; ; ++i) {
            DWORD szName = name.capacity();
            DWORD szValue = value.capacity();
            HRESULT r = WFMEnumValue(hKey, i, &name[0], &szName, &value[0], &szValue);
            if (r == WFS_ERR_CFG_NO_MORE_ITEMS) {
                break;
            }
            name[szName] = '\0';
            value[szValue] = '\0';

            XFS::Logger()
                << i << ": " << '('<<szName<<','<<szValue<<')' << std::string(name.begin(), name.begin()+szName) << "="
                << std::string(value.begin(), value.begin()+szValue);
        }
    }
};

Settings::Settings(const char* serviceName, int traceLevel)
    : traceLevel(traceLevel)
    , reportReadTrack2(false)
{
    // У Калигнайта под данным корнем не появляется провайдера, если он в
    // HKEY_LOCAL_MACHINE\SOFTWARE\XFS\SERVICE_PROVIDERS\
    // HKEY root = WFS_CFG_HKEY_XFS_ROOT;
    HKEY root = WFS_CFG_USER_DEFAULT_XFS_ROOT;// HKEY_USERS\.DEFAULT\XFS
    providerName = RegKey(root, "LOGICAL_SERVICES").child((LPSTR)serviceName).value("Provider");

    RegKey pcscSettings = RegKey(root, "SERVICE_PROVIDERS").child(providerName.c_str());

    readerName = pcscSettings.value("ReaderName");
    reportReadTrack2 = pcscSettings.dwValue("ReportReadTrack2") != 0;
}
std::string Settings::toJSONString() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "\tProviderName: " << providerName << ",\n";
    ss << "\tReaderName: " << readerName << ",\n";
    ss << "\tTraceLevel: " << traceLevel << ",\n";
    ss << "\tReportReadTrack2: " << std::boolalpha << reportReadTrack2 << ",\n";
    ss << '}';
    return ss.str();
}