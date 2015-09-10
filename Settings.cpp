
#include "Settings.h"

#include <string>
#include <vector>
// XFS API для функций доступа к реестру.
#include <xfsconf.h>

#include "XFS/Logger.h"

/// Класс для автоматического закрытия открытых ключей реестра, когда они более не нужны.
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
    /** Получает значение из ключа реестра с указанным именем.
    @param name
        Имя значения в ключе реестра, которое требуется получить. Значение по умолчанию (`NULL`)
        означает, что необходимо получить значение ключа по умолчанию.

    @return
        Строка со значением ключа. Если значения не существует, возвращает пустую строку.
    */
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
{
    // У Калигнайта под данным корнем не появляется провайдера, если он в
    // HKEY_LOCAL_MACHINE\SOFTWARE\XFS\SERVICE_PROVIDERS\
    // HKEY root = WFS_CFG_HKEY_XFS_ROOT;
    HKEY root = WFS_CFG_USER_DEFAULT_XFS_ROOT;// HKEY_USERS\.DEFAULT\XFS
    providerName = RegKey(root, "LOGICAL_SERVICES").child((LPSTR)serviceName).value("Provider");

    RegKey pcscSettings = RegKey(root, "SERVICE_PROVIDERS").child(providerName.c_str());
    readerName = pcscSettings.value("ReaderName");

    RegKey track2Settings = pcscSettings.child("Track2");
    track2.report = track2Settings.dwValue("Report") != 0;
    track2.fromChip = track2Settings.dwValue("FromChip") != 0;
    track2.value = track2Settings.value();
}
std::string Settings::toJSONString() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "\tProviderName: " << providerName << ",\n";
    ss << "\tReaderName: " << readerName << ",\n";
    ss << "\tTraceLevel: " << traceLevel << ",\n";
    ss << "\tTrack2.Report: " << std::boolalpha << track2.report << ",\n";
    ss << "\tTrack2.FromChip: " << std::boolalpha << track2.fromChip << ",\n";
    ss << "\tTrack2.Value: " << track2.value << ",\n";
    ss << '}';
    return ss.str();
}