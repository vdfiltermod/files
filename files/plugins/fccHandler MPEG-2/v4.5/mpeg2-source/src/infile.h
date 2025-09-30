#ifndef _INFILE_H
#define _INFILE_H

#define MAX_FILES 16

class inFile {

private:
	enum { BUF_SIZE  = 65536 };

	BYTE	bBuff[BUF_SIZE];
	LPWSTR  pFileName;

	LARGE_INTEGER m_Pos;
	LARGE_INTEGER m_Total;
	LARGE_INTEGER m_Size[MAX_FILES];

	HANDLE	hFile[MAX_FILES];
	bool	bDirty;

	void    inRefresh();
	void    outFlush();

public:
	inFile::inFile();
	inFile::~inFile();

	static  char *wchar_to_ansi(const wchar_t *szWchar, int len);

	static  HANDLE dualCreateFile(LPCWSTR lpFileName, DWORD dwDesiredAccess,
		DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		HANDLE hTemplateFile);

	bool    inOpen(const wchar_t *szFile);
	bool    inAppend(const wchar_t *szFile);

	__int64 inSize() const;
	__int64 inPos() const;
	bool    inEOF() const;

	LPCWSTR FileName() const { return pFileName; }

	bool    inSeek(__int64 iAbsPos);

	long    inRead(void *lpBuffer, long lBytes);
	int     inByte();

	bool    outOpen(const wchar_t *szFile);
	long    outWrite(const void *lpBuffer, long lBytes);
};

#endif	// _INFILE_H
