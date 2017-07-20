#pragma once

enum Encoding
{
    BOM_ANSI,
    BOM_UTF16_LE,
    BOM_UTF16_BE,
    BOM_UTF8
};
static const Encoding g_EncodingList[] = { BOM_ANSI, BOM_UTF16_LE, BOM_UTF16_BE, BOM_UTF8 };

HLOCAL NewFile();
HLOCAL LoadFile(LPCTSTR pszFileName, Encoding* pEncoding);
void SaveFile(HLOCAL hMem, LPCTSTR pszFileName, Encoding eEncoding);
