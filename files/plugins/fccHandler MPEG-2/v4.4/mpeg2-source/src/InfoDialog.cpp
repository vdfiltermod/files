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

// File Information dialog (part of InputFileMPEG2 class)

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
	#define _CRT_SECURE_NO_WARNINGS
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>

#include "vd2/plugin/vdinputdriver.h"
#include "Unknown.h"

#include "infile.h"
#include "append.h"
#include "InputFileMPEG2.h"
#include "InputOptionsMPEG2.h"
#include "audio.h"

#include "resource.h"

//#include <crtdbg.h>


static char *time_to_string(double seconds)
{
	// There are 86400 seconds in a day,
	// about 32 million seconds in a year.
	// Since we are unlikely to encounter videos
	// of such an extreme duration, we can easily
	// get away with using ints for this math...

	const int buf_size = 32;
	char *ret = new char[buf_size];

	if (ret != NULL)
	{
		int hours, mins, mils;
		int secs = (int)seconds;

		hours = secs / 3600;
		secs  = secs % 3600;
		mins  = secs / 60;
		secs  = secs % 60;
		mils  = (int)(seconds * 1000.0 + 0.5);
		mils  = mils % 1000;

		if (hours > 0)
		{
			_snprintf(ret, buf_size, "%i:%02i:%02i.%03i", hours, mins, secs, mils);
		}
		else
		{
			_snprintf(ret, buf_size, "%i:%02i.%03i", mins, secs, mils);
		}

		ret[buf_size - 1] = '\0';
	}

	return ret;
}


void InputFileMPEG2::UpdateDlgAudio(HWND hDlg, unsigned int index)
{
	static const char *szModes[6] = {
		"mono",
		"stereo",
		"3 channels",
		"4 channels",
		"5 channels",
		"5.1"
	};

	static const char *szLayers[4] = {
		"?",
		"I",
		"II",
		"III"
	};

	struct {
		__int64         i64AudioSize;
		unsigned int    uiFrameMinSize;
		unsigned int    uiFrameMaxSize;
		__int64         i64FrameTotalSize;
	} pInfo = { 0 };

	char buf[128];
	char *s;
	unsigned int i;
	int id, skew;
	unsigned int count;
	unsigned short header;
	double dur;

	InputFileMPEG2 *thisPtr =
		(InputFileMPEG2 *)GetWindowLongPtr(hDlg, DWLP_USER);

	if (index >= (unsigned int)96) return;

/***************************************************************************
 *      Stream number x of x
 ***************************************************************************/

	count = index;
	for (i = 0; i < 96; ++i) {
		id = thisPtr->audio_stream_list[i];
		if (id > 0) {
			if (count == 0) break;
			--count;
		}
	}

	if (id <= 0 || id > 48) return;
	--id;

	if (thisPtr->aframes[id] <= 0
		|| thisPtr->audio_sample_list[id] == NULL) return;

	count = 0;
	for (i = 0; i < 96; ++i) {
		if (thisPtr->audio_stream_list[i]) ++count;
	}

	_snprintf(buf, sizeof(buf),
		"Stream %u of %u", index + 1, count);

	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemText(hDlg, IDC_STREAMNUM, buf);

/***************************************************************************
 *      Pre-calculate Min/max/total frame sizes
 ***************************************************************************/

	if (thisPtr->aframes[id] != 0)
		pInfo.uiFrameMinSize = thisPtr->audio_sample_list[id][0].size;

	for (i = 0; i < thisPtr->aframes[id]; ++i)
	{
		unsigned int sz = thisPtr->audio_sample_list[id][i].size;

		if (sz < pInfo.uiFrameMinSize) pInfo.uiFrameMinSize = sz;
		if (sz > pInfo.uiFrameMaxSize) pInfo.uiFrameMaxSize = sz;

		pInfo.i64AudioSize += sz;
	}

/***************************************************************************
 *      Format, bitrate, type:
 ***************************************************************************/

	header = thisPtr->audio_sample_list[id][0].header;

	if (id < 32)
	{
		// MPEG audio

		MPEGAudioCompacted hdr(header);
		__int64 iTotalBitrate = 0;

		for (i = 0; i < thisPtr->aframes[id]; ++i)
		{
			iTotalBitrate += MPEGAudioCompacted(
				thisPtr->audio_sample_list[id][i].header).GetBitrateKbps();
		}

		iTotalBitrate = (__int64)
			((double)iTotalBitrate / (double)thisPtr->aframes[id] + 0.5);

		_snprintf(buf, sizeof(buf),
			"%u Hz %s, %I64iKbps, MPEG-%s Layer %s",
			hdr.GetSamplingRateHz(),
			szModes[hdr.IsStereo()? 1: 0],
			iTotalBitrate,
			hdr.IsMPEG25()? "2.5": (hdr.IsMPEG2()? "2": "1"),
			szLayers[hdr.GetLayer()]
		);

		dur = (hdr.GetLayer() == 1)? 384.0: 1152.0;
		dur = (dur * thisPtr->aframes[id]) / hdr.GetSamplingRateHz();
	}
	else if (id < 40)
	{
		// Dolby Digital audio

		int srate, brate, chans;

		if (ac3_compact_info(header, &srate, &brate, &chans))
		{
			_snprintf(buf, sizeof(buf),
				"%i Hz %s, %iKbps, Dolby Digital",
				srate, szModes[chans - 1], brate
			);
			dur = (1536.0 * thisPtr->aframes[id]) / srate;
		}
		else
		{
			buf[0] = '\0';
			dur = 0.0;
		}

	}
	else
	{
		// Linear PCM audio

		int srate = (header & 0x30)? 96000: 48000;
		int chans = (header & 7)? 2: 1;

		_snprintf(buf, sizeof(buf),
			"%i Hz %s, %luKbps, Linear PCM",
			srate, szModes[chans - 1],
			(unsigned long)((2.0 * srate * chans) / 125.0 + 0.5)
		);

		dur = (double)pInfo.i64AudioSize / (2.0 * srate);
    }

	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemText(hDlg, IDC_AUDIO_FORMAT, buf);

/***************************************************************************
 *      Number of frames (duration):
 ***************************************************************************/

	s = time_to_string(dur);
	if (s != NULL)
	{
		_snprintf(buf, sizeof(buf), "%i (%s)", thisPtr->aframes[id], s);
		delete[] s;
	}
	else
	{
		_snprintf(buf, sizeof(buf), "%i (0:00.000)", thisPtr->aframes[id]);
	}

	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemText(hDlg, IDC_AUDIO_NUMFRAMES, buf);

/***************************************************************************
 *      Min/avg/max(total) frame size:
 ***************************************************************************/

	if (thisPtr->aframes[id] != 0)
	{
		_snprintf(buf, sizeof(buf), "%lu / %lu / %lu (%I64i)",
			pInfo.uiFrameMinSize,
			(unsigned long)(((double)pInfo.i64AudioSize / (double)thisPtr->aframes[id]) + 0.5),
			pInfo.uiFrameMaxSize,
			pInfo.i64AudioSize
		);
	}
	else
	{
		_snprintf(buf, sizeof(buf), "0 / 0 / 0 (0)");
	}

	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemText(hDlg, IDC_AUDIO_SIZE, buf);

/***************************************************************************
 *      Detected skew (internal):
 ***************************************************************************/

	// Calculate internal skew with proper rounding
	skew = (thisPtr->afirstPTS[id] < thisPtr->vfirstPTS)? -1: 1;
	skew = (int)(((thisPtr->afirstPTS[id] - thisPtr->vfirstPTS) + 45 * skew) / 90);

	_snprintf(buf, sizeof(buf), "%i ms", skew);
	buf[sizeof(buf) - 1] = '\0';

	SetDlgItemText(hDlg, IDC_AUDIO_SKEW, buf);
}


void InputFileMPEG2::UpdateDlgVideo(HWND hDlg, unsigned int id)
{
	static const char *sz_aspect_ratios[32] = {
		// MPEG-1 SAR
		"SAR = 0 (forbidden)",
		"SAR = 1 (1.0) 1:1 VGA",
		"SAR = 2 (0.6735)",
		"SAR = 3 (0.7031) 16:9 PAL",
		"SAR = 4 (0.7615)",
		"SAR = 5 (0.8055)",
		"SAR = 6 (0.8437) 16:9 NTSC",
		"SAR = 7 (0.8935)",
		"SAR = 8 (0.9157) 4:3 PAL",
		"SAR = 9 (0.9815)",
		"SAR = 10 (1.0255)",
		"SAR = 11 (1.0695)",
		"SAR = 12 (1.095) 4:3 NTSC",
		"SAR = 13 (1.1575)",
		"SAR = 14 (1.2015)",
		"SAR = 15 (reserved)",
		// MPEG-2 SAR/DAR
		"DAR = 0 (forbidden)",
		"SAR = 1 (1.0)",
		"DAR = 2 (4:3)",
		"DAR = 3 (16:9)",
		"DAR = 4 (2.21:1)",
		"DAR = 5 (reserved)",
		"DAR = 6 (reserved)",
		"DAR = 7 (reserved)",
		"DAR = 8 (reserved)",
		"DAR = 9 (reserved)",
		"DAR = 10 (reserved)",
		"DAR = 11 (reserved)",
		"DAR = 12 (reserved)",
		"DAR = 13 (reserved)",
		"DAR = 14 (reserved)",
		"DAR = 15 (reserved)"
	};

    static const UINT uiCtlIds[3] = {
		IDC_VIDEO_IFRAMES,
		IDC_VIDEO_PFRAMES,
		IDC_VIDEO_BFRAMES
	};

	static const char *sz_chroma_formats[4] = {
		"?",
		"4:2:0",
		"4:2:2",
		"4:4:4"
	};

	static const char *sz_matrix_coefficients[8] = {
		"Forbidden",        // (forbidden)
		"Rec. 709",         // ITU-R Recommendation 709 (1990)
		"Unspecified",      // Unspecified Video
		"Reserved",         // reserved
		"FCC",              // FCC
		"CCIR 601",         // ITU-R Recommendation 624-4 System B, G
		"SMPTE 170M",       // SMPTE 170M
		"SMPTE 240M"        // SMPTE 240M (1987)
	};

	char buf[128];
	char *s;
	int i;
	int mc;
	double fps;
	const InputFileMPEG2 *thisPtr;

	struct {
		__int64         i64VideoSize;
		unsigned int    uiFrameCnt[3];
		unsigned int    uiFrameMinSize[3];
		unsigned int    uiFrameMaxSize[3];
		__int64         i64FrameTotalSize[3];
	} pInfo = { 0 };


	thisPtr = (InputFileMPEG2 *)GetWindowLongPtr(hDlg, DWLP_USER);

/***************************************************************************
 *      Title bar
 ***************************************************************************/

	if (thisPtr->system != 0)
	{
		i = _snprintf(buf, sizeof(buf),
			"MPEG-%i System Information", thisPtr->system);
	}
	else
	{
		i = _snprintf(buf, sizeof(buf),
			"MPEG-%i Video Information", (thisPtr->seq_ext != 0)? 2: 1);
	}

	if (i > 0 && i < sizeof(buf) && thisPtr->mNumFiles != 1)
	{
		_snprintf(buf + i, sizeof(buf) - i,
			" (%i files)", thisPtr->mNumFiles);
	}

	buf[sizeof(buf) - 1] = '\0';				
	InputOptionsMPEG2::InitDialogTitle(hDlg, buf);

/***************************************************************************
 *      Frame size, frames per second:
 ***************************************************************************/

	fps = (double)thisPtr->mFrameRate.mNumerator /
	      (double)thisPtr->mFrameRate.mDenominator;

	_snprintf(buf, sizeof(buf), "%u x %u, %.3f fps",
		thisPtr->width, thisPtr->height, fps);

	buf[sizeof(buf) - 1] = '\0';				
	SetDlgItemTextA(hDlg, IDC_VIDEO_FORMAT, buf);

/***************************************************************************
 *      Number of frames (duration):
 ***************************************************************************/

	// Duration
	s = time_to_string((double)thisPtr->fields / fps);
	if (s != NULL)
	{
		_snprintf(buf, sizeof(buf), "%u (%s)", thisPtr->fields, s);
		delete[] s;
	}
	else
	{
		_snprintf(buf, sizeof(buf), "%u (?)", thisPtr->fields);
	}

	buf[sizeof(buf) - 1] = '\0';				
	SetDlgItemTextA(hDlg, IDC_VIDEO_NUMFRAMES, buf);

/***************************************************************************
 *      Video version, format:
 ***************************************************************************/

	if (thisPtr->seq_ext != 0)
	{
		// MPEG-2
		mc = (thisPtr->seq_ext >> 9 ) & 3;
		i  = (thisPtr->seq_ext >> 11) & 1;
	}
	else
	{
		// MPEG-1 (CHROMA420, progressive_sequence)
		mc = 1;
		i  = 1;
	}
	
	_snprintf(buf, sizeof(buf), "MPEG-%c, %s %s",
		(thisPtr->seq_ext != 0)? '2': '1',
		sz_chroma_formats[mc],
		(i != 0)? "progressive": "interlaced");

	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemTextA(hDlg, IDC_CHROMA_FORMAT, buf);

/***************************************************************************
 *      Aspect ratio information:
 ***************************************************************************/

	_snprintf(buf, sizeof(buf), "%s",
		sz_aspect_ratios[thisPtr->aspect_ratio & 31]);

	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemTextA(hDlg, IDC_ASPECT_RATIO, buf);

/***************************************************************************
 *      Display width, display height:
 ***************************************************************************/

	_snprintf(buf, sizeof(buf), "%u x %u",
		thisPtr->display_width, thisPtr->display_height);

	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemTextA(hDlg, IDC_DISPLAY_WH, buf);

/***************************************************************************
 *      Detected colorspace:
 ***************************************************************************/

	// "seq_ext" is a bswapped portion of
	// the beginning of sequence_extension:
	// ........ ........ ........ 0001....	sequence_extension ID
	// ........ ........ ........ ....x...	escape bit
	// ........ ........ ........ .....xxx	profile
	// ........ ........ xxxx.... ........	level
	// ........ ........ ....x... ........	progressive_sequence
	// ........ ........ .....xx. ........	chroma_format
	// ........ x....... .......x ........	horizontal_size_ext
	// ........ .xx..... ........ ........	vertical_size_ext

	if (thisPtr->seq_ext != 0)
	{
		// MPEG-2
		mc = thisPtr->matrix_coefficients & 7;
	}
	else
	{
		// MPEG-1
		mc = 5;
	}
	
	_snprintf(buf, sizeof(buf), "%s", sz_matrix_coefficients[mc]);

	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemTextA(hDlg, IDC_COLORSPACE, buf);

/***************************************************************************
 *      Number of I, P, and B frames:
 ***************************************************************************/

	for (i = 0; i < 3; ++i)
	{
		pInfo.uiFrameMinSize[i] = 0xFFFFFFFF;
	}

	for (unsigned int f = 0; f < thisPtr->vframes; ++f)
	{
		const MPEGSampleInfo& msi = thisPtr->video_sample_list[f];
		int iFrameType = msi.frame_type & 3;

		if (iFrameType)
		{
			unsigned int uiSize = msi.size;
			--iFrameType;

			++pInfo.uiFrameCnt[iFrameType];
			pInfo.i64FrameTotalSize[iFrameType] += uiSize;

			if (uiSize < pInfo.uiFrameMinSize[iFrameType])
				pInfo.uiFrameMinSize[iFrameType] = uiSize;

			if (uiSize > pInfo.uiFrameMaxSize[iFrameType])
				pInfo.uiFrameMaxSize[iFrameType] = uiSize;

			pInfo.i64VideoSize += uiSize;
		}
	}

	_snprintf(buf, sizeof(buf),
		"%u / %u / %u", pInfo.uiFrameCnt[0],
		pInfo.uiFrameCnt[1], pInfo.uiFrameCnt[2]);

	buf[sizeof(buf) - 1] = '\0';				
    SetDlgItemTextA(hDlg, IDC_VIDEO_FRAMETYPECNT, buf);

/***************************************************************************
 *      I-frame min/avg/max/total frame size:
 *      P-frame min/avg/max/total frame size:
 *      B-frame min/avg/max/total frame size:
 ***************************************************************************/

	for (i = 0; i < 3; ++i)
	{
		if (pInfo.uiFrameCnt[i])
		{
			_snprintf(buf, sizeof(buf),
				"%u / %u / %u (%I64i)",
				pInfo.uiFrameMinSize[i],
				(unsigned int)((double)pInfo.i64FrameTotalSize[i] / (double)pInfo.uiFrameCnt[i] + 0.5),
				pInfo.uiFrameMaxSize[i],
				pInfo.i64FrameTotalSize[i]
			);
		}
		else
		{
			_snprintf(buf, sizeof(buf),
				"(no %c-frames)", "IPB"[i]);
		}

		buf[sizeof(buf) - 1] = '\0';				
		SetDlgItemTextA(hDlg, uiCtlIds[i], buf);
	}

/***************************************************************************
 *      Average bitrate:
 ***************************************************************************/

	// This won't work for movies with RFF, because we don't know
	// the display rate of the encoded frames (it may even vary).
	// However, we do know the playback duration, so divide that
	// by the number of coded frames to get an average frame rate.

	// bits * (frames per sec) / frames = bits per sec
	// (a / b) / c  =  a / (b * c)
	// a / (b / c)  =  (a * c) / b

	// First, calculate the AVERAGE frame rate:

	// calculate the duration in seconds:
	fps = (double)thisPtr->fields / fps;

	// divide the total size of the video stream by that
	// number to produce the rate in bytes per second:
	fps = (double)pInfo.i64VideoSize / fps;

	_snprintf(buf, sizeof(buf),
		"%lu bits/sec",
		(unsigned long)(8.0 * fps + 0.5)
	);

	buf[sizeof(buf) - 1] = '\0';				
	SetDlgItemTextA(hDlg, IDC_VIDEO_AVGBITRATE, buf);
}


INT_PTR CALLBACK InputFileMPEG2::InfoDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	InputFileMPEG2 *thisPtr;
	int i, count;
	unsigned int id;

	switch (message) {

    case WM_INITDIALOG:
		SetWindowLongPtr(hDlg, DWLP_USER, lParam);
		thisPtr = (InputFileMPEG2 *)lParam;

	    if (thisPtr->vframes > 0)
	    {
			UpdateDlgVideo(hDlg, 0);
	    }

		/////////// Audio ///////////////////////////////////////

		for (i = 0; i < 96; ++i) {
			id = thisPtr->audio_stream_list[i] - 1;
			if (id < 48U) break;
		}
		if (i == 96) return TRUE;

		count = 0;
		for (i = 0; i < 96; ++i) {
			if (thisPtr->audio_stream_list[i]) ++count;
		}

		if (count > 1) {
			HWND hwndSpin = GetDlgItem(hDlg, IDC_SPIN1);
			EnableWindow(hwndSpin, TRUE);
			SendMessageA(hwndSpin, UDM_SETRANGE, 0,
				(LPARAM)MAKELONG((count - 1), 0));
			SendMessageA(hwndSpin, UDM_SETPOS, 0,
				(LPARAM)MAKELONG(0, 0));
			ShowWindow(hwndSpin, SW_SHOW);
		}

		UpdateDlgAudio(hDlg, 0);
		return TRUE;


	case WM_COMMAND:
		// Note: "IDCANCEL" is the [X] button
		// Return TRUE if we process the message

		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, TRUE);
			return TRUE;
		}
		break;

	case WM_NOTIFY:
		{
			const NMUPDOWN *nmud = (NMUPDOWN *)lParam;

			if (nmud->hdr.idFrom == IDC_SPIN1 &&
				nmud->hdr.code == UDN_DELTAPOS)
			{
				UpdateDlgAudio(hDlg, nmud->iPos + nmud->iDelta);
			}
		}
		break;

    }

    return FALSE;
}

