
#include "Settings.h"

#include <string>
#include <vector>
#include <sstream>
// XFS API ��� ������� ������� � �������.
#include <xfsconf.h>
// ��� WFMOutputTraceData.
#include <xfsadmin.h>

class RegKey {
    HKEY hKey;
public:
    inline RegKey(HKEY root, const char* name) {
        HRESULT r = WFMOpenKey(root, (LPSTR)name, &hKey);
        std::stringstream ss;
        ss << std::string("RegKey::RegKey(") << root << ", " << (name == NULL ? "NULL" : name) << ", &" << hKey << ") = "  << r;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
    inline ~RegKey() {
        HRESULT r = WFMCloseKey(hKey);
        std::stringstream ss;
        ss << std::string("WFMCloseKey(") << hKey << ") = " << r;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }

    inline RegKey child(const char* name) const {
        return RegKey(hKey, name);
    }
    inline std::string value(const char* name) const {
        // ������ ������ �������� �����.
        DWORD dwSize = 0;
        HRESULT r = WFMQueryValue(hKey, (LPSTR)name, NULL, &dwSize);
        {std::stringstream ss;
        ss << '(' << dwSize << ')' << r;
        WFMOutputTraceData((LPSTR)ss.str().c_str());}
        // ���������� ������, �.�. �� ����������� ������������� ������ ��� ������,
        // ���� ������ ������� � ������ �� string.
        // dwSize �������� ����� ������ ��� ������������ NULL, �� �� ������������ � �������� ��������.
        std::vector<char> value(dwSize+1);
        if (dwSize > 0) {
            dwSize = value.capacity();
            r = WFMQueryValue(hKey, (LPSTR)name, &value[0], &dwSize);
            {std::stringstream ss;
            ss << '(' << dwSize << ')' << r;
            WFMOutputTraceData((LPSTR)ss.str().c_str());}
        }
        std::string result = std::string(value.begin(), value.end());
        std::stringstream ss;
        ss << std::string("RegKey::value(") << name << ") = " << result;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
        return result;
    }
    /// ���������� ������� ��� ������ � ������ ���� �������� ������.
    void keys() const {
        WFMOutputTraceData("keys");
        std::vector<char> keyName(256);
        for (DWORD i = 0; ; ++i) {
            DWORD size = keyName.capacity();
            HRESULT r = WFMEnumKey(hKey, i, &keyName[0], &size, NULL);
            if (r == WFS_ERR_CFG_NO_MORE_ITEMS) {
                break;
            }
            keyName[size] = '\0';
            WFMOutputTraceData((LPSTR)&keyName[0]);
        }
    }
    /// ���������� ������� ��� ������ � ������ ���� �������� �������� �����.
    /// �������� ����� -- ��� ���� (���=��������).
    void values() const {
        WFMOutputTraceData("values");
        // � ���������, ������ ���������� ����� ������� ����������.
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
            std::stringstream ss;
            ss << i << ": " << '('<<szName<<','<<szValue<<')' << std::string(name.begin(), name.begin()+szName) << "="
               << std::string(value.begin(), value.begin()+szValue);
            WFMOutputTraceData((LPSTR)ss.str().c_str());
        }
    }
    static void log(std::string operation, HRESULT r) {
        std::stringstream ss;
        ss << operation << r;
        WFMOutputTraceData((LPSTR)ss.str().c_str());
    }
};

Settings::Settings(const char* serviceName, int traceLevel)
    : traceLevel(traceLevel)
{
    HKEY root = WFS_CFG_HKEY_XFS_ROOT;
    providerName = RegKey(root, "LOGICAL_SERVICES").child((LPSTR)serviceName).value("Provider");
    readerName = RegKey(root, "SERVICE_PROVIDERS").child(providerName.c_str()).value("ReaderName");
}