#pragma once

#define WC_RADEDIT                TEXT("RadEdit")

ATOM RegisterRadEdit(HINSTANCE hInstance);

inline void EditGetSel(HWND hEdit, LPDWORD pSelStart, LPDWORD pSelEnd)
{
    SendMessage(hEdit, EM_GETSEL, (WPARAM) pSelStart, (LPARAM) pSelEnd);
}

inline void EditSetLimitText(HWND hEdit, int nSize)
{
    SendMessage(hEdit, EM_SETLIMITTEXT, nSize, 0);
}

inline BOOL EditIsWordWrap(HWND hEdit)
{
    DWORD dwStyle = (DWORD) GetWindowLongPtr(hEdit, GWL_STYLE);
    return (dwStyle & (WS_HSCROLL | ES_AUTOHSCROLL)) == 0;
}

inline POINT EditPosFromChar(HWND hEdit, DWORD nChar)
{
    LRESULT r = SendMessage(hEdit, EM_POSFROMCHAR, (WPARAM) nChar, 0);
    return { LOWORD(r), HIWORD(r) };
}

inline DWORD EditCharFromPos(HWND hEdit, POINT pos, LPDWORD pLine = nullptr)
{
    LRESULT r = SendMessage(hEdit, EM_CHARFROMPOS, 0, MAKELPARAM(pos.x, pos.y));
    if (pLine != nullptr)
        *pLine = HIWORD(r);
    return LOWORD(r);
}

DWORD EditGetCursor(HWND hEdit);
