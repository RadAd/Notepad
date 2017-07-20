#pragma once

inline DWORD RegQueryDWORD(HKEY hReg, LPCTSTR lpValueName, DWORD Default)
{
    DWORD Value = 0;
    DWORD Type = 0;
    DWORD Size = sizeof(Value);
    if (RegQueryValueEx(hReg, lpValueName, 0, &Type, (LPBYTE) &Value, &Size) == ERROR_FILE_NOT_FOUND || Type != REG_DWORD)
        Value = Default;
    return Value;
}

inline void RegSetDWORD(HKEY hReg, LPCTSTR lpValueName, DWORD Value)
{
    DWORD Type = REG_DWORD;
    DWORD Size = sizeof(Value);
    RegSetKeyValue(hReg, nullptr, lpValueName, Type, (LPBYTE) &Value, Size);
}

inline void RegQuerySZ(HKEY hReg, LPCTSTR lpValueName, LPTSTR lpString, DWORD Size, LPCTSTR Default)
{
    DWORD Type = 0;
    DWORD ByteSize = Size * sizeof(TCHAR);
    if (RegQueryValueEx(hReg, lpValueName, 0, &Type, (LPBYTE) lpString, &ByteSize) == ERROR_FILE_NOT_FOUND || Type != REG_SZ)
        _tcscpy_s(lpString, Size, Default);
}

inline void RegSetSZ(HKEY hReg, LPCTSTR lpValueName, LPCTSTR lpString)
{
    DWORD Type = REG_SZ;
    DWORD Size = (DWORD) wcslen(lpString) * sizeof(TCHAR);
    RegSetKeyValue(hReg, nullptr, lpValueName, Type, (LPBYTE) lpString, Size);
}
