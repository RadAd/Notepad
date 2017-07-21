// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define STRICT
// Windows Header Files:
#include <windows.h>
//#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <Shobjidl.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO
#include <assert.h>
#define ASSERT(x) assert(x)
#ifdef NDEBUG
#define ENSURE(x) (x)
#else
#define ensure(expression) (void)(                                                       \
            (!!(expression)) ||                                                              \
            (_wassert(_CRT_WIDE(#expression), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0) \
        )
#define ENSURE(x) ensure(x)
#endif

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
