#include "stdafx.h"
#include "FindReplace.h"
#include "StringX.h"
#include "resource.h"

extern HINSTANCE g_hInst;

// Note: Common control v6 edit control always uses WCHAR internally
// so its easier to also use the WCHAR version of Find and Replace

#define MAX_FINDSTRING 100

WCHAR g_szFind[MAX_FINDSTRING] = L"";
WCHAR g_szReplace[MAX_FINDSTRING] = L"";
HWND g_hFind = NULL;
FINDREPLACEW g_fr = {
    sizeof(FINDREPLACEW),
    NULL,
    NULL,
    FR_DOWN | FR_HIDEWHOLEWORD,
    g_szFind, g_szReplace,
    ARRAYSIZE(g_szFind), ARRAYSIZE(g_szReplace),
};
const UINT g_FindMsg = RegisterWindowMessage(FINDMSGSTRING);

BOOL IsFindDialogMessage(LPMSG lpMsg)
{
    return IsDialogMessage(g_hFind, lpMsg);
}

void FindReplaceInit(HWND hOwner)
{
    g_fr.hwndOwner = hOwner;
}

void DoFindDlg()
{
    if (g_hFind == NULL)
    {
        g_fr.Flags &= ~FR_DIALOGTERM;
        g_hFind = FindTextW(&g_fr);
    }
    else
        SetFocus(g_hFind);
}

void DoReplaceDlg()
{
    if (g_hFind == NULL)
    {
        g_fr.Flags &= ~FR_DIALOGTERM;
        g_hFind = ReplaceTextW(&g_fr);
    }
    else
        SetFocus(g_hFind);
}

void DoFindNext()
{
    if (IsEmpty(g_fr.lpstrFindWhat))
    {
        g_fr.Flags &= ~FR_DIALOGTERM;
        g_hFind = FindTextW(&g_fr);
    }
    else
    {
        g_fr.Flags &= ~(FR_FINDNEXT | FR_REPLACE | FR_REPLACEALL | FR_DIALOGTERM);
        g_fr.Flags |= FR_FINDNEXT;
        SendMessage(g_fr.hwndOwner, g_FindMsg, 0, (LPARAM) &g_fr);
    }
}

void OnDlgTerm()
{
    g_hFind = NULL;
}

BOOL Find2(HWND hEdit, LPWSTR szTitle, const FINDREPLACEW* pfr)
{
    PWCHAR pstrFindWhat = pfr->lpstrFindWhat;

    if (IsEmpty(pstrFindWhat))
        return TRUE;

    HLOCAL hMem = (HLOCAL) SendMessage(hEdit, EM_GETHANDLE, 0, 0);

    DWORD dwSelStart, dwSelEnd;
    SendMessage(hEdit, EM_GETSEL, (WPARAM) &dwSelStart, (LPARAM) &dwSelEnd);

    PCWCHAR mbstr = (PCWCHAR) LocalLock(hMem);

    PCWCHAR found = pfr->Flags & FR_MATCHCASE
        ? (pfr->Flags & FR_DOWN
            ? StrStrW(mbstr + dwSelEnd, pstrFindWhat)
            : StrRStrW(mbstr, mbstr + dwSelStart, pstrFindWhat))
        : (pfr->Flags & FR_DOWN
            ? StrStrIW(mbstr + dwSelEnd, pstrFindWhat)
            : StrRStrIW(mbstr, mbstr + dwSelStart, pstrFindWhat));

    BOOL ret = FALSE;
    if (found != nullptr)
    {
        ret = TRUE;
        dwSelStart = (DWORD) (found - mbstr);
        dwSelEnd = (DWORD) (dwSelStart + wcslen(pstrFindWhat));
        SendMessage(hEdit, EM_SETSEL, (WPARAM) dwSelStart, (LPARAM) dwSelEnd);
        SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
    }

    LocalUnlock(hMem);

    return ret;
}

BOOL Replace2(HWND hEdit, LPWSTR szTitle, const FINDREPLACEW* pfr)
{
    PWCHAR pstrFindWhat = pfr->lpstrFindWhat;
    PWCHAR lpstrReplaceWith = pfr->lpstrReplaceWith;

    if (IsEmpty(pstrFindWhat))
        return TRUE;

    HLOCAL hMem = (HLOCAL) SendMessage(hEdit, EM_GETHANDLE, 0, 0);

    DWORD dwSelStart, dwSelEnd;
    SendMessage(hEdit, EM_GETSEL, (WPARAM) &dwSelStart, (LPARAM) &dwSelEnd);

    PCWCHAR mbstr = (PCWCHAR) LocalLock(hMem);

    DWORD len = (DWORD) wcslen(pstrFindWhat);
    if (pfr->Flags & FR_MATCHCASE
        ? StrCmpNW(mbstr + dwSelStart, pstrFindWhat, len) == 0
        : StrCmpNIW(mbstr + dwSelStart, pstrFindWhat, len) == 0)
        // NOTE Need a EM_REPLACESELW
    {
        SendMessage(hEdit, EM_REPLACESEL, TRUE, (LPARAM) lpstrReplaceWith);
        dwSelEnd = dwSelStart + (DWORD) wcslen(lpstrReplaceWith);
        SendMessage(hEdit, EM_SETSEL, (WPARAM) dwSelStart, (LPARAM) dwSelEnd);
    }

    LocalUnlock(hMem);

    return Find2(hEdit, szTitle, pfr);
}

void ShowFindFailed(LPTSTR szTitle, LPTSTR pstrFindWhat)
{
    TCHAR sBuffer[1024];
    FormatString(sBuffer, IDS_ERR_FIND, pstrFindWhat);
    MessageBox(g_hFind, sBuffer, szTitle, MB_OK | MB_ICONINFORMATION);
}

LRESULT OnFindMsg(HWND hEdit, LPTSTR szTitle, WPARAM wParam, LPARAM lParam)
{
    FINDREPLACEW* pfr = (FINDREPLACEW*) lParam;
    if (pfr->Flags & FR_DIALOGTERM)
        OnDlgTerm();
    else if (pfr->Flags & FR_FINDNEXT)
    {
        if (!Find2(hEdit, szTitle, pfr))
            ShowFindFailed(szTitle, pfr->lpstrFindWhat);
    }
    else if (pfr->Flags & FR_REPLACE)
    {
        if (!Replace2(hEdit, szTitle, pfr))
            ShowFindFailed(szTitle, pfr->lpstrFindWhat);
    }
    else if (pfr->Flags & FR_REPLACEALL)
    {
        while (!Replace2(hEdit, szTitle, pfr))
            ;
    }
    return 0;
}
