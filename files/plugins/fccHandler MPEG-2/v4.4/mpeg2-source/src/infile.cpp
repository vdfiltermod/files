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

// Windows file I/O functions, wrapped in a class
// Files larger than 4GB are supported on NTFS.

// This version adds the ability to virtually join files

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "infile.h"

#define INPAGE(x)		(x & (~(BUF_SIZE-1)))
#define INOFFSET(x)		(x & (BUF_SIZE-1))


char* inFile::wchar_to_ansi(const wchar_t *szWchar, int len)
{
	char *tmp = NULL;
	if (szWchar != NULL)
	{
		int lenA = WideCharToMultiByte(CP_ACP, 0, szWchar, len, NULL, 0, NULL, NULL);
		if (lenA > 0)
		{
			tmp = new char[lenA];
			if (tmp != NULL)
			{
				if (WideCharToMultiByte(CP_ACP, 0, szWchar, len, tmp, lenA, NULL, NULL) != lenA)
				{
					delete[] tmp;
					tmp = NULL;
				}
			}
		}
	}
	return tmp;
}


inFile::inFile()
{
	pFileName = NULL;
	for (int i = 0; i < MAX_FILES; ++i)
	{
		hFile[i] = INVALID_HANDLE_VALUE;
		m_Size[i].QuadPart = 0;
	}
	m_Pos.QuadPart = 0;
	m_Total.QuadPart = 0;
	memset(bBuff, 0, BUF_SIZE);
	bDirty = false;
}


inFile::~inFile()
{
	if (bDirty) outFlush();
	for (int i = MAX_FILES - 1; i >= 0; --i)
	{
		if (hFile[i] != INVALID_HANDLE_VALUE) CloseHandle(hFile[i]);
	}
}


/*
	How Windows handles some aberrant situations:

	Setting the file pointer to absolute -10 returns 0xFFFFFFFF
	and error code 87 (ERROR_INVALID_PARAMETER), and does
	not move the file pointer (apparently)

	Setting the file pointer to relative -10 returns 0xFFFFFFFF
	and error code 131 (ERROR_NEGATIVE_SEEK), and does
	not move the file pointer

	Setting the file pointer AT or BEYOND the end of file succeeds.

	Attempting to read past the end of file does not cause
	an error, but reads zero bytes.
*/

void inFile::inRefresh()
{
	int i;
	LARGE_INTEGER pos;

	unsigned char *dst = bBuff;
	DWORD remains = BUF_SIZE;

	pos.QuadPart = INPAGE(m_Pos.QuadPart);

	for (i = 0; i < MAX_FILES; ++i)
	{
		if (hFile[i] == INVALID_HANDLE_VALUE) break;

		if (pos.QuadPart < m_Size[i].QuadPart)
		{
			DWORD dwRead;
			LONG dwHi = pos.HighPart;
			DWORD dwLo = pos.LowPart;

			for (;;)
			{
				SetFilePointer(hFile[i], dwLo, &dwHi, FILE_BEGIN);

				if (ReadFile(hFile[i], dst, remains, &dwRead, NULL) == 0)
					break;

				if (dwRead == 0 || dwRead >= remains) return;

				remains -= dwRead;
				dst += dwRead;

				if (++i >= MAX_FILES) break;
				if (hFile[i] == INVALID_HANDLE_VALUE) break;

				dwLo = 0;
				dwHi = 0;
			}

			break;
		}

		pos.QuadPart -= m_Size[i].QuadPart;
	}

	if (remains > 0) memset(dst, 0, remains);
}


HANDLE inFile::dualCreateFile(LPCWSTR lpFileName, DWORD dwDesiredAccess,
	DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile)
{
	HANDLE h;

	if (GetVersion() < 0x80000000U)
	{
		return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
			lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile);
	}

	char *tmp = wchar_to_ansi(lpFileName, -1);

	if (tmp != NULL)
	{
		h = CreateFileA(tmp, dwDesiredAccess, dwShareMode,
			lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile);

		delete[] tmp;
		return h;
	}

	return INVALID_HANDLE_VALUE;
}


bool inFile::inOpen(const wchar_t *szFile)
{
	DWORD dwLo, dwHi;
	int len;
	LPWSTR pName;
	HANDLE h;
	int i;

	bool ret = false;

	if (szFile == NULL) goto Abort;
	len = lstrlenW(szFile);
	if (len <= 0) goto Abort;
	pName = new WCHAR[len + 1];
	if (pName == NULL) goto Abort;

	h = dualCreateFile((LPWSTR)szFile, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (h == INVALID_HANDLE_VALUE) goto Abort;

	dwLo = GetFileSize(h, &dwHi);
	if (dwLo == 0xFFFFFFFF && GetLastError() != NO_ERROR) goto Abort2;
	if (dwHi & 0x80000000U) goto Abort2;

	for (i = MAX_FILES - 1; i >= 0; --i)
	{
		if (hFile[i] != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile[i]);
			hFile[i] = INVALID_HANDLE_VALUE;
		}
		m_Size[i].QuadPart = 0;
	}
	delete[] pFileName;

	pFileName = pName;
	memcpy(pFileName, szFile, (len + 1) * sizeof(wchar_t));
	hFile[0] = h;
	m_Pos.QuadPart = 0;
	m_Size[0].LowPart = dwLo;
	m_Size[0].HighPart = (LONG)dwHi;
	m_Total.QuadPart = m_Size[0].QuadPart;

	inRefresh();
	ret = true;

Abort:
    return ret;

Abort2:
	CloseHandle(h);
	goto Abort;
}


bool inFile::inAppend(const wchar_t *szFile)
{
	DWORD dwLo, dwHi;
	HANDLE h;
	int i;

	bool ret = false;

	for (i = 0; i < MAX_FILES; ++i)
	{
		if (hFile[i] == INVALID_HANDLE_VALUE) break;
	}
	if (i >= MAX_FILES) goto Abort;
	if (i == 0) return inOpen(szFile);

	if (szFile == NULL) goto Abort;
	if (*szFile == 0) goto Abort;

	h = dualCreateFile((LPWSTR)szFile, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (h == INVALID_HANDLE_VALUE) goto Abort;

	dwLo = GetFileSize(h, &dwHi);
	if (dwLo == 0xFFFFFFFF && GetLastError() != NO_ERROR) goto Abort2;
	if (dwHi & 0x80000000U) goto Abort2;

	hFile[i] = h;
	m_Size[i].LowPart = dwLo;
	m_Size[i].HighPart = (LONG)dwHi;
	m_Total.QuadPart += m_Size[i].QuadPart;

	inRefresh();
	ret = true;

Abort:
    return ret;

Abort2:
	CloseHandle(h);
	goto Abort;
}


__int64 inFile::inSize() const {
	if (hFile[0] != INVALID_HANDLE_VALUE) return m_Total.QuadPart;
	return 0;
}


__int64 inFile::inPos() const {
	if (hFile[0] != INVALID_HANDLE_VALUE) return m_Pos.QuadPart;
	return 0;
}


bool inFile::inEOF() const {
	if (hFile[0] != INVALID_HANDLE_VALUE) return (m_Pos.QuadPart >= m_Total.QuadPart);
	return true;
}


bool inFile::inSeek(__int64 iAbsPos)
{
	bool ret = false;

	if ((hFile[0] != INVALID_HANDLE_VALUE) && iAbsPos >= 0)
	{
		ret = (INPAGE(iAbsPos) != INPAGE(m_Pos.QuadPart));
		m_Pos.QuadPart = iAbsPos;

		if (ret) inRefresh();
		ret = true;
	}

	return ret;
}


long inFile::inRead(void *lpBuffer, long lBytes)
{
	int remains, offs;
	unsigned char *dst;

	long ret = 0;

	if (hFile[0] == INVALID_HANDLE_VALUE) goto Abort;
	if (lpBuffer == NULL) goto Abort;
	if (lBytes <= 0) goto Abort;

	// Don't read past EOF
	{
		__int64 i = m_Total.QuadPart - m_Pos.QuadPart;
		if (i <= 0) goto Abort;
		if ((__int64)lBytes > i) lBytes = (long)i;
	}

	dst = (unsigned char *)lpBuffer;

	offs = INOFFSET(m_Pos.LowPart);
	remains = BUF_SIZE - offs;

	for (;;)
	{
		if (remains > lBytes) remains = lBytes;
		memcpy(dst, bBuff + offs, remains);
		ret += remains;
		m_Pos.QuadPart += remains;
		lBytes -= remains;
		if (lBytes <= 0) break;
		dst += remains;
		inRefresh();
		offs = 0;
		remains = BUF_SIZE;
	}

	if (INOFFSET(m_Pos.LowPart) == 0) inRefresh();

Abort:
	return ret;
}


int inFile::inByte()
{
	int ret = 0;

	if (hFile[0] != INVALID_HANDLE_VALUE)
	{
		if (m_Pos.QuadPart < m_Total.QuadPart)
		{
			int offs = INOFFSET(m_Pos.LowPart);
			ret = bBuff[offs];
			++m_Pos.QuadPart;
			if (offs == (BUF_SIZE - 1)) inRefresh();
		}
	}

	return ret;
}


//////////////////////////////////////////////////////////////////////
//
//
//                     File output functions
//
//
//////////////////////////////////////////////////////////////////////

void inFile::outFlush()
{
	// Flush whatever remains in the buffer,
	// and update the file size.

	__int64 remains = m_Pos.QuadPart - m_Size[0].QuadPart;
	if (remains > 0)
	{
		DWORD dwWritten;

		if (remains > BUF_SIZE) remains = BUF_SIZE;

		WriteFile(hFile[0], bBuff, (DWORD)remains, &dwWritten, NULL);

		m_Size[0].QuadPart += remains;
		bDirty = false;
	}
}


bool inFile::outOpen(const wchar_t *szFile)
{
	HANDLE h;
	int len;
	LPWSTR pName;
	int i;

	bool ret = false;

	if (szFile == NULL) goto Abort;
	len = lstrlenW(szFile);
	if (len <= 0) goto Abort;
	pName = new WCHAR[len + 1];
	if (pName == NULL) goto Abort;

	h = dualCreateFile((LPCWSTR)szFile, GENERIC_WRITE, FILE_SHARE_READ, NULL,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (h == INVALID_HANDLE_VALUE) goto Abort;
	SetEndOfFile(h);

	for (i = MAX_FILES - 1; i >= 0; --i)
	{
		if (hFile[i] != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile[i]);
			hFile[i] = INVALID_HANDLE_VALUE;
		}
		m_Size[i].QuadPart = 0;
	}
	delete[] pFileName;

	pFileName = pName;
	memcpy(pFileName, szFile, (len + 1) * sizeof(wchar_t));

	hFile[0] = h;
	m_Pos.QuadPart = 0;
	m_Size[0].QuadPart = 0;
	m_Total.QuadPart = 0;
	bDirty = false;

	ret = true;

Abort:
    return ret;
}


long inFile::outWrite(const void *lpBuffer, long lBytes)
{
	DWORD remains, offs;
	long ret = 0;

	if (hFile[0] == INVALID_HANDLE_VALUE) goto Abort;
	if (lpBuffer == NULL) goto Abort;
	if (lBytes <= 0) goto Abort;

	offs = INOFFSET(m_Pos.LowPart);
	remains = BUF_SIZE - offs;

	// "remains" always indicates a write that fills
	// the buffer exactly, therefore if "lBytes" is less
	// than "remains" on any iteration, flag the buffer
	// as dirty and don't flush it yet.

	for (;;)
	{
		bDirty = ((DWORD)lBytes < remains);
		if (bDirty) remains = lBytes;
		memcpy(bBuff + offs, lpBuffer, remains);
		m_Pos.QuadPart += remains;
		if (!bDirty) outFlush();
		lBytes -= remains;
		ret += remains;
		if (lBytes <= 0) break;
		lpBuffer = (char *)lpBuffer + remains;
		offs = 0;
		remains = BUF_SIZE;
	}

Abort:
	return ret;
}

