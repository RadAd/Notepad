#include "stdafx.h"
#include "StringX.h"

PSTR StrRStrA(_In_ PCSTR pszSource, _In_opt_ PCSTR pszLast, _In_ PCSTR pszSrch)
{
    PSTR ret = NULL;
    DWORD len = (DWORD) strlen(pszSrch);
    while ((ret = StrRStrIA(pszSource, pszLast, pszSrch)) != NULL
        && StrCmpNA(ret, pszSrch, len) != 0)
        pszLast = ret;
    return ret;
}

PWSTR StrRStrW(_In_ PCWSTR pszSource, _In_opt_ PCWSTR pszLast, _In_ PCWSTR pszSrch)
{
    PWSTR ret = NULL;
    DWORD len = (DWORD) wcslen(pszSrch);
    while ((ret = StrRStrIW(pszSource, pszLast, pszSrch)) != NULL
        && StrCmpNW(ret, pszSrch, len) != 0)
        pszLast = ret;
    return ret;
}
