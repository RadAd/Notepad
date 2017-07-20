#include "stdafx.h"
#include "FileIO.h"

BYTE g_BomBytes[][4] = {
    { 0 },  // BOM_ANSI
    { u'\xFF', u'\xFE', 0 }, // BOM_UTF16_LE
    { u'\xFE', u'\xFF', 0 }, // BOM_UTF16_BE
    { u'\xEF', u'\xBB', u'\xBF', 0 }, // BOM_UTF8
};

static void byteswap16(unsigned short* data, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        data[i] = _byteswap_ushort(data[i]);
}

static UINT GetBomLength(Encoding eEncoding)
{
    PBYTE pBomData = g_BomBytes[eEncoding];
    int i = 0;
    for (i = 0; pBomData[i] != 0; ++i)
        ;
    return i;
}

static BOOL IsBom(PBYTE pData, PBYTE pBomData)
{
    int i = 0;
    for (i = 0; pBomData[i] != 0; ++i)
        if (pBomData[i] != pData[i])
            break;
    return (pBomData[i] == 0);
}

static Encoding CheckBom(PBYTE pData)
{
    for (Encoding eEncoding : g_EncodingList)
    {
        if (g_BomBytes[eEncoding][0] != 0 && IsBom(pData, g_BomBytes[eEncoding]))
            return eEncoding;
    }
    return BOM_ANSI;
}

static HLOCAL ConvertMBToWC(HLOCAL hMemMB, PDWORD pdwChars)
{
    PCHAR mbstr = (PCHAR) LocalLock(hMemMB);
    size_t Chars = strlen(mbstr) + 1;

    size_t WCSize = Chars * sizeof(WCHAR);
    HLOCAL hMemWC = LocalAlloc(LMEM_FIXED, WCSize);

    PWCHAR wcstr = (PWCHAR) LocalLock(hMemWC);
    mbstowcs_s(&Chars, wcstr, Chars, mbstr, _TRUNCATE);
    LocalUnlock(hMemWC);

    LocalUnlock(hMemMB);

    *pdwChars = (DWORD) Chars - 1;
    return hMemWC;
}

static HLOCAL ConvertWCToMB(HLOCAL hMemWC, PDWORD pdwChars)
{
    PCWCHAR wcstr = (PCWCHAR) LocalLock(hMemWC);
    size_t Chars = wcslen(wcstr) + 1;

    size_t MBSize = Chars * sizeof(CHAR);
    HLOCAL hMemMB = LocalAlloc(LMEM_FIXED, MBSize);

    PCHAR mbstr = (PCHAR) LocalLock(hMemMB);
    wcstombs_s(&Chars, mbstr, Chars, wcstr, _TRUNCATE);
    LocalUnlock(hMemMB);

    LocalUnlock(hMemWC);

    *pdwChars = (DWORD) Chars - 1;
    return hMemMB;
}

static HLOCAL LocalCopy(HLOCAL hMem)
{
    SIZE_T sSize = LocalSize(hMem);
    HLOCAL hMemNew = LocalAlloc(LMEM_FIXED, sSize);
    LPBYTE pBufferOld = (LPBYTE) LocalLock(hMem);
    LPBYTE pBufferNew = (LPBYTE) LocalLock(hMemNew);
    memcpy(pBufferNew, pBufferOld, sSize);
    LocalUnlock(hMemNew);
    LocalUnlock(hMem);
    return hMemNew;
}

static void ByteSwap(HLOCAL hMem)
{
    SIZE_T sSize = LocalSize(hMem);
    LPBYTE pBuffer = (LPBYTE) LocalLock(hMem);
    byteswap16((unsigned short*) pBuffer, sSize / sizeof(unsigned short));
    LocalUnlock(hMem);
}

static HLOCAL ConvertToUTF16LE(HLOCAL hMem, Encoding eEncoding)
{
    // NOTE: Free to modify contents of hMem
    if (eEncoding == BOM_ANSI || eEncoding == BOM_UTF8)
    {
        DWORD dwChars = 0;
        hMem = ConvertMBToWC(hMem, &dwChars);
    }
    else if (eEncoding == BOM_UTF16_BE)
    {
        ByteSwap(hMem);
    }
    return hMem;
}

static HLOCAL ConvertFromUTF16LE(HLOCAL hMem, Encoding eEncoding, PDWORD pdwBytes)
{
    // NOTE: NOT Free to modify contents of hMem
    *pdwBytes = 0;
    if (eEncoding == BOM_ANSI || eEncoding == BOM_UTF8)
    {
        DWORD dwChars = 0;
        hMem = ConvertWCToMB(hMem, &dwChars);

        *pdwBytes = dwChars * sizeof(CHAR);
    }
    else // UTF16
    {
        if (eEncoding == BOM_UTF16_BE)
        {
            hMem = LocalCopy(hMem);
            ByteSwap(hMem);
        }

        LPWSTR pStr = (LPWSTR) LocalLock(hMem);
        UINT dwChars = (UINT) wcslen(pStr);
        LocalUnlock(hMem);

        *pdwBytes = dwChars * sizeof(WCHAR);
    }

    return hMem;
}

HLOCAL NewFile()
{
    SIZE_T uBytes = sizeof(WCHAR);
    HLOCAL hMem = LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, uBytes);

    return hMem;
}

HLOCAL LoadFile(LPCTSTR pszFileName, Encoding* pEncoding)
{
    HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);
        return NULL;
    }

    HLOCAL hMem = LocalAlloc(LMEM_FIXED, dwFileSize + 1);
    {
        LPBYTE pBuffer = (LPBYTE) LocalLock(hMem);
        DWORD dwTotalRead = 0;
        DWORD dwRead = 0;
        while (dwFileSize > dwTotalRead &&  ReadFile(hFile, pBuffer + dwTotalRead, dwFileSize - dwTotalRead, &dwRead, NULL) == TRUE)
        {
            dwTotalRead += dwRead;
        }
        pBuffer[dwTotalRead] = 0;
        *pEncoding = CheckBom(pBuffer);
        {
            UINT iBomLength = GetBomLength(*pEncoding);
            memmove(pBuffer, pBuffer + iBomLength, dwFileSize - iBomLength + 1);
        }
        LocalUnlock(hMem);
    }

    CloseHandle(hFile);

    HLOCAL hMemNew = ConvertToUTF16LE(hMem, *pEncoding);
    if (hMemNew != hMem)
        LocalFree(hMem);

    return hMemNew;
}

void SaveFile(HLOCAL hMem, LPCTSTR pszFileName, Encoding eEncoding)
{
    DWORD dwBytes = 0;
    HLOCAL hMemOrig = hMem;
    hMem = ConvertFromUTF16LE(hMem, eEncoding, &dwBytes);

    HANDLE hFile = CreateFile(pszFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    {
        LPBYTE pBuffer = (LPBYTE) LocalLock(hMem);
        DWORD dwWritten = 0;
        WriteFile(hFile, g_BomBytes[eEncoding], GetBomLength(eEncoding), &dwWritten, NULL);
        DWORD dwTotalWritten = 0;
        while (dwBytes > dwTotalWritten &&  WriteFile(hFile, pBuffer + dwTotalWritten, dwBytes - dwTotalWritten, &dwWritten, NULL) == TRUE)
        {
            dwTotalWritten += dwWritten;
        }
        LocalUnlock(hMem);
    }

    CloseHandle(hFile);

    if (hMemOrig != hMem)
        LocalFree(hMem);
}
