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

// Dialog for opening multiple files

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#include "infile.h"
#include "append.h"
#include "resource.h"

static bool fAbort;

static WCHAR szInitDir[MAX_PATH + 1];

static int findex = 0;

static const WCHAR szFilter[] =
	L"DVD or elementary video (*.vob,*.m1v,*.m2v)\0;*.vob;*.m1v,*.m2v\0"
	L"All files (*.*)\0*.*\0";

static HWND hwndList;
static HWND hwndAdd;
static HWND hwndRemove;
static HWND hwndMoveUp;
static HWND hwndMoveDown;
static HWND hwndSort;

// This is global
wchar_t *copy_string(const wchar_t *szString) {
	if (szString != NULL) {
		int len = lstrlenW(szString);
		if (len > 0) {
			wchar_t *ret = new wchar_t[len + 1];
			if (ret != NULL) {
				memcpy(ret, szString, (len + 1) * sizeof(wchar_t));
				return ret;
			}
		}
	}
	return NULL;
}

static inline LPWSTR MyOpenDlgA(HWND hwndOwner) {
	OPENFILENAMEA ofn;

	// Wipe, then initialize the structure
	int i = sizeof(ofn);
	memset(&ofn, 0, i);
	ofn.lStructSize = i;
	ofn.hwndOwner = hwndOwner;

	// Initialize lpstrFile
	ofn.lpstrFile = new CHAR[(MAX_PATH + 1) * MAX_FILES];
	if (ofn.lpstrFile == NULL) return NULL;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = (MAX_PATH + 1) * MAX_FILES;

	// Initialize lpstrFilter
	ofn.lpstrFilter = inFile::wchar_to_ansi(szFilter, sizeof(szFilter) / sizeof(WCHAR));
	ofn.nFilterIndex = findex;

	// Initialize lpstrInitialDir from current file (if applicable)
	if (szInitDir[0]) {
		ofn.lpstrInitialDir = (LPSTR)szInitDir;
	}

	ofn.Flags = OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	i = (int)GetOpenFileNameA(&ofn);

	delete[] (char *)ofn.lpstrFilter;

	if (i) {
		LPWSTR tmp;

		// Copy the path (if any) to szInitDir
		int len = lstrlenA(ofn.lpstrFile);
		while (len > 3) {
			CHAR c = ofn.lpstrFile[len - 1];
			if (c != '\\' && c != '/') {
				memcpy(szInitDir, ofn.lpstrFile, len);
				break;
			}
			--len;
		}
		szInitDir[len] = L'\0';

		// Convert to wide char in place
		tmp = (LPWSTR)ofn.lpstrFile;
		for (i = (MAX_PATH + 1) * MAX_FILES - 1; i >= 0; --i) {
			tmp[i] = ofn.lpstrFile[i];
		}
	} else {
		delete[] ofn.lpstrFile;
		ofn.lpstrFile = NULL;
	}

	findex = ofn.nFilterIndex;

	return (LPWSTR)(ofn.lpstrFile);
}

static inline LPWSTR MyOpenDlgW(HWND hwndOwner) {
	OPENFILENAMEW ofn;

	// Wipe, then initialize the structure
	int i = sizeof(ofn);
	memset(&ofn, 0, i);
	ofn.lStructSize = i;
	ofn.hwndOwner = hwndOwner;

	// Initialize lpstrFile
	ofn.lpstrFile = new WCHAR[(MAX_PATH + 1) * MAX_FILES];
	if (ofn.lpstrFile == NULL) return NULL;
	ofn.lpstrFile[0] = L'\0';
	ofn.nMaxFile = (MAX_PATH + 1) * MAX_FILES;

	// Initialize lpstrFilter
	ofn.lpstrFilter = szFilter;
	ofn.nFilterIndex = findex;

	// Initialize lpstrInitialDir
	if (szInitDir[0]) {
		ofn.lpstrInitialDir = szInitDir;
	}

	ofn.Flags = OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	if (GetOpenFileNameW(&ofn)) {

		// Copy the path (if any) to szInitDir
		int len = lstrlenW(ofn.lpstrFile);
		while (len > 3) {
			WCHAR wc = ofn.lpstrFile[len - 1];
			if (wc != L'\\' && wc != L'/') {
				memcpy(szInitDir, ofn.lpstrFile, len * sizeof(WCHAR));
				break;
			}
			--len;
		}
		szInitDir[len] = L'\0';

	} else {
		delete[] ofn.lpstrFile;
		ofn.lpstrFile = NULL;
	}

	findex = ofn.nFilterIndex;

	return ofn.lpstrFile;
}

static inline LPWSTR MyOpenDlg(HWND hwndOwner) {
	if (GetVersion() & 0x80000000) {
		return MyOpenDlgA(hwndOwner);
	} else {
		return MyOpenDlgW(hwndOwner);
	}
}

static bool AddString(AppendNames *an, int from_index, int to_index) {
	bool ret = false;

	if ((unsigned int)from_index < (unsigned int)MAX_FILES) {
		char *tmp = inFile::wchar_to_ansi(an->fName[from_index], -1);
		if (tmp != NULL) {
			int len = lstrlenA(tmp);
			if (len > 0) {
				while (tmp[len - 1] != '\\' && tmp[len - 1] != '/') {
					if (--len <= 0) break;
				}
				SendMessage(hwndList, LB_INSERTSTRING, to_index, (LPARAM)(tmp + len));
				ret = true;
			}
			delete[] tmp;
		}
	}
	return ret;
}

static inline void DoMoveUp(AppendNames *an) {
	int i = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);
	if ((unsigned int)i < (unsigned int)an->num_files && i > 0) {
		// re-insert i at position i - 1,
		// then remove position i + 1

		if (AddString(an, i, i - 1)) {
			LPWSTR tmp;

			SendMessage(hwndList, LB_DELETESTRING, i + 1, 0);

			tmp = an->fName[i];
			an->fName[i] = an->fName[i - 1];
			an->fName[i - 1] = tmp;

			--i;
			SendMessage(hwndList, LB_SETCURSEL, (WPARAM)i, 0);
		}
	}
}

static inline void DoMoveDown(AppendNames *an) {
	int i = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);
	if (an->num_files > 1 && (unsigned int)i < (unsigned int)(an->num_files - 1)) {
		// re-insert i + 1 at position i,
		// then remove position i + 2

		if (AddString(an, i + 1, i)) {
			LPWSTR tmp;

			SendMessage(hwndList, LB_DELETESTRING, i + 2, 0);

			tmp = an->fName[i];
			an->fName[i] = an->fName[i + 1];
			an->fName[i + 1] = tmp;

			++i;
			SendMessage(hwndList, LB_SETCURSEL, (WPARAM)i, 0);
		}
	}
}

static inline void DoSort(AppendNames *an) {

	LPSTR tmp1 = new CHAR[(MAX_PATH + 1) * 2];

	if (tmp1 != NULL) {
		bool sorted;

		LPSTR tmp2 = tmp1 + MAX_PATH + 1;
		int prev = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);

		// Bubble-sort is easiest; speed isn't critical
		do {
			sorted = true;
			for (int n = 1; n < an->num_files; ++n) {
				SendMessage(hwndList, LB_GETTEXT, (WPARAM)(n - 1), (LPARAM)tmp1);
				SendMessage(hwndList, LB_GETTEXT, (WPARAM)n, (LPARAM)tmp2);
				if (CompareStringA(LOCALE_USER_DEFAULT, NORM_IGNORECASE, tmp1, -1, tmp2, -1) == CSTR_GREATER_THAN) {
					SendMessage(hwndList, LB_SETCURSEL, (WPARAM)n, 0);
					DoMoveUp(an);
					sorted = false;
				}
			}
		} while (!sorted);

		delete[] tmp1;

		SendMessage(hwndList, LB_SETCURSEL, (WPARAM)prev, 0);
	}
}

static inline void DoAdd(AppendNames *an, HWND hDlg) {
	if (an->num_files < MAX_FILES) {
		LPWSTR tmp = MyOpenDlg(hDlg);

		if (tmp != NULL) {
			LPWSTR path = tmp;
			int pathlen = lstrlenW(tmp);

			tmp += pathlen + 1;

			if (tmp[0] == 0) {
				// Only one file was selected.
				// Point "tmp" at the filename
				while (pathlen > 0) {
					WCHAR wc = path[pathlen - 1];
					if (wc == L'\\' || wc == L'/') {
						tmp = path + pathlen;
						break;
					}
					--pathlen;
				}
			} else {
				// Multiple files were selected
				if (pathlen > 0) {
					WCHAR wc = path[pathlen - 1];
					if (wc != L'\\' && wc != L'/') {
						path[pathlen] = L'\\';
						++pathlen;
					}
				}
			}

			while (tmp[0]) {
				// Accumulate filenames
				int len = lstrlenW(tmp);
				if (len <= 0) break;
				an->fName[an->num_files] = new WCHAR[pathlen + 1 + len + 1];
				if (an->fName[an->num_files] != NULL) {
					if (pathlen > 0) {
						memcpy(an->fName[an->num_files], path, pathlen * sizeof(WCHAR));
					}
					memcpy(an->fName[an->num_files] + pathlen, tmp, (len + 1) * sizeof(WCHAR));
					if (AddString(an, an->num_files, an->num_files)) {
						++an->num_files;
						if (an->num_files == MAX_FILES) break;
					} else {
						delete[] an->fName[an->num_files];
						an->fName[an->num_files] = NULL;
					}
				}
				tmp += len + 1;
			}

			delete[] path;

			if (an->num_files > 0) {
				SendMessage(hwndList, LB_SETCURSEL, (WPARAM)(an->num_files - 1), 0);
				EnableWindow(hwndRemove, TRUE);
				if (an->num_files > 1) {
					EnableWindow(hwndMoveUp, TRUE);
					EnableWindow(hwndMoveDown, TRUE);
					EnableWindow(hwndSort, TRUE);
				}
			}
		}
	}
}

static inline void DoRemove(AppendNames *an) {
	int i = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);
	if ((unsigned int)i < (unsigned int)an->num_files) {
		SendMessage(hwndList, LB_DELETESTRING, i, 0);

		delete[] an->fName[i];
		an->fName[i] = NULL;
		for (int j = i + 1; j < an->num_files; ++j) {
			an->fName[j - 1] = an->fName[j];
		}
		--an->num_files;
		an->fName[an->num_files] = NULL;

		if (i == an->num_files) --i;
		SendMessage(hwndList, LB_SETCURSEL, (WPARAM)i, 0);

		if (an->num_files < 2) {
			EnableWindow(hwndMoveUp, FALSE);
			EnableWindow(hwndMoveDown, FALSE);
			EnableWindow(hwndSort, FALSE);
			if (an->num_files < 1) {
				EnableWindow(hwndRemove, FALSE);
			}
		}
	}
}


static void AutoLoadVOBs(AppendNames *an) {

	// Given a DVD video object name, automatically load
	// the related objects of the title set.  The expected
	// format is VTS_TT_N.VOB, where "TT" is the title set
	// number, and "N" is the video object number (0 to 9).

	int i, len;

	HANDLE h;
	WCHAR anchor;

	LPWSTR orig = NULL;

	if (an == NULL) goto Abort;
	if (an->num_files != 1) goto Abort;

	orig = copy_string(an->fName[0]);
	if (orig == NULL) goto Abort;

	len = lstrlenW(orig);

	// The filename must have this form:
	// [drive:\path\]VTS_##_#.VOB

	if (len < 12) goto Abort;
	
	// Uppercase it
	for (i = len - 12; i < len; ++i) {
		if (orig[i] >= L'a' && orig[i] <= L'z') {
			orig[i] = L'A' + (orig[i] - L'a');
		}
	}
	
	if (orig[len - 12] != L'V') goto Abort;
	if (orig[len - 11] != L'T') goto Abort;
	if (orig[len - 10] != L'S') goto Abort;
	if (orig[len -  9] != L'_') goto Abort;
	if (orig[len -  8] < L'0' || orig[len - 8] > L'9') goto Abort;
	if (orig[len -  7] < L'0' || orig[len - 7] > L'9') goto Abort;
	if (orig[len -  6] != L'_') goto Abort;
	if (orig[len -  5] < L'0' || orig[len - 5] > L'9') goto Abort;
	if (orig[len -  4] != L'.') goto Abort;
	if (orig[len -  3] != L'V') goto Abort;
	if (orig[len -  2] != L'O') goto Abort;
	if (orig[len -  1] != L'B') goto Abort;
	
	// Input file (at least) must exist
	h = inFile::dualCreateFile(orig, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		
	if (h == INVALID_HANDLE_VALUE) goto Abort;
	CloseHandle(h);
	
	// All video objects prior must exist
	anchor = orig[len - 5];
	
	for (i = anchor - 1; i >= L'1'; --i) {
		orig[len - 5] = (WCHAR)i;

		h = inFile::dualCreateFile(orig, GENERIC_READ, FILE_SHARE_READ,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (h == INVALID_HANDLE_VALUE) goto Abort;
		CloseHandle(h);
	}

	// Are there any subsequent video objects?
	for (i = anchor + 1; i <= L'9'; ++i) {
		orig[len - 5] = (WCHAR)i;

		h = inFile::dualCreateFile(orig, GENERIC_READ, FILE_SHARE_READ,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (h == INVALID_HANDLE_VALUE) break;
		CloseHandle(h);
		++anchor;
	}

	// Finally, fill the video object list
	an->fName[0][len - 5] = L'1';
	
	for (i = '2'; i <= anchor; ++i) {
		if (an->num_files >= MAX_FILES) break;
		
		orig[len - 5] = (WCHAR)i;
		an->fName[an->num_files] = copy_string(orig);
		if (an->fName[an->num_files] != NULL) ++an->num_files;
	}

Abort:
	delete[] orig;
	return;
}

static INT_PTR APIENTRY AppendDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	AppendNames *an = (AppendNames *)GetWindowLongPtr(hDlg, DWLP_USER);

	switch (message) {

    case WM_INITDIALOG:
		{
			int i;
			bool IsDVD = false;

			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			an = (AppendNames *)lParam;

			hwndList		= GetDlgItem(hDlg, IDC_LIST1);
			hwndAdd			= GetDlgItem(hDlg, IDC_ADD);
			hwndRemove		= GetDlgItem(hDlg, IDC_REMOVE);
			hwndMoveUp		= GetDlgItem(hDlg, IDC_MOVEUP);
			hwndMoveDown	= GetDlgItem(hDlg, IDC_MOVEDOWN);
			hwndSort		= GetDlgItem(hDlg, IDC_SORT);

			if (an->num_files == 1) {
				// If this is a DVD, auto-load additional VOBs
				AutoLoadVOBs(an);
				IsDVD = true;
			}

			// Load filenames into ListBox
			for (i = 0; i < an->num_files; ++i) {
				if (an->fName[i] == NULL) break;
				AddString(an, i, i);
			}

			// If this was an auto-loaded DVD, sort the names
			if (IsDVD) DoSort(an);

			// Enable the appropriate buttons
			if (i > 0) {
				SendMessage(hwndList, LB_SETCURSEL, 0, 0);
				if (i > 1) {
					EnableWindow(hwndMoveUp, TRUE);
					EnableWindow(hwndMoveDown, TRUE);
					EnableWindow(hwndSort, TRUE);
				}
			} else {
				SendMessage(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
				EnableWindow(hwndRemove, FALSE);
			}
		}
		return TRUE;


	case WM_COMMAND:
		// Note: "IDCANCEL" is the [X] button
		// Return TRUE if we process the message

		switch (LOWORD(wParam)) {

			case IDCANCEL:
				fAbort = true;
			case IDOK:
				EndDialog(hDlg, TRUE);
				return TRUE;

			case IDC_ADD:
				DoAdd(an, hDlg);
				return TRUE;

			case IDC_REMOVE:
				DoRemove(an);
				return TRUE;

			case IDC_MOVEUP:
				DoMoveUp(an);
				return TRUE;

			case IDC_MOVEDOWN:
				DoMoveDown(an);
				return TRUE;

			case IDC_SORT:
				DoSort(an);
				return TRUE;
		}
		break;

    }

    return FALSE;
}


// Returns the number of files actually opened

void AppendDlg(AppendNames *an, inFile *pIn, bool bShowUI, HMODULE hModule, HWND hwndParent) {
	LPWSTR orig = NULL;
	int i;

	if (an->num_files < 1) goto Abort;
	if (an->fName[0] == NULL) goto Abort;

	// We need to keep a copy of this, in case of Abort
	orig = copy_string(an->fName[0]);

	// Initialize szInitDir based on the filename path
	if (orig != NULL) {
		int last = 0;		// last path divider found
		int count = 0;		// count of path dividers

		for (i = 0; ; ++i) {
			WCHAR wc = orig[i];
			if (wc == 0) break;
			if (wc == L'\\' || wc == L'/') {
				last = i;
				++count;
			}
			szInitDir[i] = orig[i];
		}
		if (count == 1) ++last;
		szInitDir[last] = 0;
	}

	fAbort = false;

	if (bShowUI) {
		// Pop up the dialog
		DialogBoxParam(hModule, MAKEINTRESOURCE(IDD_APPEND),
			(HWND)hwndParent, AppendDlgProc, (LPARAM)an);
	} else {
		// Automated; no UI
		if (an->num_files == 1) {
			AutoLoadVOBs(an);
		}
	}

	if (!fAbort) {
		if (an->num_files <= 0) goto Abort;

		if (pIn->inOpen(an->fName[0])) {
			for (i = 1; i < an->num_files; ++i) {
				if (!pIn->inAppend(an->fName[i])) break;
			}
			an->num_files = i;
			goto Abort;
		}
	}

	// undo everything, and try to fall back on orig
	for (i = MAX_FILES - 1; i >= 0; --i) {
		delete[] an->fName[i];
		an->fName[i] = NULL;
	}
	an->num_files = 0;

	if (orig != NULL) {
		if (pIn->inOpen(orig)) {
			an->fName[0] = orig;
			an->num_files = 1;
			return;
		}
	}

Abort:
	delete[] orig;
}

