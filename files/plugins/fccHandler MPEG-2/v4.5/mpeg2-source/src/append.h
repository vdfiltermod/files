#ifndef f_APPEND_H
#define f_APPEND_H

class AppendNames {
public:
	LPWSTR fName[MAX_FILES];
	int num_files;

	AppendNames::AppendNames() {
		num_files = 0;
		for (int i = 0; i < MAX_FILES; ++i) fName[i] = NULL;
	}

	AppendNames::~AppendNames() {
		for (int i = MAX_FILES - 1; i >= 0; --i) delete[] fName[i];
	}
};

wchar_t *copy_string(const wchar_t *szString);
void AppendDlg(AppendNames *an, inFile *pIn, bool bShowUI, HMODULE hModule, HWND hwndParent);

#endif	// f_APPEND_H