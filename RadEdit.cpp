#include "stdafx.h"
#include "RadEdit.h"
#include "Text.h"
#include <windowsx.h>
#include <intsafe.h>

// TODO
// Undo
// Check parent notifications EN_*
// Support IME
// Context menu
// Support word wrap
// Support middle mouse click scroll mode
// ES_LEFT, ES_CENTER, ES_RIGHT
// ES_UPPERCASE, ES_LOWERCASE
// ES_AUTOVSCROLL, ES_AUTOHSCROLL not set
// ES_OEMCONVERT CharToOem
// ES_WANTRETURN
// ES_NUMBER

// NOT Implemented:
// limit text
// single line
// word wrap
// cue banner
// ES_PASSWORD
// balloon tool tip

namespace
{
    inline LONG GetWidth(const RECT& r)
    {
        return r.right - r.left;
    }

    inline LONG GetHeight(const RECT& r)
    {
        return r.bottom - r.top;
    }

    inline SIZE ToSize(DWORD s)
    {
        return { LOWORD(s), HIWORD(s) };
    }

    inline POINT ToPoint(LPARAM p)
    {
        return { GET_X_LPARAM(p), GET_Y_LPARAM(p) };
    }

    inline LRESULT ToLResult(POINT p)
    {
        return MAKELPARAM(p.x, p.y);
    }

    BOOL HasStyle(HWND hWnd, int HasStyle)
    {
        int Style = GetWindowStyle(hWnd);
        return (Style & HasStyle) == HasStyle;
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
        // TODO Use a binary search method, need to be careful if in the middle of a character
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
    BOOL m_bSelectWord = FALSE;
    EDITWORDBREAKPROC m_pEditWordBreakProc = nullptr;

    int LineHeight() const { return m_TextMetrics.tmHeight; }
    int AveCharWidth() const { return m_TextMetrics.tmAveCharWidth; }
    int MarginLeft() const { return m_nMarginLeft == EC_USEFONTINFO ? m_TextMetrics.tmOverhang : m_nMarginLeft; }
    int MarginRight() const { return m_nMarginRight == EC_USEFONTINFO ? m_TextMetrics.tmOverhang : m_nMarginRight; }

    void CalcScrollBars(HWND hWnd) const;
    SIZE CalcTextSize(HDC hDC, PCWSTR buffer, int nCount) const;
    void MoveCaret(HWND hWnd) const;
    void Draw(HWND hWnd, HDC hDC, LPCRECT pr) const;
    void ReplaceSel(HWND hWnd, PCWSTR pText, BOOL bStoreUndo);
    DWORD GetFirstVisibleLine(HWND hWnd) const;
    DWORD GetFirstVisibleCol(HWND hWnd) const;

public:
    BOOL OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
    void OnDestroy(HWND hWnd);
    BOOL OnEraseBkgnd(HWND hWnd, HDC hdc);
    void OnPaint(HWND hWnd);
    void OnSize(HWND hWnd, UINT state, int cx, int cy);
    void OnScroll(HWND hWnd, UINT nSBCode, HWND hScrollBar, UINT nBar) const;
    void OnVScroll(HWND hWnd, HWND hWndCtl, UINT code, int /*pos*/) { OnScroll(hWnd, code, hWndCtl, SB_VERT); NotifyParent(hWnd, EN_VSCROLL); }
    void OnHScroll(HWND hWnd, HWND hWndCtl, UINT code, int /*pos*/) { OnScroll(hWnd, code, hWndCtl, SB_HORZ); NotifyParent(hWnd, EN_HSCROLL); }
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
    BOOL OnSetText(HWND hWnd, LPCTSTR lpszText);
    void OnCut(HWND hWnd);
    void OnCopy(HWND hWnd);
    void OnPaste(HWND hWnd);
    void OnClear(HWND hWnd);
    void OnSetRedraw(HWND hwnd, BOOL fRedraw);
    UINT OnGetDlgCode(HWND hWnd, LPMSG pMsg);

    LRESULT OnGetSel(HWND hWnd, LPDWORD pSelStart, LPDWORD pSelEnd) const;
    void OnSetSel(HWND hWnd, DWORD nSelStart, DWORD nSelEnd);
    LRESULT OnScroll(HWND hWnd, UINT nSBCode);
    LRESULT OnLineScroll(HWND hWnd, int nHScroll, int nVScroll);
    void OnScrollCaret(HWND hWnd);
    BOOL OnGetModify(HWND hWnd);
    void OnSetModify(HWND hWnd, BOOL bModify);
    DWORD OnGetLineCount(HWND hWnd);
    DWORD OnLineIndex(HWND hWnd, int nLine);
    void OnSetHandle(HWND hWnd, HLOCAL hText);
    HLOCAL OnGetHandle(HWND hWnd);
    DWORD OnGetThumb(HWND hWnd);
    DWORD OnLineLength(HWND hWnd, int nIndex);
    void OnReplaceSel(HWND hWnd, PCWSTR pText, BOOL bStoreUndo);
    int OnGetLine(HWND hWnd, int nLine, PTCHAR copy, DWORD nBufSize);
    DWORD OnLineFromChar(HWND hWnd, int nCharIndex);
    BOOL OnSetTabStops(HWND hWnd, LPINT rgTabStops, int nTabStops);
    DWORD OnGetFirstVisibleLine(HWND hWnd);
    BOOL OnSetReadOnly(HWND hWnd, BOOL bReadOnly);
    EDITWORDBREAKPROC OnGetWordBreakProc(HWND hWnd);
    void OnSetWordBreakProc(HWND hWnd, EDITWORDBREAKPROC pEWP);
    void OnSetMargins(HWND hWnd, UINT nFlags, UINT nLeftMargin, UINT nRightMargin);
    LRESULT OnGetMargins(HWND hWnd);
    POINT OnPosFromChar(HWND hWnd, DWORD nChar);
    LRESULT OnCharFromPos(HWND hWnd, POINT pos);

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
    cr.left += MarginLeft();
    cr.right -= MarginRight();

    SCROLLINFO vinfo = { sizeof(SCROLLINFO), SIF_ALL }, hinfo = { sizeof(SCROLLINFO), SIF_ALL };
    ENSURE(GetScrollInfo(hWnd, SB_VERT, &vinfo));
    ENSURE(GetScrollInfo(hWnd, SB_HORZ, &hinfo));

    vinfo.fMask |= SIF_DISABLENOSCROLL;
    hinfo.fMask |= SIF_DISABLENOSCROLL;
    vinfo.nMax = s.cy - 1;
    vinfo.nPage = GetHeight(cr) / LineHeight() - 1;
    hinfo.nMax = s.cx;
    hinfo.nPage = GetWidth(cr) / AveCharWidth() - 1;

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

void RadEdit::Draw(HWND hWnd, HDC hDC, LPCRECT pr) const
{
    HFONT of = SelectFont(hDC, m_hFont);

    BOOL bShowSel = GetFocus() == hWnd || HasStyle(hWnd, ES_NOHIDESEL);

    RECT rClip;
    GetClientRect(hWnd, &rClip);
    rClip.left += MarginLeft();
    rClip.right -= MarginRight();
    IntersectClipRect(hDC, rClip.left, rClip.top, rClip.right, rClip.bottom);

    DWORD nSelStart, nSelEnd;
    OnGetSel(hWnd, &nSelStart, &nSelEnd);

    POINT p = { -(int) GetFirstVisibleCol(hWnd) * AveCharWidth() + MarginLeft(), 0 };
    PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
    const int nFirstLine = GetFirstVisibleLine(hWnd);
    const int nStartLine = pr->top / LineHeight();
    p.y += nStartLine * LineHeight();
    int nIndex = TextLineIndex(m_hText, nFirstLine + nStartLine);
    while (nIndex >= 0)
    {
        const int nLineEnd = GetLineEnd(buffer, nIndex);
        const int nCount = nLineEnd - nIndex;
        TabbedTextOut(hDC, p.x, p.y, buffer + nIndex, nCount, m_nTabStops, m_rgTabStops, MarginLeft());

        if (bShowSel)
        {
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
        }

        nIndex = GetNextLine(buffer, nLineEnd);
        p.y += LineHeight();
        if ((p.y + m_TextMetrics.tmHeight) > pr->bottom)
            break;
    }
    LocalUnlock(m_hText);

    SelectFont(hDC, of);
}

void RadEdit::ReplaceSel(HWND hWnd, PCWSTR pText, BOOL /*bStoreUndo*/)
{
    // TODO bStoreUndo
    if (!HasStyle(hWnd, ES_READONLY))
    {
        DWORD nSelStart, nSelEnd;
        OnGetSel(hWnd, &nSelStart, &nSelEnd);
        if (nSelStart != nSelEnd || !IsEmpty(pText))
        {
            if (HLOCAL hTextNew = TextReplace(m_hText, nSelStart, nSelEnd, pText))
            {
                m_hText = hTextNew;
                m_bModify = TRUE;
                OnSetSel(hWnd, nSelEnd, nSelEnd);
                InvalidateRect(hWnd, nullptr, TRUE);
                NotifyParent(hWnd, EN_CHANGE);
            }
            else
                NotifyParent(hWnd, EN_ERRSPACE);
        }
    }
}

DWORD RadEdit::GetFirstVisibleLine(HWND hWnd) const
{
    return MyGetScrollPos(hWnd, SB_VERT);
}

DWORD RadEdit::GetFirstVisibleCol(HWND hWnd) const
{
    return MyGetScrollPos(hWnd, SB_HORZ);
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
            //curpos = max(info.nMin, curpos - MulDiv(info.nPage, 9, 10));
            curpos = max(info.nMin, curpos - info.nPage);
        break;

    case SB_PAGERIGHT:      // Scroll one page right.
        if (curpos < info.nMax)
            //curpos = min(info.nMax, curpos + MulDiv(info.nPage, 9, 10));
            curpos = min(info.nMax, curpos + info.nPage);
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

    HBRUSH hBgBrush = HasStyle(hWnd, ES_READONLY)
        ? FORWARD_WM_CTLCOLORSTATIC(GetParent(hWnd), hDC, hWnd, SNDMSG)
        : FORWARD_WM_CTLCOLOREDIT(GetParent(hWnd), hDC, hWnd, SNDMSG);
    DeleteBrush(hBgBrush);  // TODO Where to use brush

    DefWindowProc(hWnd, WM_ERASEBKGND, (WPARAM) hBmpDC, 0);
    Draw(hWnd, hBmpDC, &ps.rcPaint);
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

void RadEdit::OnScroll(HWND hWnd, UINT nSBCode, HWND hScrollBar, UINT nBar) const
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
    InvalidateRect(hWnd, NULL, TRUE);
    NotifyParent(hWnd, EN_SETFOCUS);
}

void RadEdit::OnKillFocus(HWND hWnd, HWND hWndNewFocus)
{
    DestroyCaret();
    InvalidateRect(hWnd, NULL, TRUE);
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
        if (!bAlt)
        {
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
                OnSetSel(hWnd, nSelStart, nSelEnd);
                OnScrollCaret(hWnd);
            }
        }
        break;

    case VK_RIGHT:
        if (!bAlt)
        {
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
                OnSetSel(hWnd, nSelStart, nSelEnd);
                OnScrollCaret(hWnd);
            }
        }
        break;

    case VK_UP:
        if (!bAlt && !bCtrl)
        {
            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            POINT pos(EditPosFromChar(hWnd, nSelEnd));
            pos.y -= LineHeight();
            nSelEnd = EditCharFromPos(hWnd, pos);
            if (!bShift)
                nSelStart = nSelEnd;
            OnSetSel(hWnd, nSelStart, nSelEnd);
            OnScrollCaret(hWnd);
        }
        break;

    case VK_DOWN:
        if (!bAlt && !bCtrl)
        {
            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            POINT pos(EditPosFromChar(hWnd, nSelEnd));
            pos.y += LineHeight();
            nSelEnd = EditCharFromPos(hWnd, pos);
            if (!bShift)
                nSelStart = nSelEnd;
            OnSetSel(hWnd, nSelStart, nSelEnd);
            OnScrollCaret(hWnd);
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
                OnSetSel(hWnd, nSelStart, nSelEnd);
                OnScrollCaret(hWnd);
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
                OnSetSel(hWnd, nSelStart, nSelEnd);
                OnScrollCaret(hWnd);
            }
        }
        break;

    case VK_NEXT:
        if (!bAlt)
        {
            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            POINT pos(EditPosFromChar(hWnd, nSelEnd));

            OnScroll(hWnd, SB_PAGEDOWN, NULL, SB_VERT);

            nSelEnd = EditCharFromPos(hWnd, pos);
            if (!bShift)
                nSelStart = nSelEnd;
            OnSetSel(hWnd, nSelStart, nSelEnd);
            OnScrollCaret(hWnd);
        }
        break;

    case VK_PRIOR:
        if (!bAlt)
        {
            int nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
            POINT pos(EditPosFromChar(hWnd, nSelEnd));

            OnScroll(hWnd, SB_PAGEUP, NULL, SB_VERT);

            nSelEnd = EditCharFromPos(hWnd, pos);
            if (!bShift)
                nSelStart = nSelEnd;
            OnSetSel(hWnd, nSelStart, nSelEnd);
            OnScrollCaret(hWnd);
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

   DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
   //OnGetSel(hWnd, &nSelStart, &nSelEnd);
   nSelEnd = EditCharFromPos(hWnd, point);
   m_bSelectWord = fDoubleClick;
   if (fDoubleClick)
   {
        EDITWORDBREAKPROC ewb = &DefaultEditWordBreakProc;
        if (!(keyFlags & MK_SHIFT))
            nSelStart = MoveWord(ewb, WB_LEFT, m_hText, nSelEnd);
        nSelEnd = MoveWord(ewb, WB_RIGHT, m_hText, nSelEnd);
        m_bSelectWord = TRUE;
   }
   if (!(keyFlags & MK_SHIFT))
       nSelStart = nSelEnd;
   OnSetSel(hWnd, nSelStart, nSelEnd);
   SetCapture(hWnd);
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
        DWORD nSelStart = m_nSelStart, nSelEnd = m_nSelEnd;
        //OnGetSel(hWnd, &nSelStart, &nSelEnd);
        nSelEnd = EditCharFromPos(hWnd, point);
        if (m_bSelectWord)
        {
            EDITWORDBREAKPROC ewb = &DefaultEditWordBreakProc;
            nSelEnd = MoveWord(ewb, WB_RIGHT, m_hText, nSelEnd);
        }
        OnSetSel(hWnd, nSelStart, nSelEnd);
        OnScrollCaret(hWnd);
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

BOOL RadEdit::OnSetText(HWND hWnd, LPCTSTR lpszText)
{
    HLOCAL hTextNew = TextCreate(lpszText);
    if (hTextNew != NULL)
    {
        LocalFree(m_hText);
        m_hText = hTextNew;
        return TRUE;
    }
    else
        return FALSE;
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
    FORWARD_WM_SETREDRAW(hWnd, fRedraw, DefWindowProc);
    if (fRedraw)
        RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
        //InvalidateRect(hWnd, NULL, TRUE);
}

UINT RadEdit::OnGetDlgCode(HWND hWnd, LPMSG pMsg)
{
    return DLGC_HASSETSEL | DLGC_WANTALLKEYS | DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;
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
        nSelStart = nSelEnd = m_nSelEnd;
    }
    else if (nSelEnd == -1)
        nSelEnd = TextLength(m_hText);
    if (m_nSelStart != nSelStart || m_nSelEnd != nSelEnd)
    {
        m_nSelStart = nSelStart;
        m_nSelEnd = nSelEnd;
        MoveCaret(hWnd);
        InvalidateRect(hWnd, nullptr, TRUE);
    }
}

LRESULT RadEdit::OnScroll(HWND hWnd, UINT nSBCode)
{
    OnScroll(hWnd, nSBCode, NULL, SB_VERT);
    // TODO Fix return
    return 0;
}

LRESULT RadEdit::OnLineScroll(HWND hWnd, int nHScroll, int nVScroll)
{
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

void RadEdit::OnScrollCaret(HWND hWnd)
{
    const int nLine = TextLineFromChar(m_hText, m_nSelEnd);
    // TODO Maybe calculate this better
    POINT pos = OnPosFromChar(hWnd, m_nSelEnd);
    pos.x -= MarginLeft();
    pos.x /= AveCharWidth();
    pos.x += GetFirstVisibleCol(hWnd);

    SCROLLINFO vinfo = { sizeof(SCROLLINFO), SIF_POS | SIF_PAGE }, hinfo = { sizeof(SCROLLINFO), SIF_POS | SIF_PAGE };
    ENSURE(GetScrollInfo(hWnd, SB_VERT, &vinfo));
    ENSURE(GetScrollInfo(hWnd, SB_HORZ, &hinfo));

    // TODO Move all of the selection into view
    if (vinfo.nPos > nLine)
        vinfo.nPos = nLine;
    else if ((vinfo.nPos + (int) vinfo.nPage) < nLine)
        vinfo.nPos = nLine - vinfo.nPage;

    if (hinfo.nPos > pos.x)
        hinfo.nPos = pos.x;
    else if ((hinfo.nPos + (int) hinfo.nPage) < pos.x)
        hinfo.nPos = pos.x - hinfo.nPage;

    SetScrollInfo(hWnd, SB_VERT, &vinfo, TRUE);
    SetScrollInfo(hWnd, SB_HORZ, &hinfo, TRUE);

    MoveCaret(hWnd);
    InvalidateRect(hWnd, nullptr, TRUE);
}

BOOL RadEdit::OnGetModify(HWND hWnd)
{
    return m_bModify;
}

void RadEdit::OnSetModify(HWND hWnd, BOOL bModify)
{
    m_bModify = bModify;
}

DWORD RadEdit::OnGetLineCount(HWND hWnd)
{
    return TextLineCount(m_hText);
}

DWORD RadEdit::OnLineIndex(HWND hWnd, int nLine)
{
    if (nLine == -1)
        return TextLineStart(m_hText, m_nSelEnd);
    else
        return TextLineIndex(m_hText, nLine);
}

void RadEdit::OnSetHandle(HWND hWnd, HLOCAL hText)
{
    m_hText = hText;
    Edit_EmptyUndoBuffer(hWnd);
    Edit_SetModify(hWnd, FALSE);
    CalcScrollBars(hWnd);
    SetScrollPos(hWnd, SB_VERT, 0, TRUE);
    SetScrollPos(hWnd, SB_HORZ, 0, TRUE);
    OnSetSel(hWnd, 0, 0);
    //OnScrollCaret(hWnd);
}

HLOCAL RadEdit::OnGetHandle(HWND hWnd)
{
    return m_hText;
}

DWORD RadEdit::OnGetThumb(HWND hWnd)
{
    return MyGetScrollPos(hWnd, SB_VERT);
}

DWORD RadEdit::OnLineLength(HWND hWnd, int nIndex)
{
    // TODO When nIndex == -1

    if (nIndex > TextLength(m_hText))
        return -1;

    const DWORD nLineStart = TextLineStart(m_hText, nIndex);
    const DWORD nLineEnd = TextLineEnd(m_hText, nIndex);

    return nLineEnd - nLineStart;
}

void RadEdit::OnReplaceSel(HWND hWnd, PCWSTR pText, BOOL bStoreUndo)
{
    ReplaceSel(hWnd, pText, bStoreUndo);
}

int RadEdit::OnGetLine(HWND hWnd, int nLine, PTCHAR copy, DWORD nBufSize)
{
    // TODO Support TCHAR

    const DWORD nLineIndex = TextLineIndex(m_hText, nLine);
    // TODO if nLineIndex  == -1
    const DWORD nLineEnd = TextLineEnd(m_hText, nLineIndex);
    const int nLineCount = min(nBufSize, nLineEnd - nLineIndex);

    PCWSTR const buffer = (PCWSTR) LocalLock(m_hText);
    StrCpyN(copy, buffer + nLineIndex, nLineCount);
    LocalUnlock(m_hText);
    return nLineCount;
}

DWORD RadEdit::OnLineFromChar(HWND hWnd, int nCharIndex)
{
    if (nCharIndex == -1)
        nCharIndex = m_nSelEnd;
    return TextLineFromChar(m_hText, nCharIndex);
}

BOOL RadEdit::OnSetTabStops(HWND hWnd, LPINT rgTabStops, int nTabStops)
{
    m_nTabStops = nTabStops;
    //memcpy(m_rgTabStops, rgTabStops, nTabStops * sizeof(INT));
    LONG nBaseUnits = LOWORD(GetDialogBaseUnits());
    int cs = AveCharWidth() * 2;
    for (int i = 0; i < nTabStops; ++i)
        m_rgTabStops[i] = MulDiv(rgTabStops[i], cs, nBaseUnits);
    return TRUE;
}

DWORD RadEdit::OnGetFirstVisibleLine(HWND hWnd)
{
    return GetFirstVisibleLine(hWnd);
}

BOOL RadEdit::OnSetReadOnly(HWND hWnd, BOOL bReadOnly)
{
    int style = GetWindowStyle(hWnd);
    if (bReadOnly)
        style |= ES_READONLY;
    else
        style &= ~ES_READONLY;
    SetWindowLong(hWnd, GWL_STYLE, style);
    return TRUE;
}

EDITWORDBREAKPROC RadEdit::OnGetWordBreakProc(HWND hWnd)
{
    return m_pEditWordBreakProc;
}

void RadEdit::OnSetWordBreakProc(HWND hWnd, EDITWORDBREAKPROC pEWB)
{
    m_pEditWordBreakProc = pEWB;
}

void RadEdit::OnSetMargins(HWND hWnd, UINT nFlags, UINT nLeftMargin, UINT nRightMargin)
{
    if (nFlags & EC_LEFTMARGIN)
        m_nMarginLeft = nLeftMargin;
    if (nFlags & EC_RIGHTMARGIN)
        m_nMarginRight = nRightMargin;
}

LRESULT RadEdit::OnGetMargins(HWND hWnd)
{
    return MAKELRESULT(m_nMarginLeft, m_nMarginRight);
}

POINT RadEdit::OnPosFromChar(HWND hWnd, DWORD nChar)
{
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

    const int nFirstLine = GetFirstVisibleLine(hWnd);
    POINT p = { sPos.cx - (int) GetFirstVisibleCol(hWnd) * AveCharWidth() + MarginLeft(), (nLine - nFirstLine) * LineHeight() };
    return p;
}

LRESULT RadEdit::OnCharFromPos(HWND hWnd, POINT pos)
{
    const int nFirstLine = GetFirstVisibleLine(hWnd);
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
    const int x = pos.x + GetFirstVisibleCol(hWnd) * AveCharWidth() - AveCharWidth() / 2 - MarginLeft();
    const int nCol = x < 0 ? 0 : ::GetTabbedTextExtentEx(hDC, buffer + nLineIndex, nCount, m_nTabStops, m_rgTabStops, x);
    LocalUnlock(m_hText);
    SelectFont(hDC, of);
    ReleaseDC(hWnd, hDC);

    return MAKELRESULT(nLineIndex + nCol, nLine);
}

#undef HANDLE_WM_SETTEXT // Need return value
#define HANDLE_WM_SETTEXT(hwnd, wParam, lParam, fn) \
    (fn)((hwnd), (LPCTSTR)(lParam))

#define HANDLE_EM_GETSEL(hWnd, wParam, lParam, fn) \
    (LRESULT)(fn)(hWnd, (LPDWORD) wParam, (LPDWORD) lParam)
#define HANDLE_EM_SETSEL(hWnd, wParam, lParam, fn) \
    ((fn)(hWnd, (DWORD) wParam, (DWORD) lParam), 0L)
#define HANDLE_EM_SCROLL(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, (UINT) wParam)
#define HANDLE_EM_LINESCROLL(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, (int) wParam, (int) lParam)
#define HANDLE_EM_SCROLLCARET(hWnd, wParam, lParam, fn) \
    ((fn)(hWnd), 0L)
#define HANDLE_EM_GETMODIFY(hWnd, wParam, lParam, fn) \
    (fn)(hWnd)
#define HANDLE_EM_SETMODIFY(hWnd, wParam, lParam, fn) \
    ((fn)(hWnd, (BOOL) wParam), 0L)
#define HANDLE_EM_GETLINECOUNT(hWnd, wParam, lParam, fn) \
    (fn)(hWnd)
#define HANDLE_EM_LINEINDEX(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, (int) wParam)
#define HANDLE_EM_SETHANDLE(hWnd, wParam, lParam, fn) \
    ((fn)(hWnd, (HLOCAL) wParam), 0L)
#define HANDLE_EM_GETHANDLE(hWnd, wParam, lParam, fn) \
    (LRESULT)(fn)(hWnd)
#define HANDLE_EM_GETTHUMB(hWnd, wParam, lParam, fn) \
    (fn)(hWnd)
#define HANDLE_EM_LINELENGTH(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, (int) wParam)
#define HANDLE_EM_REPLACESEL(hWnd, wParam, lParam, fn) \
    ((fn)(hWnd, (PCTSTR) lParam, (BOOL) wParam), 0L)
#define HANDLE_EM_GETLINE(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, (int) wParam, (PTCHAR) lParam, *(LPWORD) lParam)
#define HANDLE_EM_LINEFROMCHAR(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, (int) wParam)
#define HANDLE_EM_SETTABSTOPS(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, (LPINT) lParam, (int) wParam)
#define HANDLE_EM_GETFIRSTVISIBLELINE(hWnd, wParam, lParam, fn) \
    (fn)(hWnd)
#define HANDLE_EM_SETREADONLY(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, (BOOL) wParam)
#define HANDLE_EM_GETWORDBREAKPROC(hWnd, wParam, lParam, fn) \
    (LRESULT)(fn)(hWnd)
#define HANDLE_EM_SETWORDBREAKPROC(hWnd, wParam, lParam, fn) \
    ((fn)(hWnd, (EDITWORDBREAKPROC) lParam), 0)
#define HANDLE_EM_SETMARGINS(hWnd, wParam, lParam, fn) \
    ((fn)(hWnd, (UINT) wParam, LOWORD(lParam), HIWORD(lParam)), 0)
#define HANDLE_EM_GETMARGINS(hWnd, wParam, lParam, fn) \
    (fn)(hWnd)
#define HANDLE_EM_POSFROMCHAR(hWnd, wParam, lParam, fn) \
    ToLResult((fn)(hWnd, (DWORD) wParam))
#define HANDLE_EM_CHARFROMPOS(hWnd, wParam, lParam, fn) \
    (fn)(hWnd, ToPoint(lParam))

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
    HANDLE_MSG(hWnd, WM_GETDLGCODE,    OnGetDlgCode);

    HANDLE_MSG(hWnd, EM_GETSEL,        OnGetSel);
    HANDLE_MSG(hWnd, EM_SETSEL,        OnSetSel);
    // HANDLE_MSG(hWnd, EM_GETRECT, OnGetRect);
    // HANDLE_MSG(hWnd, EM_SETRECT, OnSetRect);
    // HANDLE_MSG(hWnd, EM_SETRECTNP, OnSetRectNP);
    HANDLE_MSG(hWnd, EM_SCROLL,        OnScroll);
    HANDLE_MSG(hWnd, EM_LINESCROLL,    OnLineScroll);
    HANDLE_MSG(hWnd, EM_SCROLLCARET,   OnScrollCaret);
    HANDLE_MSG(hWnd, EM_GETMODIFY,     OnGetModify);
    HANDLE_MSG(hWnd, EM_SETMODIFY,     OnSetModify);
    HANDLE_MSG(hWnd, EM_GETLINECOUNT,  OnGetLineCount);
    HANDLE_MSG(hWnd, EM_LINEINDEX,     OnLineIndex);
    HANDLE_MSG(hWnd, EM_SETHANDLE,     OnSetHandle);
    HANDLE_MSG(hWnd, EM_GETHANDLE,     OnGetHandle);
    HANDLE_MSG(hWnd, EM_GETTHUMB,      OnGetThumb);
    HANDLE_MSG(hWnd, EM_LINELENGTH,    OnLineLength);
    HANDLE_MSG(hWnd, EM_REPLACESEL,    OnReplaceSel);
    HANDLE_MSG(hWnd, EM_GETLINE,       OnGetLine);
    //HANDLE_MSG(hWnd, EM_LIMITTEXT, OnLimitText);  // Same as EM_SETLIMITTEXT
    // TODO HANDLE_MSG(hWnd, EM_CANUNDO, OnCanUndo);
    // TODO HANDLE_MSG(hWnd, EM_UNDO, OnUndo);
    // HANDLE_MSG(hWnd, EM_FMTLINES, OnFmtLines);
    HANDLE_MSG(hWnd, EM_LINEFROMCHAR, OnLineFromChar);
    HANDLE_MSG(hWnd, EM_SETTABSTOPS, OnSetTabStops);
    //HANDLE_MSG(hWnd, EM_SETPASSWORDCHAR, OnSetPasswordChar);  // Ignore
    // TODO HANDLE_MSG(hWnd, EM_EMPTYUNDOBUFFER, OnEmptyUndoBuffer);
    HANDLE_MSG(hWnd, EM_GETFIRSTVISIBLELINE, OnGetFirstVisibleLine);
    HANDLE_MSG(hWnd, EM_SETREADONLY, OnSetReadOnly);
    HANDLE_MSG(hWnd, EM_GETWORDBREAKPROC, OnGetWordBreakProc);
    HANDLE_MSG(hWnd, EM_SETWORDBREAKPROC, OnSetWordBreakProc);
    //HANDLE_MSG(hWnd, EM_GETPASSWORDCHAR, OnGetPasswordChar);
    HANDLE_MSG(hWnd, EM_SETMARGINS, OnSetMargins);
    HANDLE_MSG(hWnd, EM_GETMARGINS, OnGetMargins);
    //HANDLE_MSG(hWnd, EM_SETLIMITTEXT, OnSetLimitText);  // Ignore
    //HANDLE_MSG(hWnd, EM_GETLIMITTEXT, OnGetLimitText);
    HANDLE_MSG(hWnd, EM_POSFROMCHAR, OnPosFromChar);
    HANDLE_MSG(hWnd, EM_CHARFROMPOS, OnCharFromPos);
    //HANDLE_MSG(hWnd, EM_SETIMESTATUS, OnSetIMEStatus);
    //HANDLE_MSG(hWnd, EM_GETIMESTATUS, OnGetIMEStatus);
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
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
