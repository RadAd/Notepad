#include "stdafx.h"
#include "Notepad.h"
#include "FindReplace.h"
#include "StringX.h"
#include "FileIO.h"
#include "CommDlgX.h"
#include "RegX.h"
#include <windowsx.h>
#include <shellapi.h>

// TODO How to handle embedded '\0' in the file

// NOTE Edit control doesn't handle UNIX line endings
// NOTE Common control v6 always uses WCHAR in edit control

#define MAX_LOADSTRING 128

#define IDC_EDIT			1001
#define IDC_STATUS			1002

// Global Variables:
HINSTANCE g_hInst = NULL;                                // current instance
HKEY g_hReg = NULL;
TCHAR g_szTitle[MAX_LOADSTRING] = TEXT("");                  // The title bar text
TCHAR g_szUntitled[MAX_LOADSTRING] = TEXT("");
TCHAR g_szLineCol[MAX_LOADSTRING] = TEXT("");
TCHAR g_szFileName[MAX_PATH] = TEXT("");
Encoding g_Encoding = BOM_ANSI;

inline BOOL EditIsWordWrap(HWND hEdit)
{
    DWORD dwStyle = (DWORD) GetWindowLongPtr(hEdit, GWL_STYLE);
    return (dwStyle & (WS_HSCROLL | ES_AUTOHSCROLL)) == 0;
}

inline void EditReplaceHandle(HWND hEdit, HLOCAL hMem)
{
    HLOCAL hOldMem = Edit_GetHandle(hEdit);
    LocalFree(hOldMem);
    Edit_SetHandle(hEdit, hMem);
    InvalidateRect(hEdit, nullptr, TRUE);
}

inline void EditGetSel(HWND hEdit, LPDWORD pSelStart, LPDWORD pSelEnd)
{
    SendMessage(hEdit, EM_GETSEL, (WPARAM) pSelStart, (LPARAM) pSelEnd);
}

inline DWORD EditGetCursor(HWND hEdit)
{
    // NOTE There is no message to get the cursor position
    // This works by unselecting the reselecting
    SetWindowRedraw(hEdit, FALSE);
    DWORD nSelStart, nSelend;
    EditGetSel(hEdit, &nSelStart, &nSelend);
    Edit_SetSel(hEdit, -1, -1);
    DWORD nCursor;
    EditGetSel(hEdit, &nCursor, nullptr);
    if (nCursor == nSelStart)
        Edit_SetSel(hEdit, nSelend, nSelStart);
    else
        Edit_SetSel(hEdit, nSelStart, nSelend);
    SetWindowRedraw(hEdit, TRUE);
    return nCursor;
}

inline void EditSetLimitText(HWND hEdit, int nSize)
{
    SendMessage(hEdit, EM_SETLIMITTEXT, nSize, 0);
}

inline void StatusSetText(HWND hStatus, BYTE id, WORD nFlags, LPCTSTR pStr)
{
    SendMessage(hStatus, SB_SETTEXT, id | nFlags, (LPARAM) pStr);
}

inline void StatusSetParts(HWND hStatus, int nCount, LPINT pParts)
{
    SendMessage(hStatus, SB_SETPARTS, nCount, (LPARAM) pParts);
}

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance, const TCHAR* szWindowClass);
HWND                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    GoToLine(HWND, UINT, WPARAM, LPARAM);
void                FileOpen(HWND hWnd, HWND hEdit, LPCTSTR pszFileName);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE /*hPrevInstance*/,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    HWND hWndMain = InitInstance(hInstance, nCmdShow);
    if (hWndMain == NULL)
    {
        return FALSE;
    }

    if (!IsEmpty(lpCmdLine))
    {
        LPCTSTR    lpFileName = lpCmdLine;
        HWND hEdit = GetDlgItem(hWndMain, IDC_EDIT);
        if (PathIsRoot(lpFileName))
            FileOpen(hWndMain, hEdit, lpFileName);
        else
        {
            TCHAR strCurrentDirectory[MAX_PATH];
            TCHAR strFileName[MAX_PATH];
            GetCurrentDirectory(ARRAYSIZE(strCurrentDirectory), strCurrentDirectory);
            PathCombine(strFileName, strCurrentDirectory, lpFileName);
            FileOpen(hWndMain, hEdit, strFileName);
        }
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NOTEPAD));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!IsFindDialogMessage(&msg) &&
            !TranslateAccelerator(hWndMain, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    RegCloseKey(g_hReg);

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance, const TCHAR* szWindowClass)
{
    WNDCLASS wc;
    wc.style          = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc    = WndProc;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = 0;
    wc.hInstance      = hInstance;
    wc.hIcon          = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_NOTEPAD));
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH) (COLOR_WINDOW+1);
    wc.lpszMenuName   = MAKEINTRESOURCE(IDC_NOTEPAD);
    wc.lpszClassName  = szWindowClass;

    return RegisterClass(&wc);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_hInst = hInstance; // Store instance handle in our global variable

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_LINK_CLASS;
    InitCommonControlsEx(&icc);

    PCTSTR pszWindowClass = TEXT("NOTEPAD");
    MyRegisterClass(hInstance, pszWindowClass);

    LoadString(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDS_UNTITLED, g_szUntitled, MAX_LOADSTRING);
    LoadString(hInstance, IDS_LINE_COL, g_szLineCol, MAX_LOADSTRING);

    RegCreateKey(HKEY_CURRENT_USER, TEXT("Software\\RadSoft\\Notepad"), &g_hReg);

    RECT r;
    r.top = RegQueryDWORD(g_hReg, TEXT("iWindowPosY"), CW_USEDEFAULT);
    r.left = RegQueryDWORD(g_hReg, TEXT("iWindowPosX"), CW_USEDEFAULT);
    r.bottom = RegQueryDWORD(g_hReg, TEXT("iWindowPosDY"), CW_USEDEFAULT);
    r.right = RegQueryDWORD(g_hReg, TEXT("iWindowPosDX"), CW_USEDEFAULT);
    HWND hWnd = CreateWindowEx(WS_EX_ACCEPTFILES, pszWindowClass, g_szTitle, WS_OVERLAPPEDWINDOW,
        r.left, r.top, r.right, r.bottom, nullptr, nullptr, hInstance, nullptr);

    if (hWnd)
    {
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);
    }

    return hWnd;
}

void SetFileName(HWND hMain, LPCTSTR pszFileName, Encoding eEncoding)
{
    _tcscpy_s(g_szFileName, pszFileName);
    g_Encoding = eEncoding;
    LPTSTR pFileName = IsEmpty(g_szFileName) ? g_szUntitled : PathFindFileName(g_szFileName);
    TCHAR sBuffer[MAX_LOADSTRING + MAX_PATH];
    FormatString(sBuffer, IDS_WND_TITLE, pFileName, g_szTitle);
    SetWindowText(hMain, sBuffer);
}

HFONT CreateFont(HWND hWnd)
{
    const LONG PointSize = RegQueryDWORD(g_hReg, TEXT("iPointSize"), 120);

    HDC hDC = GetDC(hWnd);

    LOGFONT lFont = {};
    lFont.lfHeight = -MulDiv(PointSize, GetDeviceCaps(hDC, LOGPIXELSY), 720);
    lFont.lfCharSet = (BYTE) RegQueryDWORD(g_hReg, TEXT("lfCharSet"), DEFAULT_CHARSET);
    lFont.lfClipPrecision = (BYTE) RegQueryDWORD(g_hReg, TEXT("lfClipPrecision"), CLIP_DEFAULT_PRECIS);
    lFont.lfEscapement = RegQueryDWORD(g_hReg, TEXT("lfEscapement"), 0);
    RegQuerySZ(g_hReg, TEXT("lfFaceName"), lFont.lfFaceName, ARRAYSIZE(lFont.lfFaceName), TEXT("Consolas"));
    lFont.lfItalic = (BYTE) RegQueryDWORD(g_hReg, TEXT("lfItalic"), FALSE);
    lFont.lfOrientation = RegQueryDWORD(g_hReg, TEXT("lfOrientation"), 0);
    lFont.lfOutPrecision = (BYTE) RegQueryDWORD(g_hReg, TEXT("lfOutPrecision"), OUT_DEFAULT_PRECIS);
    lFont.lfPitchAndFamily = (BYTE) RegQueryDWORD(g_hReg, TEXT("lfPitchAndFamily"), FIXED_PITCH | FF_MODERN);
    lFont.lfQuality = (BYTE) RegQueryDWORD(g_hReg, TEXT("lfQuality"), DEFAULT_QUALITY);
    lFont.lfStrikeOut = (BYTE) RegQueryDWORD(g_hReg, TEXT("lfStrikeOut"), FALSE);
    lFont.lfUnderline = (BYTE) RegQueryDWORD(g_hReg, TEXT("lfUnderline"), FALSE);
    lFont.lfWeight = RegQueryDWORD(g_hReg, TEXT("lfWeight"), FW_NORMAL);

    ReleaseDC(hWnd, hDC);

    return CreateFontIndirect(&lFont);
}

HWND CreateEditControl(HWND hWnd, HFONT hFont, BOOL bWordWrap)
{
    DWORD dwWordWrap = bWordWrap ? 0 : WS_HSCROLL | ES_AUTOHSCROLL;
    HWND hEdit = CreateWindow(WC_EDIT, TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL | dwWordWrap,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWnd, (HMENU) IDC_EDIT, g_hInst, NULL);

    EditSetLimitText(hEdit, 0);

    SetWindowFont(hEdit, hFont, TRUE);

    SetFocus(hEdit);

    return hEdit;
}

HWND CreateStatusControl(HWND hWnd)
{
    HWND hStatus = CreateWindow(STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWnd, (HMENU) IDC_STATUS, g_hInst, NULL);
    StatusSetText(hStatus, 0, SBT_POPOUT, NULL);
    return hStatus;
}

void FileOpen(HWND hWnd, HWND hEdit, LPCTSTR pszFileName)
{
    Encoding eEncoding = BOM_ANSI;
    HLOCAL hMem = LoadFile(pszFileName, &eEncoding);
    EditReplaceHandle(hEdit, hMem);
    SetFileName(hWnd, pszFileName, eEncoding);
}

void FileSaveAs(HWND hWnd, HWND hEdit)
{
    TCHAR szFileName[MAX_PATH] = TEXT("");
    _tcscpy_s(szFileName, g_szFileName);
    Encoding eEncoding = g_Encoding;
    if (SelectFilename(hWnd, FALSE, szFileName, ARRAYSIZE(szFileName), &eEncoding))
    {
        HLOCAL hMem = Edit_GetHandle(hEdit);
        SaveFile(hMem, szFileName, eEncoding);
        Edit_SetModify(hEdit, FALSE);
        Edit_EmptyUndoBuffer(hEdit);
        SetFileName(hWnd, szFileName, eEncoding);
    }
}

void FileSave(HWND hWnd, HWND hEdit)
{
    if (!IsEmpty(g_szFileName))
    {
        HLOCAL hMem = Edit_GetHandle(hEdit);
        SaveFile(hMem, g_szFileName, g_Encoding);
        Edit_SetModify(hEdit, FALSE);
        Edit_EmptyUndoBuffer(hEdit);
    }
    else
        FileSaveAs(hWnd, hEdit);
}

BOOL CheckSave(HWND hWnd, HWND hEdit)
{
    BOOL fModified = Edit_GetModify(hEdit);
    if (fModified)
    {
        // TODO [Save] [Don't Save] [Cancel]
        LPTSTR pFileName = IsEmpty(g_szFileName) ? g_szUntitled : PathFindFileName(g_szFileName);
        TCHAR sBuffer[MAX_LOADSTRING + MAX_PATH];
        FormatString(sBuffer, IDS_ASK_SAVE, pFileName);
        int id = MessageBox(hWnd, sBuffer, g_szTitle, MB_YESNOCANCEL);
        if (id == IDYES)
            FileSave(hWnd, hEdit);
        return id != IDCANCEL;
    }
    else
        return TRUE;
}

void UpdateCursorInfo(HWND hEdit, HWND hStatus)
{
    static bool bIn = false;    // Stop infinite loop from EditGetCursor changing selection
    static DWORD dwOldCursor = 0;
    if (hStatus != NULL && !bIn)
    {
        bIn = true;
        DWORD dwCursor = EditGetCursor(hEdit);
        if (dwOldCursor != dwCursor)
        {
            dwOldCursor = dwCursor;
            DWORD dwLine = Edit_LineFromChar(hEdit, dwCursor);
            DWORD dwLineIndex = Edit_LineIndex(hEdit, dwLine);
            TCHAR sBuffer[MAX_LOADSTRING];
            _sntprintf_s(sBuffer, ARRAYSIZE(sBuffer), g_szLineCol, dwLine + 1, dwCursor - dwLineIndex + 1);
            StatusSetText(hStatus, 1, 0, sBuffer);
        }
        bIn = false;
    }
}

LRESULT CALLBACK EditExProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    static DWORD nPos = 0;
    LRESULT ret = DefSubclassProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case EM_SETSEL:
    case EM_UNDO:
    case WM_PASTE:
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        {
            HWND hParent = GetParent(hWnd);
            HWND hStatus = GetDlgItem(hParent, IDC_STATUS);;
            UpdateCursorInfo(hWnd, hStatus);
        }
        break;
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON && nPos != (DWORD) lParam)
        {
            HWND hParent = GetParent(hWnd);
            HWND hStatus = GetDlgItem(hParent, IDC_STATUS);;
            UpdateCursorInfo(hWnd, hStatus);
        }
        nPos = (DWORD) lParam;
        break;
    }

    return ret;
}

void PositionWindows(HWND hWnd, HWND hEdit, HWND hStatus)
{
    RECT rcClient = {};
    GetClientRect(hWnd, &rcClient);
    RECT rcStatus = {};
    if (hStatus)
        GetClientRect(hStatus, &rcStatus);

    SetWindowPos(hEdit, NULL, 0, 0, rcClient.right, rcClient.bottom - (rcStatus.bottom - rcStatus.top), SWP_NOZORDER);

    if (hStatus != NULL)
    {
        SetWindowPos(hStatus, NULL, rcClient.bottom - (rcStatus.bottom - rcStatus.top), 0, rcClient.right, rcClient.bottom, SWP_NOZORDER);

        int r = rcClient.right - rcClient.left - 200;
        int Parts[] = { r, -1 };
        StatusSetParts(hStatus, ARRAYSIZE(Parts), Parts);
        UpdateCursorInfo(hEdit, hStatus);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND hEdit = GetDlgItem(hWnd, IDC_EDIT);
    HWND hStatus = GetDlgItem(hWnd, IDC_STATUS);

    switch (message)
    {
    case WM_CREATE:
        {
            SetFileName(hWnd, TEXT(""), BOM_ANSI);

            FindReplaceInit(hWnd);

            HFONT hFont = CreateFont(hWnd);
            BOOL bWrap = RegQueryDWORD(g_hReg, TEXT("fWrap"), FALSE);
            hEdit = CreateEditControl(hWnd, hFont, bWrap);
            BOOL bStatus = RegQueryDWORD(g_hReg, TEXT("StatusBar"), FALSE);
            if (bStatus)
                hStatus = CreateStatusControl(hWnd);
            SetWindowSubclass(hEdit, EditExProc, 0, 0);

            return hEdit != NULL ? 0 : -1;
        }
        break;
    case WM_CLOSE:
        if (CheckSave(hWnd, hEdit))
            return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    case WM_SIZE:
        PositionWindows(hWnd, hEdit, hStatus);
        break;
    case WM_SETFOCUS:
        SetFocus(hEdit);
        break;
    case WM_DROPFILES:
        {
            HDROP hDropInfo = (HDROP) wParam;

            SetForegroundWindow(hWnd);
            UINT nFiles = ::DragQueryFile(hDropInfo, (UINT) -1, NULL, 0);
            if (nFiles > 0)
            {
                TCHAR szFileName[_MAX_PATH];
                ::DragQueryFile(hDropInfo, 0, szFileName, _MAX_PATH);
                FileOpen(hWnd, hEdit, szFileName);
            }
            ::DragFinish(hDropInfo);
        }
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case ID_FILE_NEW:
                if (CheckSave(hWnd, hEdit))
                {
                    HLOCAL hMem = NewFile();
                    EditReplaceHandle(hEdit, hMem);
                    SetFileName(hWnd, TEXT(""), BOM_ANSI);
                }
                break;
            case ID_FILE_OPEN:
                if (CheckSave(hWnd, hEdit))
                {
                    TCHAR szFileName[MAX_PATH] = TEXT("");
                    Encoding eEncoding = BOM_ANSI;
                    if (SelectFilename(hWnd, TRUE, szFileName, ARRAYSIZE(szFileName), &eEncoding))
                    {
                        // TODO How to use selected encoding?
                        FileOpen(hWnd, hEdit, szFileName);
                    }
                }
                break;
            case ID_FILE_SAVE:
                FileSave(hWnd, hEdit);
                break;
            case ID_FILE_SAVEAS:
                FileSaveAs(hWnd, hEdit);
                break;
            case IDM_EDIT_UNDO:
                Edit_Undo(hEdit);
                break;
            case ID_EDIT_CUT:
                FORWARD_WM_CUT(hEdit, SNDMSG);
                break;
            case ID_EDIT_COPY:
                FORWARD_WM_COPY(hEdit, SNDMSG);
                break;
            case ID_EDIT_PASTE:
                FORWARD_WM_PASTE(hEdit, SNDMSG);
                break;
            case ID_EDIT_DELETE:
                FORWARD_WM_CLEAR(hEdit, SNDMSG);
                break;
            case ID_EDIT_FIND:
                DoFindDlg();
                break;
            case ID_EDIT_FINDNEXT:
                DoFindNext();
                break;
            case ID_EDIT_REPLACE:
                DoReplaceDlg();
                break;
            case ID_EDIT_GOTO:
                {
                    DWORD dwCursor = EditGetCursor(hEdit);
                    DWORD dwLine = Edit_LineFromChar(hEdit, dwCursor) + 1;
                    if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_GOTOLINE), hWnd, GoToLine, (LPARAM) &dwLine) == IDOK)
                    {
                        if (dwLine <= 0 || dwLine > Edit_GetLineCount(hEdit))
                        {
                            TCHAR sErrGoto[MAX_LOADSTRING];
                            LoadString(g_hInst, IDS_ERRGOTO, sErrGoto, MAX_LOADSTRING);
                            MessageBox(hWnd, sErrGoto, g_szTitle, MB_OK | MB_ICONERROR);
                        }
                        else
                        {
                            DWORD dwLineIndex = Edit_LineIndex(hEdit, dwLine - 1);
                            Edit_SetSel(hEdit, dwLineIndex, dwLineIndex);
                            Edit_ScrollCaret(hEdit);
                        }
                    }
                }
                break;
            case IDM_EDIT_SELECTALL:
                Edit_SetSel(hEdit, 0, -1);
                break;
            case ID_FORMAT_WORDWRAP:
                {
                    BOOL bWordWrap = EditIsWordWrap(hEdit);

                    HLOCAL hMem = Edit_GetHandle(hEdit);
                    Edit_SetHandle(hEdit, NewFile());
                    BOOL fModified = Edit_GetModify(hEdit);
                    HFONT hFont = GetWindowFont(hEdit);
                    DestroyWindow(hEdit);

                    bWordWrap = !bWordWrap;

                    hEdit = CreateEditControl(hWnd, hFont, bWordWrap);
                    Edit_SetHandle(hEdit, hMem);
                    Edit_SetModify(hEdit, fModified);

                    PositionWindows(hWnd, hEdit, hStatus);

                    RegSetDWORD(g_hReg, TEXT("fWrap"), bWordWrap);
                }
                break;
            case ID_FORMAT_FONT:
                {
                    HFONT hFont = GetWindowFont(hEdit);
                    HFONT hNewFont = ChooseFont(hWnd, hFont);
                    if (hNewFont != NULL)
                    {
                        DeleteFont(hFont);
                        SetWindowFont(hEdit, hNewFont, TRUE);

                        HDC hDC = GetDC(hWnd);

                        LOGFONT lFont;
                        GetObject(hNewFont, sizeof(LOGFONT), &lFont);
                        const LONG PointSize = -MulDiv(lFont.lfHeight, 720, GetDeviceCaps(hDC, LOGPIXELSY));
                        RegSetDWORD(g_hReg, TEXT("iPointSize"), PointSize);
                        RegSetDWORD(g_hReg, TEXT("lfCharSet"), lFont.lfCharSet);
                        RegSetDWORD(g_hReg, TEXT("lfClipPrecision"), lFont.lfClipPrecision);
                        RegSetDWORD(g_hReg, TEXT("lfEscapement"), lFont.lfEscapement);
                        RegSetSZ(g_hReg, TEXT("lfFaceName"), lFont.lfFaceName);
                        RegSetDWORD(g_hReg, TEXT("lfItalic"), lFont.lfItalic);
                        RegSetDWORD(g_hReg, TEXT("lfOrientation"), lFont.lfOrientation);
                        RegSetDWORD(g_hReg, TEXT("lfOutPrecision"), lFont.lfOutPrecision);
                        RegSetDWORD(g_hReg, TEXT("lfPitchAndFamily"), lFont.lfPitchAndFamily);
                        RegSetDWORD(g_hReg, TEXT("lfQuality"), lFont.lfQuality);
                        RegSetDWORD(g_hReg, TEXT("lfStrikeOut"), lFont.lfStrikeOut);
                        RegSetDWORD(g_hReg, TEXT("lfUnderline"), lFont.lfUnderline);
                        RegSetDWORD(g_hReg, TEXT("lfWeight"), lFont.lfWeight);

                        ReleaseDC(hWnd, hDC);
                    }
                }
                break;
            case ID_VIEW_STATUSBAR:
                if (hStatus != NULL)
                {
                    DestroyWindow(hStatus);
                    hStatus = NULL;
                }
                else
                    hStatus = CreateStatusControl(hWnd);
                PositionWindows(hWnd, hEdit, hStatus);
                RegSetDWORD(g_hReg, TEXT("StatusBar"), hStatus != NULL);
                break;
            case IDM_ABOUT:
                DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                //DestroyWindow(hWnd);
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                break;
            case IDC_EDIT:
                switch (HIWORD(wParam))
                {
                case EN_ERRSPACE:
                    {
                        TCHAR sErrSpace[MAX_LOADSTRING];
                        LoadString(g_hInst, IDS_ERRSPACE, sErrSpace, MAX_LOADSTRING);
                        MessageBox(hWnd, sErrSpace, g_szTitle, MB_OK | MB_ICONERROR);
                    }
                    break;
                }
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_INITMENUPOPUP:
        {
            HMENU hMenu = (HMENU) wParam;
            // Could Iterate through menus instead?

            BOOL bCanUndo = Edit_CanUndo(hEdit);
            EnableMenuItem(hMenu, IDM_EDIT_UNDO, MF_BYCOMMAND | (bCanUndo ? MF_ENABLED : MF_DISABLED));

            DWORD dwSelStart, dwSelEnd;
            EditGetSel(hEdit, &dwSelStart, &dwSelEnd);
            BOOL bSelected = dwSelEnd > dwSelStart;
            EnableMenuItem(hMenu, ID_EDIT_CUT, MF_BYCOMMAND | (bSelected ? MF_ENABLED : MF_DISABLED));
            EnableMenuItem(hMenu, ID_EDIT_COPY, MF_BYCOMMAND | (bSelected ? MF_ENABLED : MF_DISABLED));
            EnableMenuItem(hMenu, ID_EDIT_DELETE, MF_BYCOMMAND | (bSelected ? MF_ENABLED : MF_DISABLED));

            UINT Formats[] = { CF_UNICODETEXT, CF_TEXT, CF_OEMTEXT };
            BOOL bClipboard = GetPriorityClipboardFormat(Formats, ARRAYSIZE(Formats)) > 0;
            EnableMenuItem(hMenu, ID_EDIT_PASTE, MF_BYCOMMAND | (bSelected ? MF_ENABLED : MF_DISABLED));

            BOOL bWordWrap = EditIsWordWrap(hEdit);
            CheckMenuItem(hMenu, ID_FORMAT_WORDWRAP, MF_BYCOMMAND | (bWordWrap ? MF_CHECKED : MF_UNCHECKED));

            CheckMenuItem(hMenu, ID_VIEW_STATUSBAR, MF_BYCOMMAND | ((hStatus != NULL) ? MF_CHECKED : MF_UNCHECKED));
        }
        break;
    case WM_DESTROY:
        {
            RECT r;
            GetWindowRect(hWnd, &r);

            RegSetDWORD(g_hReg, TEXT("iWindowPosY"), r.top);
            RegSetDWORD(g_hReg, TEXT("iWindowPosX"), r.left);
            RegSetDWORD(g_hReg, TEXT("iWindowPosDY"), r.bottom - r.top);
            RegSetDWORD(g_hReg, TEXT("iWindowPosDX"), r.right - r.left);

            HFONT hFont = GetWindowFont(hEdit);
            DeleteFont(hFont);
            PostQuitMessage(0);
        }
        break;
    default:
        if (message == g_FindMsg)
            return OnFindMsg(hEdit, g_szTitle, wParam, lParam);
        else
          return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR) TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR) TRUE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}

INT_PTR CALLBACK GoToLine(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    PDWORD pdwLine = (PDWORD) GetWindowLongPtr(hDlg, GWLP_USERDATA);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
            pdwLine = (PDWORD) lParam;
            SetDlgItemInt(hDlg, IDC_LINE, *pdwLine, FALSE);
        }
        return (INT_PTR) TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            {
                BOOL bTranslated = FALSE;
                *pdwLine = GetDlgItemInt(hDlg, IDC_LINE, &bTranslated, FALSE);
                if (bTranslated)
                    EndDialog(hDlg, LOWORD(wParam));
            }
            break;
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR) TRUE;
        }
        break;
    }
    return (INT_PTR) FALSE;
}
