#include "stdafx.h"
#include "RadEdit.h"
#include "Text.h"
#include <windowsx.h>
#include <intsafe.h>

LONG GetWidth(const RECT& r)
{
    return r.right - r.left;
}

LONG GetHeight(const RECT& r)
{
    return r.bottom - r.top;
}

SIZE ToSize(DWORD s)
{
    return { LOWORD(s), HIWORD(s) };
}

POINT ToPoint(LPARAM p)
{
    return { GET_X_LPARAM(p), GET_Y_LPARAM(p) };
}

int MyGetScrollPos(HWND hWnd, int nBar)
{
    SCROLLINFO info = { sizeof(SCROLLINFO), SIF_POS };
    ENSURE(GetScrollInfo(hWnd, nBar, &info));
    return info.nPos;
}

UINT MyGetScrollPage(HWND hWnd, int nBar)
{
    SCROLLINFO info = { sizeof(SCROLLINFO), SIF_PAGE };
    ENSURE(GetScrollInfo(hWnd, nBar, &info));
    return info.nPage;
}

int GetTabbedTextExtentEx(_In_ HDC hDC,
    _In_reads_(chCount) LPCWSTR lpString,
    _In_ int chCount,
    _In_ int nTabPositions,
    _In_reads_opt_(nTabPositions) INT *lpnTabStopPositions,
    int x)
{
    // TODO Use a binary search method
    for (int i = 0; i < chCount; ++i)
    {
        SIZE s = ToSize(GetTabbedTextExtent(hDC, lpString, i, nTabPositions, lpnTabStopPositions));
        if (s.cx >= x)
            return i;
    }
    return chCount;
}

void NotifyParent(HWND hWnd, int code)
{
    SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), code), (LPARAM) hWnd);
}

LRESULT CALLBACK RadEditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

ATOM RegisterRadEdit(HINSTANCE hInstance)
{
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = RadEditWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WC_RADEDIT;

    return RegisterClass(&wc);
}

struct RadEdit
{
    HFONT m_hFont = NULL;
    TEXTMETRIC m_TextMetrics = {};
    HLOCAL m_hText = NULL;
    BOOL m_bModify = FALSE;
    int m_nMarginLeft = EC_USEFONTINFO;
    int m_nMarginRight = EC_USEFONTINFO;
    int m_nTabStops = 0;
    INT m_rgTabStops[256];
    DWORD m_nSelStart = 0;
    DWORD m_nSelEnd = 0;
    EDITWORDBREAKPROC m_pEditWordBreakProc = nullptr;

    int LineHeight() const { return m_TextMetrics.tmHeight; }
    int AveCharWidth() const { return m_TextMetrics.tmAveCharWidth; }
    int MarginLeft() const { return m_nMarginLeft == EC_USEFONTINFO ? m_TextMetrics.tmOverhang : m_nMarginLeft; }
    int MarginRight() const { return m_nMarginRight == EC_USEFONTINFO ? m_TextMetrics.tmOverhang : m_nMarginRight; }

    void CalcScrollBars(HWND hWnd) const;
    SIZE CalcTextSize(HDC hDC, PCWSTR buffer, int nCount) const;
    void MoveCaret(HWND hWnd) const;
    void Draw(HWND hWnd, HDC hDC) const;
    void ReplaceSel(HWND hWnd, PCWSTR pText, BOOL bStoreUndo);

    void OnScroll(HWND hWnd, UINT nSBCode, UINT /*nPos*/, HWND hScrollBar, UINT nBar) const;
    void OnSetFont(HWND hWnd, HFONT hFont, BOOL bRedraw);
    void OnKey(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
    void OnChar(HWND hWnd, TCHAR ch, int cRepeat);

    LRESULT OnGetSel(LPDWORD pSelStart, LPDWORD pSelEnd) const;
    void OnSetSel(HWND hWnd, DWORD nSelStart, DWORD nSelEnd);

    LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

void RadEdit::CalcScrollBars(HWND hWnd) const
{
    SIZE s = {};
    PCWSTR const buffer = (PCWSTR) GlobalLock(m_hText);
    int nIndex = 0;
    while (nIndex >= 0)
    {
        int nNext = GetNextLine(buffer, nIndex);
        if (nNext >= 0)
        {
            int x = nNext - nIndex - 1;
            if (x > s.cx)
                s.cx = x;
            nIndex = nNext;
        }
        else
        {
            int x = (int) wcslen(buffer + nIndex);
            if (x > s.cx)
                s.cx = x;
            nIndex = -1;
        }
        ++s.cy;
    }
    GlobalUnlock(m_hText);

    RECT cr = {};
    GetClientRect(hWnd, &cr);

    SCROLLINFO vinfo = { sizeof(SCROLLINFO), SIF_ALL }, hinfo = { sizeof(SCROLLINFO), SIF_ALL };
    ENSURE(GetScrollInfo(hWnd, SB_VERT, &vinfo));
    ENSURE(GetScrollInfo(hWnd, SB_HORZ, &hinfo));

    vinfo.fMask |= SIF_DISABLENOSCROLL;
    hinfo.fMask |= SIF_DISABLENOSCROLL;
    vinfo.nMax = s.cy - 1;
    vinfo.nPage = GetHeight(cr) / LineHeight();
    hinfo.nMax = s.cx;
    hinfo.nPage = GetWidth(cr) / AveCharWidth();

    SetScrollInfo(hWnd, SB_VERT, &vinfo, TRUE);
    SetScrollInfo(hWnd, SB_HORZ, &hinfo, TRUE);
}

SIZE RadEdit::CalcTextSize(HDC hDC, PCWSTR buffer, int nCount) const
{
    return ToSize(GetTabbedTextExtent(hDC, buffer, nCount, m_nTabStops, const_cast<LPINT>(m_rgTabStops)));
}

void RadEdit::MoveCaret(HWND hWnd) const
{
    POINT p = EditPosFromChar(hWnd, m_nSelEnd);
    SetCaretPos(p.x, p.y);
}

void RadEdit::Draw(HWND hWnd, HDC hDC) const
{
    HFONT of = (HFONT) SelectObject(hDC, m_hFont);

    /*HBRUSH hBgBrush = (HBRUSH)*/ SendMessage(GetParent(hWnd), WM_CTLCOLOREDIT, (WPARAM) hDC, (LPARAM) hWnd);
    //::DeleteObject(hBgBrush);

    // TODO Use margins here and in calcscrollbars

    RECT cr = {};
    GetClientRect(hWnd, &cr);

    DWORD nSelStart, nSelEnd;
    OnGetSel(&nSelStart, &nSelEnd);

    POINT p = { -MyGetScrollPos(hWnd, SB_HORZ) * AveCharWidth() + MarginLeft(), 0 };
    PCWSTR const buffer = (PCWSTR) GlobalLock(m_hText);
    const int nFirstLine = Edit_GetFirstVisibleLine(hWnd);  // TODO Use message ???
    int nIndex = TextLineIndex(m_hText, nFirstLine);
    while (nIndex >= 0)
    {
        const int nLineEnd = GetLineEnd(buffer, nIndex);
        const int nCount = nLineEnd - nIndex;
        TabbedTextOut(hDC, p.x, p.y, buffer + nIndex, nCount, m_nTabStops, m_rgTabStops, MarginLeft());

        const DWORD nSelLineStart = min(max((int) nSelStart, nIndex), nIndex + nCount);
        const DWORD nSelLineEnd = min(max((int) nSelEnd, nIndex), nIndex + nCount);
        if (nSelLineStart != nSelLineEnd)
        {
            int save = SaveDC(hDC);
            SIZE sBegin(CalcTextSize(hDC, buffer + nIndex, nSelLineStart - nIndex));
            SIZE sEnd(CalcTextSize(hDC, buffer + nIndex, nSelLineEnd - nIndex));
            IntersectClipRect(hDC, p.x + sBegin.cx, p.y, p.x + sEnd.cx, p.y + LineHeight());
            SetBkColor(hDC, GetSysColor(COLOR_HIGHLIGHT));
            SetTextColor(hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
            TabbedTextOut(hDC, p.x, p.y, buffer + nIndex, nCount, m_nTabStops, m_rgTabStops, MarginLeft());
            RestoreDC(hDC, save);
        }

        nIndex = GetNextLine(buffer, nLineEnd);
        p.y += LineHeight();
        if ((p.y + m_TextMetrics.tmHeight) > cr.bottom)
            break;
    }
    GlobalUnlock(m_hText);

    SelectObject(hDC, of);
}

void RadEdit::ReplaceSel(HWND hWnd, PCWSTR pText, BOOL /*bStoreUndo*/)
{
    // TODO bStoreUndo

    DWORD nSelStart, nSelEnd;
    OnGetSel(&nSelStart, &nSelEnd);
    if (HLOCAL hTextNew = TextReplace(m_hText, nSelStart, nSelEnd, pText))
    {
        m_hText = hTextNew;
        m_bModify = TRUE;
        Edit_SetSel(hWnd, nSelEnd, nSelEnd);
        InvalidateRect(hWnd, nullptr, TRUE);
        NotifyParent(hWnd, EN_CHANGE);
    }
    else
        NotifyParent(hWnd, EN_ERRSPACE);
}

UINT DoScroll(UINT nSBCode, const SCROLLINFO& info)
{
    int curpos = info.nPos;

    // Determine the new position of scroll box.
    switch (nSBCode)
    {
    case SB_LEFT:      // Scroll to far left.
        curpos = info.nMin;
        break;

    case SB_RIGHT:      // Scroll to far right.
        curpos = info.nMax;
        break;

    case SB_ENDSCROLL:   // End scroll.
        break;

    case SB_LINELEFT:      // Scroll left.
        if (curpos > info.nMin)
            curpos--;
        break;

    case SB_LINERIGHT:   // Scroll right.
        if (curpos < info.nMax)
            curpos++;
        break;

    case SB_PAGELEFT:    // Scroll one page left.
        if (curpos > info.nMin)
            curpos = max(info.nMin, curpos - MulDiv(info.nPage, 9, 10));
        break;

    case SB_PAGERIGHT:      // Scroll one page right.
        if (curpos < info.nMax)
            curpos = min(info.nMax, curpos + MulDiv(info.nPage, 9, 10));
        break;

    case SB_THUMBPOSITION:          // Scroll to absolute position. nTrackPos is the position
        curpos = info.nTrackPos;    // of the scroll box at the end of the drag operation.
        break;

    case SB_THUMBTRACK:             // Drag scroll box to specified position. nTrackPos is the
        curpos = info.nTrackPos;    // position that the scroll box has been dragged to.
        break;
    }

    return curpos;
}

void RadEdit::OnScroll(HWND hWnd, UINT nSBCode, UINT /*nPos*/, HWND hScrollBar, UINT nBar) const
{
    if (hScrollBar == NULL)
        hScrollBar = hWnd;
    else
        nBar = SB_CTL;

    SCROLLINFO   info = { sizeof(SCROLLINFO), SIF_ALL };
    ENSURE(GetScrollInfo(hScrollBar, nBar, &info));

    info.nPos = DoScroll(nSBCode, info);

    SetScrollInfo(hScrollBar, nBar, &info, TRUE);

    MoveCaret(hWnd);
    InvalidateRect(hWnd, nullptr, TRUE);
}

void RadEdit::OnSetFont(HWND hWnd, HFONT hFont, BOOL bRedraw)
{
    if (hFont == NULL)
        hFont = (HFONT) GetStockObject(SYSTEM_FONT);

    const int ocs = AveCharWidth();

    m_hFont = hFont;

    {
        HDC hDC = GetWindowDC(hWnd);
        HFONT of = (HFONT) SelectObject(hDC, m_hFont);
        GetTextMetrics(hDC, &m_TextMetrics);
        SelectObject(hDC, of);
        ReleaseDC(hWnd, hDC);
    }

    {
        const int cs = AveCharWidth();
        for (int i = 0; i < m_nTabStops; ++i)
            m_rgTabStops[i] = MulDiv(m_rgTabStops[i], cs, ocs);
    }

    if (bRedraw)
        InvalidateRect(hWnd, nullptr, TRUE);
}

void RadEdit::OnKey(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
    bool bShift = (GetKeyState(VK_SHIFT) & KF_UP) != 0;
    bool bCtrl = (GetKeyState(VK_CONTROL) & KF_UP) != 0;
    bool bAlt = (GetKeyState(VK_MENU) & KF_UP) != 0;

    if (!fDown)
        return;

    switch (vk)
    {
    case VK_LEFT:
        // TODO Move over end of line
        if (!bAlt)
        {
            // TODO if (bCtrl) Move to prev word
            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            nSelEnd = GetPrevChar(m_hText, nSelEnd);
            if (bCtrl && nSelEnd >= 0)
            {
                //EDITWORDBREAKPROC ewb = m_pEditWordBreakProc != nullptr ? m_pEditWordBreakProc : DefaultEditWordBreakProc;
                EDITWORDBREAKPROC ewb = &DefaultEditWordBreakProc;
                nSelEnd = MoveWord(ewb, WB_LEFT, m_hText, nSelEnd);
            }
            if (nSelEnd >= 0)
            {
                if (!bShift)
                    nSelStart = nSelEnd;
                Edit_SetSel(hWnd, nSelStart, nSelEnd);
                Edit_ScrollCaret(hWnd);
            }
        }
        break;

    case VK_RIGHT:
        // TODO Move over end of line
        if (!bAlt)
        {
            // TODO if (bCtrl) Move to next word
            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            nSelEnd = GetNextChar(m_hText, nSelEnd);
            if (bCtrl && nSelEnd >= 0)
            {
                //EDITWORDBREAKPROC ewb = m_pEditWordBreakProc != nullptr ? m_pEditWordBreakProc : DefaultEditWordBreakProc;
                EDITWORDBREAKPROC ewb = DefaultEditWordBreakProc;
                nSelEnd = MoveWord(ewb, WB_RIGHT, m_hText, nSelEnd);
            }
            if (nSelEnd >= 0)
            {
                if (!bShift)
                    nSelStart = nSelEnd;
                Edit_SetSel(hWnd, nSelStart, nSelEnd);
                Edit_ScrollCaret(hWnd);
            }
        }
        break;

    case VK_UP:
        if (!bAlt)
        {
            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            POINT pos(EditPosFromChar(hWnd, nSelEnd));
            pos.y -= LineHeight();
            nSelEnd = EditCharFromPos(hWnd, pos);
            if (!bShift)
                nSelStart = nSelEnd;
            Edit_SetSel(hWnd, nSelStart, nSelEnd);
            Edit_ScrollCaret(hWnd);
        }
        break;

    case VK_DOWN:
        if (!bAlt)
        {
            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            POINT pos(EditPosFromChar(hWnd, nSelEnd));
            pos.y += LineHeight();
            nSelEnd = EditCharFromPos(hWnd, pos);
            if (!bShift)
                nSelStart = nSelEnd;
            Edit_SetSel(hWnd, nSelStart, nSelEnd);
            Edit_ScrollCaret(hWnd);
        }
        break;

    case VK_HOME:
        if (!bAlt)
        {
            DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            if (bCtrl)
                nSelEnd = 0;
            else
                nSelEnd = TextLineStart(m_hText, nSelEnd);
            if (nSelEnd >= 0)
            {
                if (!bShift)
                    nSelStart = nSelEnd;
                Edit_SetSel(hWnd, nSelStart, nSelEnd);
                Edit_ScrollCaret(hWnd);
            }
        }
        break;

    case VK_END:
        if (!bAlt)
        {
            DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            if (bCtrl)
                nSelEnd = TextLength(m_hText);
            else
                nSelEnd = TextLineEnd(m_hText, nSelEnd);
            if (nSelEnd >= 0)
            {
                if (!bShift)
                    nSelStart = nSelEnd;
                Edit_SetSel(hWnd, nSelStart, nSelEnd);
                Edit_ScrollCaret(hWnd);
            }
        }
        break;

    case VK_NEXT:
        if (!bAlt)
        {
            // TODO Scroll up a page, keep caret on relative line
            UINT nPage = MyGetScrollPage(hWnd, SB_VERT);

            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            POINT pos(EditPosFromChar(hWnd, nSelEnd));
            pos.y += LineHeight() * MulDiv(nPage, 9, 10);
            nSelEnd = EditCharFromPos(hWnd, pos);
            if (!bShift)
                nSelStart = nSelEnd;
            Edit_SetSel(hWnd, nSelStart, nSelEnd);
            Edit_ScrollCaret(hWnd);
        }
        break;

    case VK_PRIOR:
        if (!bAlt)
        {
            // TODO Scroll up a page, keep caret on relative line
            UINT nPage = MyGetScrollPage(hWnd, SB_VERT);

            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            POINT pos(EditPosFromChar(hWnd, nSelEnd));
            pos.y -= LineHeight() * MulDiv(nPage, 9, 10);
            nSelEnd = EditCharFromPos(hWnd, pos);
            if (!bShift)
                nSelStart = nSelEnd;
            Edit_SetSel(hWnd, nSelStart, nSelEnd);
            Edit_ScrollCaret(hWnd);
        }
        break;

    case VK_DELETE:
        if (m_nSelEnd == m_nSelStart)
        {
            int nSelEnd = GetNextChar(m_hText, m_nSelEnd);
            if (nSelEnd >= 0)
                m_nSelEnd = nSelEnd;
        }
        if (m_nSelEnd != m_nSelStart)
        {
            WCHAR r[] = _T("");
            ReplaceSel(hWnd, r, TRUE);
        }
        break;
    }
}

void RadEdit::OnChar(HWND hWnd, TCHAR ch, int cRepeat)
{
    switch (ch)
    {
    default:
        if (iswprint((TCHAR) ch))
        {
            WCHAR r[] = { (WCHAR) ch, L'\0' };
            ReplaceSel(hWnd, r, TRUE);
        }
        break;

    case VK_RETURN:
        {
            WCHAR r[] = _T("\r\n");
            ReplaceSel(hWnd, r, TRUE);
        }
        break;

    case VK_TAB:
        {
            WCHAR r[] = _T("\t");
            ReplaceSel(hWnd, r, TRUE);
        }
        break;

    case VK_BACK:
        if (m_nSelEnd == m_nSelStart)
        {
            int nSelEnd = GetPrevChar(m_hText, m_nSelEnd);
            if (nSelEnd >= 0)
                m_nSelEnd = nSelEnd;
        }
        if (m_nSelEnd != m_nSelStart)
        {
            WCHAR r[] = _T("");
            ReplaceSel(hWnd, r, TRUE);
        }
        break;
    }
}

LRESULT RadEdit::OnGetSel(LPDWORD pSelStart, LPDWORD pSelEnd) const
{
    DWORD nStart = min(m_nSelStart, m_nSelEnd);
    DWORD nEnd = max(m_nSelStart, m_nSelEnd);

    if (pSelStart != nullptr)
        *pSelStart = nStart;
    if (pSelEnd != nullptr)
        *pSelEnd = nEnd;

    if (m_nSelStart > USHORT_MAX || m_nSelEnd > USHORT_MAX)
        return -1;
    else
        return MAKELRESULT(nStart, nEnd);
}

void RadEdit::OnSetSel(HWND hWnd, DWORD nSelStart, DWORD nSelEnd)
{
    if (nSelStart == -1)
    {
        m_nSelStart = m_nSelEnd;
    }
    else
    {
        m_nSelStart = nSelStart;
        m_nSelEnd = nSelEnd;
        if (m_nSelEnd == -1)
            m_nSelEnd = TextLength(m_hText);
    }
    // TODO Detect if changed
    MoveCaret(hWnd);
    InvalidateRect(hWnd, nullptr, TRUE);
}

LRESULT RadEdit::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        {
            LPCREATESTRUCT lpCreateStruct = (LPCREATESTRUCT) lParam;
            ASSERT(lpCreateStruct->style & WS_VSCROLL);
            ASSERT(lpCreateStruct->style & WS_HSCROLL);
            ASSERT(lpCreateStruct->style & ES_AUTOVSCROLL);
            ASSERT(lpCreateStruct->style & ES_AUTOHSCROLL);
            ASSERT(lpCreateStruct->style & ES_MULTILINE);
        }

        m_hText = GlobalAlloc(GHND, sizeof(WCHAR));
        OnSetFont(hWnd, NULL, FALSE);
        break;

    case WM_DESTROY:
        LocalFree(m_hText);
        break;

    case WM_ERASEBKGND:
        return FALSE;
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hDC = BeginPaint(hWnd, &ps);

            RECT cr = {};
            GetClientRect(hWnd, &cr);

            HDC hBmpDC = CreateCompatibleDC(hDC);
            HBITMAP hBmp = CreateCompatibleBitmap(hDC, GetWidth(cr), GetHeight(cr));
            HBITMAP hOldBmp = (HBITMAP) SelectObject(hBmpDC, hBmp);

            DefWindowProc(hWnd, WM_ERASEBKGND, (WPARAM) hBmpDC, 0);
            Draw(hWnd, hBmpDC);
            BitBlt(hDC, 0, 0, GetWidth(cr), GetHeight(cr), hBmpDC, 0, 0, SRCCOPY);

            SelectObject(hBmpDC, hOldBmp);

            DeleteObject(hBmp);
            DeleteDC(hBmpDC);

            EndPaint(hWnd, &ps);
            return 0;
        }
        break;

    case WM_SIZE:
        CalcScrollBars(hWnd);
        break;

    case WM_VSCROLL:
        OnScroll(hWnd, LOWORD(wParam), HIWORD(wParam), (HWND) lParam, SB_VERT);
        NotifyParent(hWnd, EN_VSCROLL);
        return 0;
        break;

    case WM_HSCROLL:
        OnScroll(hWnd, LOWORD(wParam), HIWORD(wParam), (HWND) lParam, SB_HORZ);
        NotifyParent(hWnd, EN_HSCROLL);
        return 0;
        break;

    case WM_SETFOCUS:
        CreateCaret(hWnd, NULL, 1, m_TextMetrics.tmHeight);
        MoveCaret(hWnd);
        ShowCaret(hWnd);
        NotifyParent(hWnd, EN_SETFOCUS);
        break;

    case WM_KILLFOCUS:
        DestroyCaret();
        NotifyParent(hWnd, EN_KILLFOCUS);
        break;

    case WM_SETFONT:
        HANDLE_WM_SETFONT(hWnd, wParam, lParam, OnSetFont);
        return 0;
        break;

    case WM_GETFONT:
        return (LRESULT) m_hFont;
        break;

    case WM_KEYDOWN:    HANDLE_WM_KEYDOWN(hWnd, wParam, lParam, OnKey); break;
    case WM_KEYUP:      HANDLE_WM_KEYUP(hWnd, wParam, lParam, OnKey);   break;
    case WM_CHAR:       HANDLE_WM_CHAR(hWnd, wParam, lParam, OnChar);   break;

    case WM_LBUTTONDOWN:
        {
            const UINT nFlags = (UINT) wParam;
            const POINT point = ToPoint(lParam);

            // TODO shift, ctrl, etc.
            DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            //OnGetSel(&nSelStart, &nSelEnd);
            nSelEnd = EditCharFromPos(hWnd, point);
            if (!(nFlags & MK_SHIFT))
                nSelStart = nSelEnd;
            OnSetSel(hWnd, nSelStart, nSelEnd);
            SetCapture(hWnd);
        }
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
        if (GetCapture() == hWnd)
        {
            const UINT nFlags = (UINT) wParam;
            const POINT point = ToPoint(lParam);

            // TODO Word select mode after a dbl click
            // TODO Scroll window
            DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            //OnGetSel(&nSelStart, &nSelEnd);
            nSelEnd = EditCharFromPos(hWnd, point);
            OnSetSel(hWnd, nSelStart, nSelEnd);
        }
        break;

    case WM_MOUSEWHEEL:
        {
            const UINT fwKeys = GET_KEYSTATE_WPARAM(wParam);
            const int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            const POINT point = ToPoint(lParam);

            if ((fwKeys & (MK_CONTROL | MK_SHIFT)) == 0)
            {
                int nLines = MulDiv(zDelta, -3, WHEEL_DELTA);
                SendMessage(hWnd, EM_LINESCROLL, 0, nLines);
            }
        }
        break;

    case EM_GETSEL:
        return OnGetSel((LPDWORD) wParam, (LPDWORD) lParam);
        break;

    case EM_SETSEL:
        OnSetSel(hWnd, (DWORD) wParam, (DWORD) lParam);
        break;

    // TODO case EM_GETRECT:
    // TODO case EM_SETRECT:
    // TODO case EM_SETRECTNP:

    case EM_SCROLL:
        {
            UINT nSBCode = (UINT) wParam;
            OnScroll(hWnd, nSBCode, 0, NULL, SB_VERT);
        }
        break;

    case EM_LINESCROLL:
        {
            int nHScroll = (int) wParam;
            int nVScroll = (int) lParam;

            const int nBar = SB_VERT;

            SCROLLINFO vinfo = { sizeof(SCROLLINFO), SIF_POS }, hinfo = { sizeof(SCROLLINFO), SIF_POS };
            ENSURE(GetScrollInfo(hWnd, SB_VERT, &vinfo));
            ENSURE(GetScrollInfo(hWnd, SB_HORZ, &hinfo));

            vinfo.nPos += nVScroll;
            hinfo.nPos += nHScroll;

            SetScrollInfo(hWnd, SB_VERT, &vinfo, TRUE);
            SetScrollInfo(hWnd, SB_HORZ, &hinfo, TRUE);

            MoveCaret(hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);

            return TRUE;    // Only multi-line supported
        }

    case EM_SCROLLCARET:
        {
            const int nLine = TextLineFromChar(m_hText, m_nSelEnd);

            SCROLLINFO vinfo = { sizeof(SCROLLINFO), SIF_POS }, hinfo = { sizeof(SCROLLINFO), SIF_POS };
            ENSURE(GetScrollInfo(hWnd, SB_VERT, &vinfo));
            ENSURE(GetScrollInfo(hWnd, SB_HORZ, &hinfo));

            // TODO Move all of the selection into view
            if (vinfo.nPos > nLine)
                vinfo.nPos = nLine;
            else if ((vinfo.nPos + (int) vinfo.nPage) < nLine)
                vinfo.nPos = nLine - vinfo.nPage + 1;

            SetScrollInfo(hWnd, SB_VERT, &vinfo, TRUE);
            //SetScrollInfo(hWnd, SB_HORZ, &hinfo, TRUE);   // TODO

            MoveCaret(hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);

            return 0;
        }
        break;

    case EM_GETMODIFY:
        return m_bModify;
        break;

    case EM_SETMODIFY:
        m_bModify = (BOOL) wParam;
        break;

    case EM_GETLINECOUNT:
        return TextLineCount(m_hText);
        break;

    case EM_LINEINDEX:
        {
            int nLine = (int) wParam;
            if (nLine == -1)
                return TextLineStart(m_hText, m_nSelEnd);
            else
                return TextLineIndex(m_hText, nLine);
        }
        break;

    case EM_SETHANDLE:
        m_hText = (HLOCAL) wParam;
        Edit_EmptyUndoBuffer(hWnd);
        Edit_SetModify(hWnd, FALSE);
        CalcScrollBars(hWnd);
        SetScrollPos(hWnd, SB_VERT, 0, TRUE);
        SetScrollPos(hWnd, SB_HORZ, 0, TRUE);
        Edit_SetSel(0, 0, TRUE);
        Edit_ScrollCaret(hWnd);
        break;

    case EM_GETHANDLE:
        return (LRESULT) m_hText;
        break;

    case EM_GETTHUMB:
        return MyGetScrollPos(hWnd, SB_VERT);
        break;

    case EM_LINELENGTH:
        {
            const DWORD nIndex = (DWORD) wParam;
            // TODO When nLine == -1

            if (nIndex > TextLength(m_hText))
                return -1;

            const DWORD nLineStart = TextLineStart(m_hText, nIndex);
            const DWORD nLineEnd = TextLineEnd(m_hText, nIndex);

            return nLineEnd - nLineStart;
        }
        break;

    case EM_REPLACESEL:
        ReplaceSel(hWnd, (PCTSTR) lParam, (BOOL) wParam);
        return 0;
        break;

    case EM_GETLINE:
        {
            // TODO Support TCHAR
            int nLine = (int) wParam;
            DWORD nBufSize = *(LPWORD) lParam;
            PTCHAR copy = (PTCHAR) lParam;

            const DWORD nLineIndex = TextLineIndex(m_hText, nLine);
            // TODO if nLineIndex  == -1
            const DWORD nLineEnd = TextLineEnd(m_hText, nLineIndex);
            const int nLineCount = min(nBufSize, nLineEnd - nLineIndex);

            PCWSTR const buffer = (PCWSTR) GlobalLock(m_hText);
            StrCpyN(copy, buffer + nLineIndex, nLineCount);
            GlobalUnlock(m_hText);
            return nLineCount;
        }
        break;

    // TODO case EM_LIMITTEXT: // Same as EM_SETLIMITTEXT
    // TODO case EM_CANUNDO:
    // TODO case EM_UNDO:
    // TODO case EM_FMTLINES:

    case EM_LINEFROMCHAR:
        {
            int nCharIndex = (int) wParam;
            if (nCharIndex == -1)
                nCharIndex = m_nSelEnd;
            return TextLineFromChar(m_hText, nCharIndex);
        }
        break;

    case EM_SETTABSTOPS:
        {
            const int nTabStops = (int) wParam;
            const LPINT rgTabStops = (LPINT) lParam;

            m_nTabStops = nTabStops;
            //memcpy(m_rgTabStops, rgTabStops, nTabStops * sizeof(INT));
            LONG nBaseUnits = LOWORD(GetDialogBaseUnits());
            int cs = AveCharWidth() * 2;
            for (int i = 0; i < nTabStops; ++i)
                m_rgTabStops[i] = MulDiv(rgTabStops[i], cs, nBaseUnits);
            return TRUE;
        }
        break;

    // TODO case EM_SETPASSWORDCHAR:  // Ignore
    // TODO case EM_EMPTYUNDOBUFFER:

    case EM_GETFIRSTVISIBLELINE:
        return MyGetScrollPos(hWnd, SB_VERT);
        break;

    // TODO case EM_SETREADONLY:

    case EM_GETWORDBREAKPROC:
        return (LRESULT) m_pEditWordBreakProc;
        break;

    case EM_SETWORDBREAKPROC:
        m_pEditWordBreakProc = (EDITWORDBREAKPROC) lParam;
        break;

    // TODO case EM_GETPASSWORDCHAR:  // Ignore

    case EM_SETMARGINS:
        if (wParam & EC_LEFTMARGIN)
            m_nMarginLeft = LOWORD(lParam);
        if (wParam & EC_RIGHTMARGIN)
            m_nMarginRight = HIWORD(lParam);
        break;

    case EM_GETMARGINS:
        return MAKELRESULT(m_nMarginLeft, m_nMarginRight);
        break;

    // TODO case EM_SETLIMITTEXT:  // Ignore
    // TODO case EM_GETLIMITTEXT:

    case EM_POSFROMCHAR:
        {
            DWORD nChar = (DWORD) wParam;

            // TODO Take into account the margins

            const int nLine = TextLineFromChar(m_hText, nChar);
            const DWORD nLineIndex = TextLineStart(m_hText, nChar);

            SIZE sPos = {};
            {
                HDC hDC = GetWindowDC(hWnd);
                HFONT of = (HFONT) SelectObject(hDC, m_hFont);
                PCWSTR const buffer = (PCWSTR) GlobalLock(m_hText);
                int nCount = nChar - nLineIndex;
                sPos = CalcTextSize(hDC, buffer + nLineIndex, nCount);
                GlobalUnlock(m_hText);
                SelectObject(hDC, of);
                ReleaseDC(hWnd, hDC);
            }

            const int nFirstLine = Edit_GetFirstVisibleLine(hWnd);  // TODO Use message ???
            POINT p = { sPos.cx - MyGetScrollPos(hWnd, SB_HORZ) * AveCharWidth() + MarginLeft(), (nLine - nFirstLine) * LineHeight() };
            return MAKELRESULT(p.x, p.y);
        }
        break;

    case EM_CHARFROMPOS:
        {
            const POINT pos = ToPoint(lParam);

            // TODO Take into account the margins

            const int nFirstLine = Edit_GetFirstVisibleLine(hWnd);  // TODO Use message ???
            const int nLine = nFirstLine + pos.y / LineHeight();
            if (nLine < 0)
                return 0;

            HDC hDC = GetWindowDC(hWnd);
            HFONT of = (HFONT) SelectObject(hDC, m_hFont);
            PCWSTR const buffer = (PCWSTR) GlobalLock(m_hText);
            const int nLineIndex = TextLineIndex(m_hText, nLine);
            if (nLineIndex < 0)
                return MAKELRESULT(TextLength(m_hText), nLine);
            const DWORD nLineEnd = TextLineEnd(m_hText, nLineIndex);
            const int nCount = nLineEnd - nLineIndex;
            const int x = pos.x + MyGetScrollPos(hWnd, SB_HORZ) * AveCharWidth() - AveCharWidth() / 2 - MarginLeft();
            const int nCol = x < 0 ? 0 : ::GetTabbedTextExtentEx(hDC, buffer + nLineIndex, nCount, m_nTabStops, m_rgTabStops, x);
            GlobalUnlock(m_hText);
            SelectObject(hDC, of);
            ReleaseDC(hWnd, hDC);

            return MAKELRESULT(nLineIndex + nCol, nLine);
        }
        break;

    // TODO case EM_SETIMESTATUS:
    // TODO case EM_GETIMESTATUS:

    case WM_GETTEXTLENGTH:
        return TextLength(m_hText);
        break;

    case WM_GETTEXT:
        {
            int nBufSize = (int) wParam;
            LPTSTR copy = (LPTSTR) lParam;

            PCWSTR const buffer = (PCWSTR) GlobalLock(m_hText);
            StrCpyN(copy, buffer, nBufSize);
            GlobalUnlock(m_hText);

            return wcslen(copy);    // TODO capture size when copying
        }
        break;

    case WM_SETTEXT:
        {
            LPCTSTR copy = (LPCTSTR) lParam;

            HLOCAL hTextNew = TextCreate(copy);
            if (hTextNew != NULL)
            {
                LocalFree(m_hText);
                m_hText = hTextNew;
                return TRUE;
            }
            else
                return FALSE;
        }
        break;

    // TODO case WM_CUT:
    // TODO case WM_COPY:
    // TODO case WM_PASTE:
    // TODO case WM_CLEAR:
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK RadEditWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RadEdit* pData = (RadEdit*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (message == WM_CREATE)
    {
        pData = new RadEdit();
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) pData);
    }

    LRESULT ret = pData
        ? pData->WndProc(hWnd, message, wParam, lParam)
        : DefWindowProc(hWnd, message, wParam, lParam);

    if (message == WM_DESTROY)
    {
        delete pData;
        pData = nullptr;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) pData);
    }

    return ret;
}
