#pragma once

#define WC_RADEDIT                TEXT("RadEdit")

ATOM RegisterRadEdit(HINSTANCE hInstance);

inline void EditGetSel(HWND hEdit, LPDWORD pSelStart, LPDWORD pSelEnd)
{
    SendMessage(hEdit, EM_GETSEL, (WPARAM) pSelStart, (LPARAM) pSelEnd);
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
