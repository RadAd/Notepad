#include "stdafx.h"
#include "CommDlgX.h"
#include "FileIO.h"
#include "resource.h"

#define MAX_LOADSTRING 128

extern HINSTANCE g_hInst;

static const int g_BomName[] = {
    IDS_BOM_ANSI,  // BOM_ANSI
    IDS_BOM_UTF16_LE, // BOM_UTF16_LE
    IDS_BOM_UTF16_BE, // BOM_UTF16_BE
    IDS_BOM_UTF8, // BOM_UTF8
};


static PTCHAR NextFilter(PTCHAR lpstrFilter)
{
    lpstrFilter += wcslen(lpstrFilter) + 1;
    lpstrFilter += wcslen(lpstrFilter) + 1;
    return lpstrFilter;
}

BOOL SelectFilename(HWND hWnd, BOOL bLoad, PTCHAR pszFileName, DWORD nMaxSize, Encoding* pEncoding)
{
    TCHAR szFilter[MAX_LOADSTRING];
    LoadString(g_hInst, IDS_FILTER, szFilter, MAX_LOADSTRING);
    PTCHAR pszReplace = szFilter;
    while ((pszReplace = StrChr(pszReplace, TEXT('|'))) != nullptr)
    {
        *pszReplace = TEXT('\0');
        ++pszReplace;
    }

    if (true)
    {
        IFileDialog* pIFileDialog;
        IFileDialogCustomize* pIFileDialogCustomize;

        HRESULT hr;

        //USE_INTERFACE_PART_STD(FileDialogEvents);
        //USE_INTERFACE_PART_STD(FileDialogControlEvents);

        hr = CoCreateInstance(bLoad ? CLSID_FileOpenDialog : CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIFileDialog));
        if (FAILED(hr))
        {
            return FALSE;
        }

        hr = pIFileDialog->QueryInterface(IID_PPV_ARGS(&pIFileDialogCustomize));
        if (FAILED(hr))
        {
            return FALSE;
        }

        pIFileDialog->SetFileName(pszFileName);
        pIFileDialog->SetDefaultExtension(L"txt");
        //FILEOPENDIALOGOPTIONS fos;  // https://msdn.microsoft.com/en-us/library/windows/desktop/dn457282(v=vs.85).aspx
        //pIFileDialog->GetOptions(&fos);
        //pIFileDialog->SetOptions(fos);
        //pIFileDialog->SetFilter();
        UINT nFilterCount = 0;
        for (PTCHAR iFilter = szFilter; iFilter[0] != L'\0'; iFilter = NextFilter(iFilter))
            ++nFilterCount;
        COMDLG_FILTERSPEC* pFilter = new COMDLG_FILTERSPEC[nFilterCount];
        int i = 0;
        for (PTCHAR iFilter = szFilter; iFilter[0] != L'\0'; iFilter = NextFilter(iFilter))
        {
            pFilter[i].pszName = iFilter;
            pFilter[i].pszSpec = iFilter + wcslen(iFilter) + 1;
            ++i;
        }
        hr = pIFileDialog->SetFileTypes(nFilterCount, pFilter);
        //hr = pIFileDialog->SetFileTypeIndex(m_ofn.nFilterIndex > 1 ? m_ofn.nFilterIndex : 1);

        const DWORD idCtrlEncoding = 1;
        TCHAR sEncoding[MAX_LOADSTRING];
        LoadString(g_hInst, IDS_ENCODING, sEncoding, MAX_LOADSTRING);
        pIFileDialogCustomize->StartVisualGroup((DWORD) -1, sEncoding);
        pIFileDialogCustomize->AddComboBox(idCtrlEncoding);
        for (Encoding eEncoding : g_EncodingList)
        {
            TCHAR sBuffer[MAX_LOADSTRING];
            LoadString(g_hInst, g_BomName[eEncoding], sBuffer, MAX_LOADSTRING);
            pIFileDialogCustomize->AddControlItem(idCtrlEncoding, eEncoding, sBuffer);
        }
        pIFileDialogCustomize->SetSelectedControlItem(idCtrlEncoding, *pEncoding);
        pIFileDialogCustomize->EndVisualGroup();

        hr = pIFileDialog->Show(hWnd);
        if (hr == S_OK)
        {
            IShellItem *psiResult;
            hr = pIFileDialog->GetResult(&psiResult);
            if (SUCCEEDED(hr))
            {
                SFGAOF sfgaoAttribs;
                if ((psiResult->GetAttributes(SFGAO_STREAM, &sfgaoAttribs) == S_FALSE)
                    && (psiResult->GetAttributes(SFGAO_FOLDER, &sfgaoAttribs) == S_OK))
                {
                    ;
                }
                else
                {
                    LPWSTR wcPathName = NULL;
                    hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &wcPathName);
                    if (SUCCEEDED(hr))
                    {
                        wcscpy_s(pszFileName, nMaxSize, wcPathName);
                        CoTaskMemFree(wcPathName);
                    }
                }
                psiResult->Release();
            }

            DWORD dwSelectedEncoding = 0;
            pIFileDialogCustomize->GetSelectedControlItem(idCtrlEncoding, &dwSelectedEncoding);
            *pEncoding = (Encoding) dwSelectedEncoding;

            pIFileDialogCustomize->Release();
            pIFileDialog->Release();
            return TRUE;
        }
        else
        {
            // HRESULT_FROM_WIN32(ERROR_CANCELLED)
            pIFileDialogCustomize->Release();
            pIFileDialog->Release();
            return FALSE;
        }
    }
    else
    {
        *pEncoding = BOM_ANSI;

        OPENFILENAME ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hWnd;
        ofn.lpstrFilter = szFilter;
        ofn.lpstrFile = pszFileName;
        ofn.nMaxFile = nMaxSize;
        ofn.Flags = OFN_EXPLORER;
        ofn.Flags |= bLoad ? OFN_FILEMUSTEXIST : OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = TEXT("txt");
        if (bLoad)
            return GetOpenFileName(&ofn);
        else
            return GetSaveFileName(&ofn);
    }
}

HFONT ChooseFont(HWND hWnd, HFONT hFont)
{
    LOGFONT lf;
    GetObject(hFont, sizeof(LOGFONT), &lf);

    CHOOSEFONT cf = { sizeof(CHOOSEFONT) };
    cf.Flags = CF_INITTOLOGFONTSTRUCT;
    cf.hwndOwner = hWnd;
    cf.lpLogFont = &lf;

    if (!ChooseFont(&cf))
        return NULL;

    return CreateFontIndirect(&lf);
}
