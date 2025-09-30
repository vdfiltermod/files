///////////////////////////////////////////////////////////////////////
// MPEG-2 Plugin for VirtualDub 1.8.1+
// Copyright (C) 2007-2012 fccHandler
// Copyright (C) 1998-2012 Avery Lee
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
///////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
	#define _CRT_SECURE_NO_WARNINGS
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <crtdbg.h>

#include "vd2/plugin/vdinputdriver.h"
#include "Unknown.h"
#include "InputOptionsMPEG2.h"
#include "resource.h"


/************************************************************************
 *
 *                  InputOptionsMPEG2
 *
 ************************************************************************/

extern "C" const VDXPluginInfo *const * VDXAPIENTRY VDGetPluginInfo();

static const char MyRegKey[] = "Software\\fccHandler\\Plugins\\MPEG2";

extern HMODULE g_hModule;	// InputFileMPEG2.cpp


InputOptionsMPEG2::InputOptionsMPEG2()
{
	HKEY hKey;

	opts.len = sizeof(opts);

	// Defaults
	opts.fCreateIndex  = false;
	opts.fAllowDSC     = false;
	opts.fAllowMatrix  = false;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, MyRegKey,
		0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD cbData = opts.len;
		RegQueryValueExA(hKey, "Options", NULL, NULL, (LPBYTE)&opts, &cbData);
		RegCloseKey(hKey);
	}

	// Ignore registry fMultipleOpen value
	opts.fMultipleOpen = false;
}


uint32 VDXAPIENTRY InputOptionsMPEG2::Write(void *buf, uint32 buflen)
{
	if (buf != NULL && buflen >= opts.len)
	{
		memcpy(buf, &opts, opts.len);
	}
	return opts.len;
}


INT_PTR CALLBACK InputOptionsMPEG2::OptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	InputOptionsMPEG2 *thisPtr;
	WORD wChosen;

	switch (message) {

	case WM_INITDIALOG:
		SetWindowLongPtrA(hDlg, DWLP_USER, lParam);
		thisPtr = (InputOptionsMPEG2 *)lParam;
		InputOptionsMPEG2::InitDialogTitle(hDlg, "Extended Options");
		CheckDlgButton(hDlg, IDC_MULTIPLE,     thisPtr->opts.fMultipleOpen? BST_CHECKED: BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_WRITE_INDEX,  thisPtr->opts.fCreateIndex?  BST_CHECKED: BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_ALLOW_DSC,    thisPtr->opts.fAllowDSC?     BST_CHECKED: BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_ALLOW_MATRIX, thisPtr->opts.fAllowMatrix?  BST_CHECKED: BST_UNCHECKED);
		return TRUE;

	case WM_COMMAND:
		wChosen = LOWORD(wParam);
		if (wChosen == IDC_SAVE || wChosen == IDOK || wChosen == IDCANCEL)
		{
			if (wChosen != IDCANCEL)
			{
				thisPtr = (InputOptionsMPEG2 *)GetWindowLongPtrA(hDlg, DWLP_USER);

				// fMultipleOpen is not saved to registry
				thisPtr->opts.fMultipleOpen = false;
				thisPtr->opts.fCreateIndex  = (IsDlgButtonChecked(hDlg, IDC_WRITE_INDEX)  == BST_CHECKED);
				thisPtr->opts.fAllowDSC     = (IsDlgButtonChecked(hDlg, IDC_ALLOW_DSC)    == BST_CHECKED);
				thisPtr->opts.fAllowMatrix  = (IsDlgButtonChecked(hDlg, IDC_ALLOW_MATRIX) == BST_CHECKED);

				if (wChosen == IDC_SAVE)
				{
					HKEY hKey;

					if (RegCreateKeyExA(HKEY_CURRENT_USER, MyRegKey,
						0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
						NULL, &hKey, NULL) == ERROR_SUCCESS)
					{
						RegSetValueExA(hKey, "Options", 0, REG_BINARY,
							(CONST BYTE *)&thisPtr->opts, thisPtr->opts.len);

						RegCloseKey(hKey);
					}
				}

				thisPtr->opts.fMultipleOpen = (IsDlgButtonChecked(hDlg, IDC_MULTIPLE)     == BST_CHECKED);
			}
			EndDialog(hDlg, TRUE);
			return TRUE;
		}

	default:
		break;
	}

	return FALSE;
}


void InputOptionsMPEG2::InitDialogTitle(HWND hDlg, char *pTitle)
{
	char buf[64];
	int i;

	uint32 ver = (*VDGetPluginInfo())->mVersion;

	i = _snprintf(buf, sizeof(buf), "MPEG-2 Plugin v%i.%i",
		(ver >> 24) & 0xFF, (ver >> 16) & 0xFF);

	if (i > 0 && i < sizeof(buf) && pTitle != NULL && *pTitle != '\0')
	{
		_snprintf(buf + i, sizeof(buf) - i, " - %s", pTitle);
	}

	buf[sizeof(buf) - 1] = '\0';
	SetWindowTextA(hDlg, buf);
}


void InputOptionsMPEG2::OptionsDlg(HWND hwndParent)
{
	DialogBoxParamA(g_hModule, MAKEINTRESOURCEA(IDD_OPTIONS),
		hwndParent, OptionsDlgProc, (LPARAM)this);
}

