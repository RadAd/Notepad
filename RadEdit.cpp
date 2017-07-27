#include "stdafx.h"
#include "RadEdit.h"
#include "Text.h"
#include <windowsx.h>
#include <intsafe.h>

// TODO
// WM_WANTKEYS https://blogs.msdn.microsoft.com/oldnewthing/20031126-00/?p=41703
// Check parent notifications EN_*
// Support IME
// Context menu
// Support word wrap
// readonly
// ES_NOHIDESEL

// NOT Implemented:
// limit text
// single line
// cue banner
// password char
// ballon tool tip

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
    //SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), code), (LPARAM) hWnd);
    FORWARD_WM_COMMAND(GetParent(hWnd), GetWindowID(hWnd), hWnd, code, SNDMSG);
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

class RadEdit
{
private:
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

public:
    BOOL OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
    void OnDestroy(HWND hWnd);
    BOOL OnEraseBkgnd(HWND hWnd, HDC hdc);
    void OnPaint(HWND hWnd);
    void OnSize(HWND hWnd, UINT state, int cx, int cy);
    void OnScroll(HWND hWnd, UINT nSBCode, UINT /*nPos*/, HWND hScrollBar, UINT nBar) const;
    void OnVScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos) { OnScroll(hWnd, code, pos, hWndCtl, SB_VERT); NotifyParent(hWnd, EN_VSCROLL); }
    void OnHScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos) { OnScroll(hWnd, code, pos, hWndCtl, SB_HORZ); NotifyParent(hWnd, EN_HSCROLL); }
    void OnSetFocus(HWND hWnd, HWND hWndOldFocus);
    void OnKillFocus(HWND hWnd, HWND hWndNewFocus);
    void OnSetFont(HWND hWnd, HFONT hFont, BOOL bRedraw);
    HFONT OnGetFont(HWND hWnd) const;
    void OnKey(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
    void OnChar(HWND hWnd, TCHAR ch, int cRepeat);
    void OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
    void OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags);
    void OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags);
    void OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys);
    INT OnGetTextLength(HWND hWnd);
    INT OnGetText(HWND hWnd, int cchTextMax, LPTSTR lpszText);
    void OnSetText(HWND hWnd, LPCTSTR lpszText);
    void OnCut(HWND hWnd);
    void OnCopy(HWND hWnd);
    void OnPaste(HWND hWnd);
    void OnClear(HWND hWnd);
    void OnSetRedraw(HWND hwnd, BOOL fRedraw);

    LRESULT OnGetSel(HWND hWnd, LPDWORD pSelStart, LPDWORD pSelEnd) const;
    void OnSetSel(HWND hWnd, DWORD nSelStart, DWORD nSelEnd);

    LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

void RadEdit::CalcScrollBars(HWND hWnd) const
{
    SIZE s = {};
    PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
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
    LocalUnlock(m_hText);

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
    HFONT of = SelectFont(hDC, m_hFont);

    /*HBRUSH hBgBrush = */ FORWARD_WM_CTLCOLOREDIT(GetParent(hWnd), hDC, hWnd, SNDMSG);
    //DeleteBrush(hBgBrush);

    // TODO Use margins here and in calcscrollbars

    RECT cr = {};
    GetClientRect(hWnd, &cr);

    DWORD nSelStart, nSelEnd;
    OnGetSel(hWnd, &nSelStart, &nSelEnd);

    POINT p = { -MyGetScrollPos(hWnd, SB_HORZ) * AveCharWidth() + MarginLeft(), 0 };
    PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
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
    LocalUnlock(m_hText);

    SelectFont(hDC, of);
}

void RadEdit::ReplaceSel(HWND hWnd, PCWSTR pText, BOOL /*bStoreUndo*/)
{
    // TODO bStoreUndo

    DWORD nSelStart, nSelEnd;
    OnGetSel(hWnd, &nSelStart, &nSelEnd);
    if (nSelStart != nSelEnd || !IsEmpty(pText))
    {
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

BOOL RadEdit::OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    ASSERT(lpCreateStruct->style & WS_VSCROLL);
    ASSERT(lpCreateStruct->style & WS_HSCROLL);
    ASSERT(lpCreateStruct->style & ES_AUTOVSCROLL);
    ASSERT(lpCreateStruct->style & ES_AUTOHSCROLL);
    ASSERT(lpCreateStruct->style & ES_MULTILINE);

    m_hText = LocalAlloc(LHND, sizeof(WCHAR));
    OnSetFont(hWnd, NULL, FALSE);
    return TRUE;
}

void RadEdit::OnDestroy(HWND hWGnd)
{
   LocalFree(m_hText);
}

BOOL RadEdit::OnEraseBkgnd(HWND hWnd, HDC hdc)
{
   return FALSE;
}

void RadEdit::OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps = {};
    HDC hDC = BeginPaint(hWnd, &ps);

    RECT cr = {};
    GetClientRect(hWnd, &cr);

    HDC hBmpDC = CreateCompatibleDC(hDC);
    HBITMAP hBmp = CreateCompatibleBitmap(hDC, GetWidth(cr), GetHeight(cr));
    HBITMAP hOldBmp = SelectBitmap(hBmpDC, hBmp);

    DefWindowProc(hWnd, WM_ERASEBKGND, (WPARAM) hBmpDC, 0);
    Draw(hWnd, hBmpDC);
    BitBlt(hDC, 0, 0, GetWidth(cr), GetHeight(cr), hBmpDC, 0, 0, SRCCOPY);

    SelectBitmap(hBmpDC, hOldBmp);

    DeleteBitmap(hBmp);
    DeleteDC(hBmpDC);

    EndPaint(hWnd, &ps);
}

void RadEdit::OnSize(HWND hWnd, UINT state, int cx, int cy)
{
   CalcScrollBars(hWnd);
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

void RadEdit::OnSetFocus(HWND hWnd, HWND hWndOldFocus)
{
    CreateCaret(hWnd, NULL, 1, m_TextMetrics.tmHeight);
    MoveCaret(hWnd);
    ShowCaret(hWnd);
    NotifyParent(hWnd, EN_SETFOCUS);
}

void RadEdit::OnKillFocus(HWND hWnd, HWND hWndNewFocus)
{
    DestroyCaret();
    NotifyParent(hWnd, EN_KILLFOCUS);
}

void RadEdit::OnSetFont(HWND hWnd, HFONT hFont, BOOL bRedraw)
{
    if (hFont == NULL)
        hFont = GetStockFont(SYSTEM_FONT);

    const int ocs = AveCharWidth();

    m_hFont = hFont;

    {
        HDC hDC = GetWindowDC(hWnd);
        HFONT of = SelectFont(hDC, m_hFont);
        GetTextMetrics(hDC, &m_TextMetrics);
        SelectFont(hDC, of);
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

HFONT RadEdit::OnGetFont(HWND hWnd) const
{
   return m_hFont;
}

void RadEdit::OnKey(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
    if (!fDown)
        return;

    const bool bShift = (GetKeyState(VK_SHIFT) & KF_UP) != 0;
    const bool bCtrl = (GetKeyState(VK_CONTROL) & KF_UP) != 0;
    const bool bAlt = (GetKeyState(VK_MENU) & KF_UP) != 0;

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
                EDITWORDBREAKPROC ewb = m_pEditWordBreakProc != nullptr ? m_pEditWordBreakProc : DefaultEditWordBreakProc;
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
                EDITWORDBREAKPROC ewb = m_pEditWordBreakProc != nullptr ? m_pEditWordBreakProc : DefaultEditWordBreakProc;
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
        if (bShift)
           OnCut(hWnd);
        else
        {
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
        }
        break;

    case VK_INSERT:
       if (bCtrl)
           OnCopy(hWnd);
       if (bShift)
           OnPaste(hWnd);
       break;

    case _T('X'):
       if (bCtrl)
           OnCut(hWnd);
       break;

    case _T('C'):
       if (bCtrl)
          OnCopy(hWnd);
       break;

    case _T('V'):
       if (bCtrl)
          OnPaste(hWnd);
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

void RadEdit::OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
   POINT point = { x, y };

   if (fDoubleClick)
   {
        DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
        //OnGetSel(hWnd, &nSelStart, &nSelEnd);
        nSelEnd = EditCharFromPos(hWnd, point);
        EDITWORDBREAKPROC ewb = &DefaultEditWordBreakProc;
        nSelStart = MoveWord(ewb, WB_LEFT, m_hText, nSelEnd);
        nSelEnd = MoveWord(ewb, WB_RIGHT, m_hText, nSelEnd);
        OnSetSel(hWnd, nSelStart, nSelEnd);
   }
   else
   {
        DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
        //OnGetSel(hWnd, &nSelStart, &nSelEnd);
        nSelEnd = EditCharFromPos(hWnd, point);
        if (!(keyFlags & MK_SHIFT))
            nSelStart = nSelEnd;
        OnSetSel(hWnd, nSelStart, nSelEnd);
        SetCapture(hWnd);
   }
}

void RadEdit::OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
    POINT point = { x, y };
    ReleaseCapture();
}

void RadEdit::OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags)
{
    POINT point = { x, y };
    if (GetCapture() == hWnd)
    {
        // TODO Word select mode after a dbl click
        // TODO Scroll window
        DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
        //OnGetSel(hWnd, &nSelStart, &nSelEnd);
        nSelEnd = EditCharFromPos(hWnd, point);
        OnSetSel(hWnd, nSelStart, nSelEnd);
    }
}

void RadEdit::OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    if ((fwKeys & (MK_CONTROL | MK_SHIFT)) == 0)
    {
        int nLines = MulDiv(zDelta, -3, WHEEL_DELTA);
        Edit_Scroll(hWnd, nLines, 0);
    }
}

INT RadEdit::OnGetTextLength(HWND hWnd)
{
   return TextLength(m_hText);
}

INT RadEdit::OnGetText(HWND hWnd, int cchTextMax, LPTSTR lpszText)
{
    PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
    StrCpyN(lpszText, buffer, cchTextMax);
    LocalUnlock(m_hText);

    return (INT) wcslen(lpszText);    // TODO capture size when copying
}

void RadEdit::OnSetText(HWND hWnd, LPCTSTR lpszText)
{
    HLOCAL hTextNew = TextCreate(lpszText);
    if (hTextNew != NULL)
    {
        LocalFree(m_hText);
        m_hText = hTextNew;
        return; // TODO TRUE;
    }
    else
        return; // TODO FALSE;
}

void RadEdit::OnCut(HWND hWnd)
{
   OnCopy(hWnd);
   ReplaceSel(hWnd, _T(""), TRUE);
}

void RadEdit::OnCopy(HWND hWnd)
{
    DWORD nSelStart, nSelEnd;
    OnGetSel(hWnd, &nSelStart, &nSelEnd);

    if (nSelStart < nSelEnd)
    {
        DWORD nLength = nSelEnd - nSelStart;
        HGLOBAL hClip = GlobalAlloc(GMEM_MOVEABLE, (nLength + 1) * sizeof(WCHAR));
        PWSTR const clip = (PWSTR) GlobalLock(hClip);
        PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
        memcpy(clip, buffer + nSelStart, nLength * sizeof(WCHAR));
        clip[nLength] = L'\0';
        LocalUnlock(m_hText);
        GlobalUnlock(hClip);

        if (OpenClipboard(hWnd))
        {
            ENSURE(SetClipboardData(CF_UNICODETEXT, hClip));
            CloseClipboard();
        }
    }
}

void RadEdit::OnPaste(HWND hWnd)
{
   if (OpenClipboard(hWnd))
   {
       HGLOBAL hClip = GetClipboardData(CF_UNICODETEXT);
       if (hClip != NULL)
       {
           PCWSTR const buffer = (PCWSTR) GlobalLock(hClip);
           ReplaceSel(hWnd, buffer, TRUE);
           GlobalUnlock(hClip);
       }
       CloseClipboard();
   }
}

void RadEdit::OnClear(HWND hWnd)
{
   ReplaceSel(hWnd, L"", TRUE);
}

void RadEdit::OnSetRedraw(HWND hWnd, BOOL fRedraw)
{
    // TODO Capture rcPaint during WM_PAINT to reapply here
    if (fRedraw)
        RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    FORWARD_WM_SETREDRAW(hWnd, fRedraw, DefWindowProc);
}

LRESULT RadEdit::OnGetSel(HWND hWnd, LPDWORD pSelStart, LPDWORD pSelEnd) const
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

#define HANDLE_EM_GETSEL(hWnd, wParam, lParam, fn) \
    (LRESULT)(fn)(hWnd, (LPDWORD) wParam, (LPDWORD) lParam)
#define HANDLE_EM_SETSEL(hWnd, wParam, lParam, fn) \
    ((fn)(hWnd, (DWORD) wParam, (DWORD) lParam), 0L)

LRESULT RadEdit::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    HANDLE_MSG(hWnd, WM_CREATE,        OnCreate);
    HANDLE_MSG(hWnd, WM_DESTROY,       OnDestroy);
    HANDLE_MSG(hWnd, WM_ERASEBKGND,    OnEraseBkgnd);
    HANDLE_MSG(hWnd, WM_PAINT,         OnPaint);
    HANDLE_MSG(hWnd, WM_SIZE,          OnSize);
    HANDLE_MSG(hWnd, WM_VSCROLL,       OnVScroll);
    HANDLE_MSG(hWnd, WM_HSCROLL,       OnHScroll);
    HANDLE_MSG(hWnd, WM_SETFOCUS,      OnSetFocus);
    HANDLE_MSG(hWnd, WM_KILLFOCUS,     OnKillFocus);
    HANDLE_MSG(hWnd, WM_SETFONT,       OnSetFont);
    HANDLE_MSG(hWnd, WM_GETFONT,       OnGetFont);
    HANDLE_MSG(hWnd, WM_KEYDOWN,       OnKey);
    HANDLE_MSG(hWnd, WM_KEYUP,         OnKey);
    HANDLE_MSG(hWnd, WM_CHAR,          OnChar);
    HANDLE_MSG(hWnd, WM_LBUTTONDOWN,   OnLButtonDown);
    HANDLE_MSG(hWnd, WM_LBUTTONDBLCLK, OnLButtonDown);
    HANDLE_MSG(hWnd, WM_LBUTTONUP,     OnLButtonUp);
    HANDLE_MSG(hWnd, WM_MOUSEMOVE,     OnMouseMove);
    HANDLE_MSG(hWnd, WM_MOUSEWHEEL,    OnMouseWheel);
    HANDLE_MSG(hWnd, WM_GETTEXTLENGTH, OnGetTextLength);
    HANDLE_MSG(hWnd, WM_GETTEXT,       OnGetText);
    HANDLE_MSG(hWnd, WM_SETTEXT,       OnSetText);
    HANDLE_MSG(hWnd, WM_CUT,           OnCut);
    HANDLE_MSG(hWnd, WM_COPY,          OnCopy);
    HANDLE_MSG(hWnd, WM_PASTE,         OnPaste);
    HANDLE_MSG(hWnd, WM_CLEAR,         OnClear);
    HANDLE_MSG(hWnd, WM_SETREDRAW,     OnSetRedraw);

    HANDLE_MSG(hWnd, EM_GETSEL,        OnGetSel);
    HANDLE_MSG(hWnd, EM_SETSEL,        OnSetSel);
    // TODO HANDLE_MSG(hWnd, EM_GETRECT, OnGetRect);
    // TODO HANDLE_MSG(hWnd, EM_SETRECT, OnSetRect);
    // TODO HANDLE_MSG(hWnd, EM_SETRECTNP, OnSetRectNP);

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

            SCROLLINFO vinfo = { sizeof(SCROLLINFO), SIF_POS | SIF_PAGE }, hinfo = { sizeof(SCROLLINFO), SIF_POS | SIF_PAGE };
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

            PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
            StrCpyN(copy, buffer + nLineIndex, nLineCount);
            LocalUnlock(m_hText);
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
                HFONT of = SelectFont(hDC, m_hFont);
                PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
                int nCount = nChar - nLineIndex;
                sPos = CalcTextSize(hDC, buffer + nLineIndex, nCount);
                LocalUnlock(m_hText);
                SelectFont(hDC, of);
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
            HFONT of = SelectFont(hDC, m_hFont);
            PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
            const int nLineIndex = TextLineIndex(m_hText, nLine);
            if (nLineIndex < 0)
                return MAKELRESULT(TextLength(m_hText), nLine);
            const DWORD nLineEnd = TextLineEnd(m_hText, nLineIndex);
            const int nCount = nLineEnd - nLineIndex;
            const int x = pos.x + MyGetScrollPos(hWnd, SB_HORZ) * AveCharWidth() - AveCharWidth() / 2 - MarginLeft();
            const int nCol = x < 0 ? 0 : ::GetTabbedTextExtentEx(hDC, buffer + nLineIndex, nCount, m_nTabStops, m_rgTabStops, x);
            LocalUnlock(m_hText);
            SelectFont(hDC, of);
            ReleaseDC(hWnd, hDC);

            return MAKELRESULT(nLineIndex + nCol, nLine);
        }
        break;

    // TODO case EM_SETIMESTATUS:
    // TODO case EM_GETIMESTATUS:
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
