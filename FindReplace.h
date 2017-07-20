#pragma once

extern const UINT g_FindMsg;

BOOL IsFindDialogMessage(LPMSG lpMsg);
void FindReplaceInit(HWND hOwner);
void DoFindDlg();
void DoReplaceDlg();
void DoFindNext();
LRESULT OnFindMsg(HWND hEdit, LPTSTR szTitle, WPARAM wParam, LPARAM lParam);
void OnDlgTerm();
