#pragma once

template<int size>
void FormatString(TCHAR (&pStr)[size], UINT uID, ...)
{
    TCHAR sFormat[128];
    LoadString(g_hInst, uID, sFormat, ARRAYSIZE(sFormat));
    va_list args;
    va_start(args, uID);
    _vsntprintf_s(pStr, size, sFormat, args);
    va_end(args);
}

PSTR StrRStrA(_In_ PCSTR pszSource, _In_opt_ PCSTR pszLast, _In_ PCSTR pszSrch);
PWSTR StrRStrW(_In_ PCWSTR pszSource, _In_opt_ PCWSTR pszLast, _In_ PCWSTR pszSrch);
