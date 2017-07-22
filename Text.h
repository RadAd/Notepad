#pragma once

// TODO Consistently use DWORD for char pos
// TODO Support unicode use CharPrev/CharNext or IsDBCSLeadByte

int TextLength(HLOCAL hText);

int NewLineNextLength(PCWSTR buffer, int nIndex)
{
    if (nIndex < 0)
        return -1;
    else if (buffer[nIndex] == L'\0')
        return -1;
    else if (buffer[nIndex] == L'\r' && buffer[nIndex + 1] == L'\n')
        return 2;
    else if (buffer[nIndex] == L'\r')
        return 1;
    else if (buffer[nIndex] == L'\n')
        return 1;
    else
        return -1;
}

int NewLinePrevLength(PCWSTR buffer, int nIndex)
{
    if (nIndex < 1)
        return -1;
    else if (nIndex >= 2 && buffer[nIndex - 2] == L'\r' && buffer[nIndex - 1] == L'\n')
        return 2;
    else if (buffer[nIndex - 1] == L'\r')
        return 1;
    else if (buffer[nIndex - 1] == L'\n')
        return 1;
    else
        return -1;
}

int GetNextChar(PCWSTR buffer, int nIndex)
{
    if (buffer[nIndex] == L'\0')
        return -1; // TODO Or return nIndex
    else if (NewLineNextLength(buffer, nIndex) > 0)
        return nIndex + NewLineNextLength(buffer, nIndex);
    else
        return nIndex + 1;
}

int GetNextChar(HLOCAL hText, int nIndex)
{
    PCWSTR const buffer = (PCWSTR) LocalLock(hText);
    nIndex = GetNextChar(buffer, nIndex);
    LocalUnlock(hText);
    return nIndex;
}

int GetPrevChar(PCWSTR buffer, int nIndex)
{
    if (nIndex <= 0)
        return -1; // TODO Or return nIndex
    else if (NewLinePrevLength(buffer, nIndex) > 0)
        return nIndex - NewLinePrevLength(buffer, nIndex);
    else
        return nIndex - 1;
}

int GetPrevChar(HLOCAL hText, int nIndex)
{
    PCWSTR const buffer = (PCWSTR) LocalLock(hText);
    nIndex = GetPrevChar(buffer, nIndex);
    LocalUnlock(hText);
    return nIndex;
}

int CALLBACK DefaultEditWordBreakProc(LPWSTR lpch, int ichCurrent, int cch, int code)
{
    switch (code)
    {
    case WB_ISDELIMITER:
        {
            WCHAR c = lpch[ichCurrent];
            ichCurrent = iswspace(c);
        }
        return ichCurrent;
        break;

    case WB_LEFT:
        if (iswblank(lpch[ichCurrent - 1]))
        {
            while (ichCurrent > 0 && iswblank(lpch[ichCurrent - 1]))
                --ichCurrent;
        }
        else if (ichCurrent >= 2 && NewLinePrevLength(lpch, ichCurrent) == 2)
        {
            ichCurrent -= 2;
        }
        else //if (!iswblank(lpch[ichCurrent - 1]))
        {
            while (ichCurrent > 0 && !iswspace(lpch[ichCurrent - 1]))
                --ichCurrent;
        }
        return ichCurrent;
        break;

    case WB_RIGHT:
        if (iswblank(lpch[ichCurrent]))
        {
            while (ichCurrent < (cch - 1) && iswblank(lpch[ichCurrent]))
                ++ichCurrent;
        }
        else if (ichCurrent <= (cch - 2) && NewLineNextLength(lpch, ichCurrent) == 2)
        {
            ichCurrent += 2;
        }
        else //if (!iswblank(lpch[ichCurrent]))
        {
            while (ichCurrent < (cch - 1) && !iswspace(lpch[ichCurrent]))
                ++ichCurrent;
        }
        return ichCurrent;
        break;

    default:
        ASSERT(FALSE);
        return -1;
    }
}

int MoveWord(EDITWORDBREAKPROC ewb, int code, HLOCAL hText, int nIndex)
{
    ASSERT(ewb != nullptr);
    int iTextLength = TextLength(hText);
    PWSTR const buffer = (PWSTR) LocalLock(hText);
    nIndex = ewb(buffer, nIndex, iTextLength, code);
    LocalUnlock(hText);
    return nIndex;
}

int GetNextLine(PCWSTR buffer, int nIndex)
{
    ASSERT(buffer != NULL);

    PCWSTR end = StrChr(buffer + nIndex, L'\n'); // TODO Handle different line endings
    return end != nullptr ? static_cast<int>(end - buffer + 1) : -1;
}

int GetLineStart(PCWSTR buffer, int nIndex)
{
    PCWSTR const end = StrRChr(buffer, buffer + nIndex, L'\n'); // TODO Handle different line endings
    return end != nullptr ? static_cast<int>(end - buffer + 1) : 0;
}

int GetLineEnd(PCWSTR buffer, int nIndex)
{
    PCWSTR end = StrChr(buffer + nIndex, L'\r'); // TODO Handle different line endings
    return end != nullptr ? static_cast<int>(end - buffer) : nIndex + (int) wcslen(buffer + nIndex);
}

HLOCAL TextCreate(PCWSTR const pText)
{
    int nLength = (int) wcslen(pText) + 1;
    HLOCAL hText = LocalAlloc(LMEM_MOVEABLE, nLength * sizeof(WCHAR));
    PWSTR const buffer = (PWSTR) LocalLock(hText);
    memcpy(buffer, pText, nLength * sizeof(WCHAR));
    LocalUnlock(hText);
    return hText;
}

int TextLength(HLOCAL hText)
{
    ASSERT(hText != NULL);

    PCWSTR const buffer = (PCWSTR) LocalLock(hText);
    int nLength = (int) wcslen(buffer);
    LocalUnlock(hText);
    return nLength;
}

int TextLineCount(HLOCAL hText)
{
    ASSERT(hText != NULL);

    int nLine = 0;
    PCWSTR const buffer = (PCWSTR) LocalLock(hText);
    int nIndex = 0;
    while (nIndex >= 0)
    {
        nIndex = GetNextLine(buffer, nIndex);
        ++nLine;
    }
    LocalUnlock(hText);
    return nLine;
}

int TextLineStart(HLOCAL hText, int nIndex)
{
    ASSERT(hText != NULL);
    ASSERT(nIndex >= 0);
    ASSERT(nIndex <= TextLength(hText));

    PCWSTR const buffer = (PCWSTR) LocalLock(hText);
    nIndex = GetLineStart(buffer, nIndex);
    LocalUnlock(hText);
    return nIndex;
}

int TextLineEnd(HLOCAL hText, int nIndex)
{
    ASSERT(hText != NULL);
    ASSERT(nIndex >= 0);
    ASSERT(nIndex <= TextLength(hText));

    PCWSTR const buffer = (PCWSTR) LocalLock(hText);
    nIndex = GetLineEnd(buffer, nIndex);
    LocalUnlock(hText);

    return nIndex;
}

int TextLineFromChar(HLOCAL hText, int nChar)
{
    ASSERT(hText != NULL);
    ASSERT(nChar >= 0);

    int nLine = 0;
    PCWSTR const buffer = (PCWSTR) LocalLock(hText);
    int nIndex = 0;
    while ((nIndex = GetNextLine(buffer, nIndex)) <= nChar)
    {
        if (nIndex == -1)
            break;
        ++nLine;
    }
    LocalUnlock(hText);
    return nLine;
}

int TextLineIndex(HLOCAL hText, int nLine)
{
    ASSERT(hText != NULL);
    ASSERT(nLine >= 0);

    PCWSTR const buffer = (PCWSTR) LocalLock(hText);
    int nIndex = 0;
    while (nIndex >= 0 && nLine > 0)
    {
        nIndex = GetNextLine(buffer, nIndex);
        --nLine;
    }
    LocalUnlock(hText);
    return nIndex;
}

HLOCAL TextReplace(HLOCAL hText, const int nStart, DWORD& nEnd, PCWSTR const pText)
{
    ASSERT(hText != NULL);
    ASSERT(nStart <= nEnd);
    ASSERT(pText != nullptr);

    const int nTextLen = (int) wcslen(pText);
    const int nDiff = nTextLen - (nEnd - nStart);
    const int nLen = TextLength(hText) + 1;

    ASSERT(nStart >= 0 && nStart <= nLen);
    ASSERT(nEnd >= 0 && nEnd <= nLen);

    HLOCAL hTextNew = LocalReAlloc(hText, (nLen + nDiff) * sizeof(WCHAR), LMEM_MOVEABLE);
    if (hTextNew == NULL)
        return NULL;
    hText = hTextNew;

    PWSTR const buffer = (PWSTR) LocalLock(hText);
    memmove(buffer + nEnd + nDiff, buffer + nEnd, (nLen - nEnd) * sizeof(WCHAR));
    memcpy(buffer + nStart, pText, nTextLen * sizeof(WCHAR));
    LocalUnlock(hText);

    nEnd += nDiff;

    return hText;
}
