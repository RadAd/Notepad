#pragma once

enum Encoding;

BOOL SelectFilename(HWND hWnd, BOOL bLoad, PTCHAR pszFileName, DWORD nMaxSize, Encoding* pEncoding);
HFONT ChooseFont(HWND hWnd, HFONT hFont);
