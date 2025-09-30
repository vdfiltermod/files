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

// Read/Write index (.midx) files

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "vd2/plugin/vdinputdriver.h"
#include "Unknown.h"

#include "infile.h"
#include "append.h"
#include "InputFileMPEG2.h"

#define VARIABLE_SIZES		1
#define VARIABLE_HEADERS	2
#define VARIABLE_POSITIONS	4
#define LONG_DELTAS			8
#define SHORT_DELTAS		16
#define LONG_SIZES			32
#define SHORT_SIZES			64


LPWSTR InputFileMPEG2::GetIndexName(LPCWSTR szFile) {
	int len = lstrlenW(szFile);
	if (len > 0) {

		LPWSTR ret = new WCHAR[len + 5 + 1];
		if (ret != NULL) {
			memcpy(ret, szFile, (len + 1) * sizeof(WCHAR));

			if (len > 5) {
				const __int64 ext = *(__int64 *)(ret + len - 4);
				if ((ext | 0x0020002000200020) == 0x007800640069006D) {
					if (*(ret + len - 5) == L'.') return ret;
				}
			}

			// Append ".idx" extension
			ret[len + 0] = L'.';
			ret[len + 1] = L'm';
			ret[len + 2] = L'i';
			ret[len + 3] = L'd';
			ret[len + 4] = L'x';
			ret[len + 5] = 0;

			return ret;
		}
	}

	return NULL;
}


int InputFileMPEG2::GetPathLen(LPCWSTR szFile)
{
	// Note: Returned path length includes the final separator
	int len = 0;
	if (szFile != NULL)
	{
		int i;
		for (i = 0; ; )
		{
			WCHAR wc = szFile[i++];
			if (wc == L'\0') break;
			if (wc == L'\\' || wc == L'/') len = i;
		}
	}
	return len;
}


void InputFileMPEG2::ReadPacketInfo19(inFile& idxFile, MPEGPacketInfo *mpi, unsigned long packets) {
	unsigned long delta, i;
	int flags;

	idxFile.inRead(mpi, sizeof(MPEGPacketInfo));
	idxFile.inRead(&flags, sizeof(int));

	for (i = 1; i < packets; ++i) {
		if (flags == SHORT_DELTAS) {
			delta = 0;
			idxFile.inRead(&delta, sizeof(short));
		} else {
			idxFile.inRead(&delta, sizeof(long));
		}
		mpi[i].file_pos = ((mpi[i - 1].file_pos >> 16) + delta) << 16;

		idxFile.inRead(&(mpi[i].file_pos), sizeof(short));
	}
}

void InputFileMPEG2::WritePacketInfo(inFile& idxFile, const MPEGPacketInfo *mpi, unsigned long packets) {
	unsigned long delta, i;
	int flags;

	// Write the initial packet
	idxFile.outWrite(mpi, sizeof(MPEGPacketInfo));

	// If we can use shorts, do so, otherwise use longs
	flags = SHORT_DELTAS;
	for (i = 1; i < packets; ++i) {
		delta = (unsigned long)(
			(mpi[i].file_pos >> 16) - (mpi[i - 1].file_pos >> 16)
		);
		if (delta > 65535) {
			flags = LONG_DELTAS;
			break;
		}
	}

	idxFile.outWrite(&flags, sizeof(int));

	for (i = 1; i < packets; ++i) {
		delta = (unsigned long)(
			(mpi[i].file_pos >> 16) - (mpi[i - 1].file_pos >> 16)
		);
		if (flags == SHORT_DELTAS) {
			idxFile.outWrite(&delta, sizeof(short));
		} else {
			idxFile.outWrite(&delta, sizeof(long));
		}
		delta = (unsigned long)(mpi[i].file_pos & 0xFFFF);
		idxFile.outWrite(&delta, sizeof(short));
	}
}

void InputFileMPEG2::ReadSampleInfo19(inFile& idxFile, MPEGSampleInfo *msi, int samples) {
	for (int i = 0; i < samples; ++i) {
		idxFile.inRead(msi, sizeof(MPEGSampleInfo));
		++msi;
	}
}

void InputFileMPEG2::WriteSampleInfo(inFile& idxFile, const MPEGSampleInfo *msi, int samples) {
	for (int i = 0; i < samples; ++i) {
		idxFile.outWrite(msi, sizeof(MPEGSampleInfo));
		++msi;
	}
}

bool InputFileMPEG2::ReadIndex20(inFile& idxFile, AppendNames *an)
{
	bool fInterleaved;
	int i, nf;

	an->num_files = 0;

	idxFile.inRead(&nf, sizeof(int));
	if (nf <= 0) goto Abort;

	for (i = 0; i < nf; ++i) {
		bool success;
		int len = 0;

		idxFile.inRead(&len, sizeof(int));
		if (len <= 0 || len >= MAX_PATH) break;

		an->fName[i] = new WCHAR[len];
		if (an->fName[i] == NULL) break;

		idxFile.inRead(an->fName[i], len * 2);

		// If the name lacks a path, prepend it
		if (GetPathLen(an->fName[i]) == 0)
		{
			int pathlen = GetPathLen(idxFile.FileName());
			if (pathlen > 0)
			{
				LPWSTR pTemp = new WCHAR[pathlen + len];
				if (pTemp == NULL) break;
				memcpy(pTemp, idxFile.FileName(), pathlen * sizeof(WCHAR));
				memcpy(pTemp + pathlen, an->fName[i], len * sizeof(WCHAR));
				delete[] an->fName[i];
				an->fName[i] = pTemp;
			}
		}

		if (i == 0) {
			success = mFile.inOpen(an->fName[i]);
		} else {
			success = mFile.inAppend(an->fName[i]);
		}

		if (!success) {
			delete[] an->fName[i];
			an->fName[i] = NULL;
			break;
		}

		++an->num_files;
	}

	if (i <= 0) goto Abort;

	// Now allocate the memory resources (we want
	// to give up early if there is a memory problem)

	idxFile.inRead(&i, sizeof(int));
	fInterleaved			= ((i & 1) != 0);
	progressive_sequence	= ((i & 2) != 0);

	idxFile.inRead(&vframes, sizeof(int));
	if (vframes <= 0) goto Abort;
	idxFile.inRead(&vpackets, sizeof(long));
	if (vpackets == 0) goto Abort;

	if (fInterleaved) {
		idxFile.inRead(audio_stream_list, 96);
		for (i = 0; i < 96; ++i) {
			unsigned int id = audio_stream_list[i] - 1;
			if (id < (unsigned int)48) {
				idxFile.inRead(&(apackets[id]), sizeof(long));
				idxFile.inRead(&(aframes[id]), sizeof(int));
				idxFile.inRead(&(afirstPTS[id]), sizeof(__int64));
				if (aframes[id] > 0 && apackets[id] <= 0) goto Abort;
			}
		}
	}

	// Allocate the memory
	video_packet_list = (MPEGPacketInfo *)(new char[vpackets * sizeof(MPEGPacketInfo)]);
	if (video_packet_list == NULL) goto Abort;

	if (fInterleaved) {
		for (i = 0; i < 96; ++i) {
			unsigned int id = audio_stream_list[i] - 1;
			if (id < (unsigned int)48) {
				if (apackets[id] > 0) {
					audio_packet_list[id] = (MPEGPacketInfo *)(new char[apackets[id] * sizeof(MPEGPacketInfo)]);
					if (audio_packet_list[id] == NULL) goto Abort;

					if (aframes[id] > 0) {
						audio_sample_list[id] = (MPEGSampleInfo *)(new char[aframes[id] * sizeof(MPEGSampleInfo)]);
						if (audio_sample_list[id] == NULL) goto Abort;
					}
				}
			}
		}
	}

	video_sample_list = (MPEGSampleInfo *)(new char[vframes * sizeof(MPEGSampleInfo)]);
	if (video_sample_list == NULL) goto Abort;

	// Good to go!  Now we just read in all the information...

	ReadSampleInfo19(idxFile, video_sample_list, vframes);
	ReadPacketInfo19(idxFile, video_packet_list, vpackets);

	if (fInterleaved) {
		for (i = 0; i < 96; ++i) {
			unsigned int id = audio_stream_list[i] - 1;
			if (id < (unsigned int)48) {
				if (apackets[id] > 0) {
					ReadPacketInfo19(idxFile, audio_packet_list[id], apackets[id]);
					if (aframes[id] > 0) {
						ReadSampleInfo19(idxFile, audio_sample_list[id], aframes[id]);
					}
				}
			}
		}
	}

	idxFile.inRead(&mFrameRate, sizeof(VDXFraction));
	idxFile.inRead(&vfirstPTS, sizeof(__int64));

	idxFile.inRead(&width, sizeof(int));
	idxFile.inRead(&height, sizeof(int));
	idxFile.inRead(&aspect_ratio, sizeof(int));

//	idxFile.inRead(&iChosenAudio, sizeof(int));
	idxFile.inRead(&seq_ext, sizeof(long));

	// Legacy version 2.0 did not store matrix_coefficients, so
	// use a default based on the presence of sequence_extension.
	matrix_coefficients = (seq_ext != 0)? 1: 5;

	// Legacy version 2.0 did not store the system version,
	// so take a best guess (not guaranteed to be correct).
	if (fInterleaved)
		system = (seq_ext != 0)? 2: 1;
	else
		system = 0;

	// Legacy version 2.0 did not store display width/height.
	display_width  = width;
	display_height = height;

	return true;

Abort:
	return false;
}


bool InputFileMPEG2::ReadIndex21(inFile& idxFile, AppendNames *an)
{
	if (ReadIndex20(idxFile, an))
	{
		idxFile.inRead(&matrix_coefficients, sizeof(matrix_coefficients));
		idxFile.inRead(&system, sizeof(system));
		idxFile.inRead(&display_width,  sizeof(display_width));
		idxFile.inRead(&display_height, sizeof(display_height));
		return true;
	}
	return false;
}


void InputFileMPEG2::WriteIndex(LPCWSTR szFile, AppendNames *an)
{
	inFile idxFile;
	DWORD buf;
	int i;

	if (an->num_files <= 0) return;

	// Create index file
	{
		bool success;
		LPWSTR idxName = GetIndexName(szFile);
		if (idxName == NULL) goto Abort;
		success = idxFile.outOpen((wchar_t *)idxName);
		delete[] idxName;

		if (!success) goto Abort;
	}

	buf = 0x5844494D;	// 'MIDX'
	idxFile.outWrite(&buf, 4);

	// Due to unfortunate planning on my part, the version
	// is stored as two 16-bit WORDs in Motorola order;
	// first the major version, then the minor version.

	buf = 0x01000200;	// Write version 2.1
	idxFile.outWrite(&buf, 4);

	idxFile.outWrite(&(an->num_files), sizeof(int));
	for (i = 0; i < an->num_files; ++i)
	{
		// Write the filename minus the path
		int pathlen = GetPathLen(an->fName[i]);
		int len = (lstrlenW(an->fName[i]) + 1) - pathlen;
		if (len <= 0 || len >= MAX_PATH) len = 0;
		idxFile.outWrite(&len, sizeof(int));
		if (len > 0) idxFile.outWrite(an->fName[i] + pathlen, len * 2);
	}

	i = 0;
	if (system != 0)			i |= 1;
	if (progressive_sequence)	i |= 2;
	idxFile.outWrite(&i, sizeof(int));

	idxFile.outWrite(&vframes, sizeof(int));
	idxFile.outWrite(&vpackets, sizeof(long));
	if (system != 0) {
		idxFile.outWrite(audio_stream_list, 96);
		for (i = 0; i < 96; ++i) {
			unsigned int id = audio_stream_list[i] - 1;
			if (id < (unsigned int)48) {
				idxFile.outWrite(&(apackets[id]), sizeof(long));
				idxFile.outWrite(&(aframes[id]), sizeof(int));
				idxFile.outWrite(&(afirstPTS[id]), sizeof(__int64));
			}
		}
	}

	// Now write the arrays as applicable
	WriteSampleInfo(idxFile, video_sample_list, vframes);
	WritePacketInfo(idxFile, video_packet_list, vpackets);
	if (system != 0) {
		for (i = 0; i < 96; ++i) {
			unsigned int id = audio_stream_list[i] - 1;
			if (id < (unsigned int)48) {
				if (apackets[id] > 0) {
					WritePacketInfo(idxFile, audio_packet_list[id], apackets[id]);
					if (aframes[id] > 0) {
						WriteSampleInfo(idxFile, audio_sample_list[id], aframes[id]);
					}
				}
			}
		}
	}

	idxFile.outWrite(&mFrameRate, sizeof(VDXFraction));
	idxFile.outWrite(&vfirstPTS, sizeof(__int64));

	idxFile.outWrite(&width, sizeof(int));
	idxFile.outWrite(&height, sizeof(int));
	idxFile.outWrite(&aspect_ratio, sizeof(int));

//	idxFile.outWrite(&iChosenAudio, sizeof(int));
	idxFile.outWrite(&seq_ext, sizeof(long));
	idxFile.outWrite(&matrix_coefficients, sizeof(matrix_coefficients));
	idxFile.outWrite(&system, sizeof(system));
	idxFile.outWrite(&display_width,  sizeof(display_width));
	idxFile.outWrite(&display_height, sizeof(display_height));

Abort:
	return;
}



bool InputFileMPEG2::ReadIndex(LPCWSTR szFile, AppendNames *an) {
	inFile idxFile;

	// Open the index file, if it exists
	LPWSTR idxName = GetIndexName(szFile);
	if (idxName != NULL) {
		bool success = idxFile.inOpen(idxName);
		delete[] idxName;

		if (success) {
			DWORD buf;
			idxFile.inRead(&buf, 4);
			if (buf == 0x5844494D) {	// 'MIDX'
				int i;
				idxFile.inRead(&buf, 4);

				switch (buf) {
					case 0x00000200:	// (Motorola) Version 2.0
						if (ReadIndex20(idxFile, an)) return true;
						break;
					case 0x01000200:	// (Motorola) Version 2.1
						if (ReadIndex21(idxFile, an)) return true;
						break;
				}

				// If ReadIndex## fails, destroy everything
				delete [] video_sample_list;
				video_sample_list = NULL;
				vframes = 0;

				for (i = 47; i >= 0; --i) {
					delete [] audio_sample_list[i];
					audio_sample_list[i] = NULL;
					aframes[i] = 0;

					delete [] audio_packet_list[i];
					audio_packet_list[i] = NULL;
					apackets[i] = 0;
				}

				delete [] video_packet_list;
				video_packet_list = NULL;
				vpackets = 0;
			}
		}
	}

	return false;
}
