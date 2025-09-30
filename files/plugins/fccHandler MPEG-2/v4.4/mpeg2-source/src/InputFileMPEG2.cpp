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

// Main Plugin interfaces

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
#include "DataVector.h"
#include "video.h"
#include "audio.h"
#include "parser.h"

#include "resource.h"

#include <crtdbg.h>


HMODULE g_hModule;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		g_hModule = (HMODULE)hinstDLL;
	}
	return TRUE;
}


/************************************************************************
 *
 *                  InputFileMPEG2
 *
 ************************************************************************/

InputFileMPEG2::InputFileMPEG2(const VDXInputDriverContext& context, uint32 flags)
	: mContext  (context)
	, uiFlags   (flags)
{
_RPT0(_CRT_WARN, "InputFileMPEG2 constructed\n");

	// NULL all pointers before calling Cleanup!
    video_packet_list   = NULL;
	video_sample_list   = NULL;
	video_field_map     = NULL;

	for (int i = 0; i < 48; ++i)
	{
		audio_packet_list[i] = NULL;
		audio_sample_list[i] = NULL;
	}

	Cleanup();
}


InputFileMPEG2::~InputFileMPEG2()
{
_RPT0(_CRT_WARN, "InputFileMPEG2 destructed\n");
	Cleanup();
}


void InputFileMPEG2::Cleanup()
{
	// Clean up everything and restore default values.

	int i;

	delete[] video_field_map;
	video_field_map = NULL;

	delete[] video_sample_list;
	video_sample_list = NULL;

	for (i = 47; i >= 0; --i)
	{
		delete[] audio_sample_list[i];
		audio_sample_list[i] = NULL;

		delete[] audio_packet_list[i];
		audio_packet_list[i] = NULL;

		apackets[i]  = 0;
		afirstPTS[i] = 0;
		aframes[i]   = 0;
	}

    delete[] video_packet_list;
    video_packet_list = NULL;

	fUserAborted            = false;

	vpackets                = 0;
	vfirstPTS               = 0;
	progressive_sequence    = true;
	lFirstCode              = 0;

	memset(audio_stream_list, 0, 96);

#ifndef MULTIAUDIO
	iChosenAudio            = 0;
#endif

	mNumFiles               = 0;
    fAllowDSC               = false;
    vframes                 = 0;
    largest_framesize       = 0;
    width                   = 0;
    height                  = 0;
    mFrameRate.mNumerator   = 1;
    mFrameRate.mDenominator = 1;
    aspect_ratio            = 0;
    seq_ext                 = 0;
    fields                  = 0;
    fAllowMatrix            = false;
    matrix_coefficients     = 5;

	system                  = 0;
}


#ifndef MULTIAUDIO


// Create MPEG audio dialog in memory

//IDD_CHOOSEAUDIO DIALOG DISCARDABLE  0, 0, 256, 96
//STYLE DS_SETFOREGROUND | WS_VISIBLE | WS_CAPTION
//CAPTION "Multiple Audio Streams Found"
//FONT 8, "MS Sans Serif"
//BEGIN
//    CTEXT           "Choose an audio stream:",IDC_STATIC,87,4,82,8
//    LISTBOX         IDC_LIST1,4,16,248,56,LBS_NOINTEGRALHEIGHT | WS_VSCROLL | 
//                    WS_TABSTOP
//    DEFPUSHBUTTON   "OK",IDOK,100,76,56,16
//END

// DS_SETFOREGROUND = 0x00000200
// WS_VISIBLE       = 0x10000000
// WS_CAPTION       = 0x00C00000

const unsigned short IDD_CHOOSEAUDIO2[] = {
	// DLGTEMPLATE
	0x0240, 0x10C0,		// style (DS_SETFONT | DS_SETFOREGROUND | WS_VISIBLE | WS_CAPTION)
	0, 0,				// dwExtendedStyle
	3,					// cdit
	0, 0, 256, 96,		// x, y, cx, cy
	0, 0,				// menu, class
	'M','u','l','t','i','p','l','e',' ','A','u','d','i','o',' ',
	'S','t','r','e','a','m','s',' ','F','o','u','n','d',' ',0,
	8,					// pointsize
	'M','S',' ','S','a','n','s',' ','S','e','r','i','f',0,

	// DLGITEMTEMPLATE
	0x0001, 0x5002,		// style (WS_CHILD | WS_VISIBLE | WS_GROUP | SS_CENTER)
	0, 0,				// dwExtendedStyle
	87, 4, 82, 8,		// x, y, cx, cy
	0xFFFF,				// id (IDC_STATIC)
	0xFFFF, 0x0082,		// class (Static)
	'C','h','o','o','s','e',' ','a','n',' ',
	'a','u','d','i','o',' ','s','t','r','e','a','m',':',0,
	0,					// data

	// DLGITEMTEMPLATE
	0x0101, 0x50A1,		// style (WS_CHILD | WS_VISIBLE | WS_BORDER |
	// WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY)
	0, 0,				// dwExtendedStyle
	4, 16, 248, 56,		// x, y, cx, cy
	IDC_LIST1,			// id
	0xFFFF, 0x0083,		// class (List Box)
	'L','i','s','t','1',0,
	0,					// data

	// DLGITEMTEMPLATE
	0x0001, 0x5001,		// style (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON)
	0, 0,				// dwExtendedStyle
	100, 76, 56, 16,	// x, y, cx, cy
	IDOK,				// id
	0xFFFF, 0x0080,		// class (Button)
	'O','K',0,
	0					// data
};

INT_PTR CALLBACK InputFileMPEG2::ChooseAudioDlg(HWND hDlg, UINT uMsg,
	WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {

	case WM_INITDIALOG:
		{
			char buf[48];

			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			InputFileMPEG2 *thisPtr = (InputFileMPEG2 *)lParam;

			HWND hwndList = GetDlgItem(hDlg, IDC_LIST1);
			int i;

			// Any MPEG audio streams (C0 to DF)
			for (i = 64; i < 96; ++i) {
				if (thisPtr->audio_stream_list[i]) {
					sprintf(buf, "MPEG Audio Stream 0x%02X", i + 128);
					SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)buf);
				}
			}

			// Any AC-3 audio streams?
			for (i = 0; i < 8; ++i) {
				if (thisPtr->audio_stream_list[i]) {
					sprintf(buf, "Dolby AC-3 Audio (substream 0x%02X)", i + 128);
					SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)buf);
				}
			}

			// Any LPCM audio streams?
			for (i = 32; i < 40; ++i) {
				if (thisPtr->audio_stream_list[i]) {
					sprintf(buf, "Linear PCM Audio (substream 0x%02X)", i + 128);
					SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)buf);
				}
			}

			SendMessage(hwndList,LB_SETCURSEL,0,0);
			return TRUE;
		}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			int i, sel;

			InputFileMPEG2 *thisPtr = (InputFileMPEG2 *)GetWindowLongPtr(hDlg, DWLP_USER);

			HWND hwndList = GetDlgItem(hDlg, IDC_LIST1);
			sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);

			// Any MPEG audio streams (C0 to DF)
			for (i = 64; i < 96; ++i) {
				if (thisPtr->audio_stream_list[i]) {
					if (sel == 0) break;
					--sel;
				}
			}

			if (i == 96) {
				// Any AC-3 audio streams?
				for (i = 0; i < 8; ++i) {
					if (thisPtr->audio_stream_list[i]) {
						if (sel == 0) break;
						--sel;
					}
				}

				if (i == 8) {
					// Any LPCM audio streams?
					for (i = 32; i < 40; ++i) {
						if (thisPtr->audio_stream_list[i]) {
							if (sel == 0) break;
							--sel;
						}
					}
				}
			}

			EndDialog(hDlg, i);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

#endif	// MULTIAUDIO


void VDXAPIENTRY InputFileMPEG2::Init(const wchar_t *szFile, IVDXInputOptions *opts)
{
	struct InputOptionsMPEG2::OptionsMPEG2 tOptions;
	MPEGFileParser *fileParser;

	AppendNames an;
	bool fHasIndex;
	int i;

	if (opts != NULL)
	{
		// Get options from VirtualDub
		InputOptionsMPEG2 *pOptions = (InputOptionsMPEG2 *)opts;
		pOptions->Write(&tOptions, sizeof(tOptions));
	}
	else
	{
		// Get options (if any) from the registry
		InputOptionsMPEG2 pOptions;
		pOptions.Write(&tOptions, sizeof(tOptions));
	}

	fAllowDSC    = tOptions.fAllowDSC;
	fAllowMatrix = tOptions.fAllowMatrix;
	fHasIndex    = ReadIndex(szFile, &an);

	if (!fHasIndex)
	{
		// see if we can open the file

		i = lstrlenW(szFile);
		if (i <= 0 || i >= MAX_PATH)
		{
			mContext.mpCallbacks->SetError("Bad file name %ls", szFile);
			return;
		}
			
		an.fName[0] = new WCHAR[i + 1];
		if (an.fName[0] == NULL)
		{
			mContext.mpCallbacks->SetErrorOutOfMemory();
			return;
		}
			
		memcpy(an.fName[0], szFile, (i + 1) * sizeof(wchar_t));
		an.num_files = 1;

		if (tOptions.fMultipleOpen)
		{
			// If we were invoked via script, show no UI
			AppendDlg(&an, &mFile, (uiFlags & IVDXInputFileDriver::kFlagQuiet) == 0, g_hModule, GetActiveWindow());

			if (an.num_files <= 0)
			{
				mContext.mpCallbacks->SetError("Open multiple files failed");
				return;
			}
		}
		else
		{
			if (!mFile.inOpen(an.fName[0]))
			{
				mContext.mpCallbacks->SetError("Cannot open %ls", szFile);
				return;
			}
		}

		// Create and invoke MPEGFileParser
		fileParser = new MPEGFileParser(this);

		if (fileParser != NULL)
		{
			fileParser->Parse(g_hModule);

			// Voila; the MPEG is parsed.
			delete fileParser;
			fileParser = NULL;
		}
		else
		{
			mContext.mpCallbacks->SetErrorOutOfMemory();
			return;
		}

		if ((system != 0 && vpackets == 0) || vframes == 0)
		{
			mContext.mpCallbacks->SetError("No video stream!");
			return;
		}

		// MPEG-2 aspect ratio?
		if (seq_ext != 0) aspect_ratio += 16;
	}

	mNumFiles = an.num_files;

	if (!fUserAborted)
	{
		if (tOptions.fCreateIndex) WriteIndex(szFile, &an);
	}

#ifndef MULTIAUDIO
	for (i = 0; i < 96; ++i) {
		if (audio_stream_list[i]) {
			if (iChosenAudio >= 0) {
				// Multiple audio streams found
				iChosenAudio = DialogBoxIndirectParamA(g_hModule,
					(LPCDLGTEMPLATE)IDD_CHOOSEAUDIO2, GetActiveWindow(),
					ChooseAudioDlg, (LPARAM)this);

				break;
			}
			iChosenAudio = i;
		}
	}
#endif	// MULTIAUDIO

	if (!PostInit())
	{
		mContext.mpCallbacks->SetError("PostInit failed.");
		return;
	}
}


bool InputFileMPEG2::PostInit()
{
	unsigned int i;
	unsigned int j;
	unsigned int count;
	unsigned int largest;
	int id;

	// If there are field pictures, they must come in pairs.
	// Combine matching pairs into frame pictures and adjust
	// the total number of frames.

	// This must be done BEFORE reordering pictures

	i = 0;
	while (i < (vframes - 1))
	{
		MPEGSampleInfo& m1 = video_sample_list[i + 0];
		MPEGSampleInfo& m2 = video_sample_list[i + 1];

		if ( ((m1.bitflags & 3) == TOP_FIELD && (m2.bitflags & 3) == BOTTOM_FIELD)
		  || ((m1.bitflags & 3) == BOTTOM_FIELD && (m2.bitflags & 3) == TOP_FIELD))
		{
			if (m1.frame_type == m2.frame_type ||
			   (m1.frame_type == MPEG_FRAME_TYPE_I &&
			    m2.frame_type == MPEG_FRAME_TYPE_P))
			{
				// Combine them, and remove the second field
				m1.size += m2.size;

				for (j = i + 1; j < (vframes - 1); ++j)
				{
					video_sample_list[j] = video_sample_list[j + 1];
				}
				--vframes;
			}
		}
		++i;
	}

    // Now bubble-sort the frames into display order,
	for (i = 1; i < vframes; ++i)
	{
		if (video_sample_list[i].frame_type == MPEG_FRAME_TYPE_B)
		{
			MPEGSampleInfo m1 = video_sample_list[i];
			video_sample_list[i] = video_sample_list[i - 1];
			video_sample_list[i - 1] = m1;
		}
	}

	// To be fully decodable, the resulting sequence must
	// begin with an I-frame, and mustn't end with a B-frame

	// Remove leading P/B frames
	for (i = 0; i < vframes; ++i)
	{
		if (video_sample_list[i].frame_type == MPEG_FRAME_TYPE_I)
		{
			break;
		}
	}
	// "i" is now a count of how many frames precede the first I-frame
	if (i > 0)
	{
		if (i >= vframes) return false; // No I-frames at all?
		vframes -= i;
		for (j = 0; j < vframes; ++j)
		{
			video_sample_list[j] = video_sample_list[j + i];
		}
	}

	// Remove trailing B frames
	while (video_sample_list[vframes - 1].frame_type == MPEG_FRAME_TYPE_B)
	{
		--vframes;
	}
	if (vframes == 0) return false;

	for (id = 0; id < 48; ++id)
	{
		if (aframes[id] != 0 && audio_sample_list[id] != NULL)
		{
			// The skew is in 90 KHz ticks:
			int srate;
			int iSamplesPerFrame = 0;

			int skew = (int)(afirstPTS[id] - vfirstPTS);

			// Calculate the duration of a frame in 90 KHz ticks

			if (id < 32)
			{
				// MPEG Audio
				MPEGAudioCompacted hdr(audio_sample_list[id][0].header);
				srate = (int)hdr.GetSamplingRateHz();
				iSamplesPerFrame = (hdr.GetLayer() == 1)? 384: 1152;
			}
			else if (id < 40)
			{
				// Dolby Digital Audio
				if (ac3_compact_info(audio_sample_list[id][0].header, &srate, NULL, NULL))
				{
					iSamplesPerFrame = 1536;
				}
			}
			else
			{
				// Linear PCM Audio
				srate = (audio_sample_list[id][0].header & 0x30)? 96000: 48000;
				iSamplesPerFrame = 1536;
			}

			if (srate > 0 && iSamplesPerFrame > 0)
			{
				int iTicksPerFrame = (int)((90000.0 * iSamplesPerFrame) / srate + 0.5);
				if (iTicksPerFrame > 0)
				{
					MPEGSampleInfo *asl = audio_sample_list[id];

					if (skew < 0)
					{
						// Remove audio frames if the skew is negative
						skew = (-skew + 45) / iTicksPerFrame;

						// "skew" is now the number of frames to remove
						if (skew > 0)
						{
							for (unsigned int k = skew; k < aframes[id]; ++k)
							{
								asl[k - skew] = asl[k];
							}
							aframes[id] -= skew;
						}
					}
					else if (skew > 0)
					{
						// Duplicate audio frames if the skew is positive
						skew = (skew + 45) / iTicksPerFrame;

						// "skew" is now the number of frames to duplicate
						if (skew > 0 && (unsigned int)skew < aframes[id])
						{
							int k;
							for (k = aframes[id] - 1; k >= skew; --k)
							{
								asl[k] = asl[k - skew];
							}
							while (k >= 0)
							{
								asl[k] = asl[skew];
								--k;
							}
						}
					}
				}
			}
		}
	}

	// Generate field map
	// While we're at it, determine the largest frame

	// "Force FILM" is a work in progress.  For now,
	// just emulate the 1.5.10 behavior and ignore RFF

	count = 0;
	j = 0;	// cadence

	for (i = 0; i < vframes; ++i)
	{
		const MPEGSampleInfo& m1 = video_sample_list[i];

		// this is overly complicated
		if (   ( m1.bitflags & PICTUREFLAG_RFF)
		    && ((m1.bitflags & 3) == FRAME_PICTURE)
		    && ( m1.bitflags & PICTUREFLAG_PF))
		{
			if (progressive_sequence)
			{
				if (m1.bitflags & PICTUREFLAG_TFF) {
					count += 3;	// 3 frames
				} else {
					count += 2;	// 2 frames
				}
			} else {
				++count;		// 3 fields
				j = 1 - j;		// flip the cadence
				if (j == 0) ++count;
			}
		}
		else
		{
			++count;			// 1 frame, or 2 fields
		}
	}

	video_field_map = new unsigned int[count];
	if (video_field_map == NULL) return false;

	// Now load the map
	fields = count;
	count  = 0;

	j = 0;	// cadence
	largest = 0;

	for (i = 0; i < vframes; ++i)
	{
		const MPEGSampleInfo& m1 = video_sample_list[i];

		if (m1.size > largest) largest = m1.size;

		if (   ( m1.bitflags & PICTUREFLAG_RFF)
		    && ((m1.bitflags & 3) == FRAME_PICTURE)
		    && ( m1.bitflags & PICTUREFLAG_PF))
		{
			// Add 1 progressive frame
			video_field_map[count++] = i * 2 + j;

			if (progressive_sequence)
			{
				// 2 progressive frames
				video_field_map[count++] = i * 2 + j;
				if (m1.bitflags & PICTUREFLAG_TFF)
				{
					// 3 progressive frames
					video_field_map[count++] = i * 2 + j;
				}
			}
			else
			{
				j = 1 - j;	// flip cadence
				if (j == 0)
				{
					// Add a duplicate frame
					video_field_map[count++] = i * 2 + j;
				}
			}
		}
		else
		{
			// 1 progressive frame, or 2 field pictures
			video_field_map[count++] = i * 2 + j;
		}
	}

    // Set video_packet_buffer to size of the largest frame
    largest_framesize = (largest + 31) & -15;
    return true;
}


bool VDXAPIENTRY InputFileMPEG2::Append(const wchar_t *szFile)
{
	return false;
}


bool VDXAPIENTRY InputFileMPEG2::PromptForOptions(VDXHWND hwndParent, IVDXInputOptions **ppOptions)
{
	InputOptionsMPEG2 *pOpts = new InputOptionsMPEG2;
	*ppOptions = pOpts;
	if (pOpts == NULL) return false;

	pOpts->AddRef();
	pOpts->OptionsDlg((HWND)hwndParent);
	return true;
}


bool VDXAPIENTRY InputFileMPEG2::CreateOptions(const void *buf, uint32 len, IVDXInputOptions **ppOptions)
{
	InputOptionsMPEG2 *pOpts = new InputOptionsMPEG2;
	*ppOptions = pOpts;
	if (pOpts == NULL) return false;

	pOpts->AddRef();

	if (len > pOpts->opts.len) len = pOpts->opts.len;
	memcpy(&(pOpts->opts), buf, len);

	return true;
}


void VDXAPIENTRY InputFileMPEG2::DisplayInfo(VDXHWND hwndParent)
{
    DialogBoxParamA(g_hModule, MAKEINTRESOURCEA(IDD_INFORMATION),
		(HWND)hwndParent, InfoDlgProc, (LPARAM)this);
}


bool VDXAPIENTRY InputFileMPEG2::GetVideoSource(int index, IVDXVideoSource **ppVS)
{
	*ppVS = NULL;
	if (index != 0) return false;

	*ppVS = new VideoSourceMPEG2(this);
	if (*ppVS == NULL) return false;

	(*ppVS)->AddRef();
	return true;
}


bool VDXAPIENTRY InputFileMPEG2::GetAudioSource(int index, IVDXAudioSource **ppAS)
{
	unsigned int id;
	int i;

#ifdef MULTIAUDIO
	for (i = 0; i < 96; ++i)
	{
		id = audio_stream_list[i] - 1;
		if (id < 48U)
		{
			if (index == 0) break;
			--index;
		}
	}
#else
	i = iChosenAudio;
	if (index != 0 || (unsigned int)i >= 96U) return false;
	id = audio_stream_list[i] - 1;
#endif

	if (i < 96)
	{
		if (id < 32)
		{
			if (fAllowDSC) {
				*ppAS = CreateAudioSourceMPEG(this, id);
			} else {
				*ppAS = CreateAudioSourceMPEG2(this, id);
			}
		} else if (id < 40) {
			*ppAS = CreateAudioSourceAC3(this, id);
		} else {
			*ppAS = CreateAudioSourceLPCM(this, id);
		}
	}

	if (*ppAS == NULL) return false;

	(*ppAS)->AddRef();
	return true;
}


__int64 InputFileMPEG2::GetSamplePosition(unsigned int sample, int stream_id)
{
	const MPEGPacketInfo *packet_list;
	unsigned int pkt;
	unsigned int delta;

	if (stream_id > 0)
	{
		// Audio
		--stream_id;
		pkt = audio_sample_list[stream_id][sample].pktindex;
		delta = audio_sample_list[stream_id][sample].pktoffset;
		packet_list = audio_packet_list[stream_id];
	}
	else
	{
		// Video
		pkt = video_sample_list[sample].pktindex;
		delta = video_sample_list[sample].pktoffset;
		packet_list = video_packet_list;
	}
	return ((packet_list[pkt].file_pos >> 16) + delta);
}


void InputFileMPEG2::ReadSample(void *buffer, unsigned int sample, int len, int stream_id)
{
	const MPEGPacketInfo *packet_list;
	unsigned int pkt, pkts;
	unsigned int delta;
	unsigned int tc;

	if (stream_id > 0)
	{
		// Audio
		--stream_id;
		pkts = apackets[stream_id];
		pkt = audio_sample_list[stream_id][sample].pktindex;
		delta = audio_sample_list[stream_id][sample].pktoffset;
		packet_list = audio_packet_list[stream_id];
	}
	else
	{
		// Video
		pkts = vpackets;
		pkt = video_sample_list[sample].pktindex;
		delta = video_sample_list[sample].pktoffset;
		packet_list = video_packet_list;
	}

	while (len > 0)
	{
		if (pkt >= pkts)
		{
			mContext.mpCallbacks->SetError(
				"Attempt to read past end of %s stream",
				(stream_id > 0)? "audio": "video"
			);
			break;
		}

		// Size of packet minus the offset:
		tc = (unsigned int)(packet_list[pkt].file_pos & 0xFFFF) - delta;
		if (tc > (unsigned int)len) tc = len;

		mFile.inSeek((packet_list[pkt].file_pos >> 16) + delta);
		mFile.inRead(buffer, tc);

		len -= tc;
		buffer = (char *)buffer + tc;
		++pkt;
		delta = 0;
	}
}


/************************************************************************
 *
 *                  InputFileDriverMPEG2
 *
 ************************************************************************/


class InputFileDriverMPEG2 : public vdxunknown<IVDXInputFileDriver> {
public:
	InputFileDriverMPEG2(const VDXInputDriverContext& context);
	~InputFileDriverMPEG2();

	// IVDXInputFileDriver
	int     VDXAPIENTRY DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize);
	bool    VDXAPIENTRY CreateInputFile(uint32 flags, IVDXInputFile **ppFile);

protected:
	const VDXInputDriverContext& mContext;
};


InputFileDriverMPEG2::InputFileDriverMPEG2(const VDXInputDriverContext& context)
	: mContext(context)
{
}


InputFileDriverMPEG2::~InputFileDriverMPEG2()
{
}


int VDXAPIENTRY InputFileDriverMPEG2::DetectBySignature(
	const void *pHeader, sint32 nHeaderSize,
	const void *pFooter, sint32 nFooterSize, sint64 nFileSize)
{
	return -1;
}


bool VDXAPIENTRY InputFileDriverMPEG2::CreateInputFile(uint32 flags, IVDXInputFile **ppFile)
{
	*ppFile = new InputFileMPEG2(mContext, flags);
	if (*ppFile == NULL) return false;

	(*ppFile)->AddRef();
	return true;
}


bool VDXAPIENTRY mpeg2_create(const VDXInputDriverContext *pContext, IVDXInputFileDriver **ppDriver)
{
	*ppDriver = new InputFileDriverMPEG2(*pContext);
	if (*ppDriver == NULL) return false;

	(*ppDriver)->AddRef();
	return true;
}


const uint8 MPEG2_sig[] = {
	0x00, 0xFF,
	0x00, 0xFF,
	0x01, 0xFF,
//	0xB2, 0xF6		// either 0x1B3 or 0x1BA
    0x80, 0x80
};

//	0010
//	0110
//	x01x	anything can go here
//	0010	0x1B2
//	0011	0x1B3
//	1010	0x1BA
//	1011	0x1BB

const VDXInputDriverDefinition mpeg2_input = {
	sizeof(VDXInputDriverDefinition),		// size of this structure
	VDXInputDriverDefinition::kFlagSupportsVideo |
	VDXInputDriverDefinition::kFlagSupportsAudio, // |
//	VDXInputDriverDefinition::kFlagCustomSignature,
	1,										// priority
	sizeof(MPEG2_sig),						// size of file signature
	MPEG2_sig,								// pointer to signature
	L"*.mpg|*.vob|*.m2v|*.mpeg|*.mpv|*.vdr",			// extension pattern
	L"MPEG-2 files (*.mpg,*.vob,*.m2v,*.mpeg,*.mpv,*.vdr)|*.mpg;*.vob;*.m2v;*.mpeg;*.mpv;*.vdr",
	L"MPEG-2",								// driver target name
	mpeg2_create							// create proc
};

const VDXPluginInfo mpeg2_info = {
	sizeof(VDXPluginInfo),				// size of this structure
	L"MPEG-2",							// name of this plugin
	L"fccHandler",						// author's name
	L"Loads MPEG-2 program streams.",	// plugin description
	0x04050000,							// version = 4.5.0000
	kVDXPluginType_Input,				// type of plugin = input
	0,									// flags = 0
	10,									// API version required
	kVDXPlugin_APIVersion,				// API version used
	2,									// API version required
	kVDXPlugin_InputDriverAPIVersion,	// API version used
	&mpeg2_input						// driver definition
};


const uint8 MIDX_sig[] = {
	'M', 0xFF,
	'I', 0xFF,
	'D', 0xFF,
	'X', 0xFF,
	0, 0xFF, 2, 0xFF,	// (Motorola) major version 2
	0, 0xFF, 0, 0xFE	// (Motorola) minor version 0 or 1
};

const VDXInputDriverDefinition midx_input = {
	sizeof(VDXInputDriverDefinition),		// size of this structure
	VDXInputDriverDefinition::kFlagSupportsVideo |
	VDXInputDriverDefinition::kFlagSupportsAudio,
	0,										// priority
	sizeof(MIDX_sig),						// size of file signature
	MIDX_sig,								// pointer to signature
	L"*.midx",								// extension pattern
	L"MPEG-2 index (*.midx)|*.midx",
	L"MIDX",								// driver target name
	mpeg2_create							// create proc
};

const VDXPluginInfo midx_info = {
	sizeof(VDXPluginInfo),				// size of this structure
	L"MIDX",							// name of this plugin
	L"fccHandler",						// author's name
	L"Loads MPEG-2 index files.",		// plugin description
	0x02010000,							// version = 2.1.0000
	kVDXPluginType_Input,				// type of plugin = input
	0,									// flags = 0
	10,									// API version required
	kVDXPlugin_APIVersion,				// API version used
	2,									// API version required
	kVDXPlugin_InputDriverAPIVersion,	// API version used
	&midx_input							// driver definition
};


///////////////////////////////////////////////////////////////////////////

const VDXPluginInfo *const kPlugins[] = {
	&mpeg2_info,
	&midx_info,
	NULL
};

extern "C" const VDXPluginInfo *const * VDXAPIENTRY VDGetPluginInfo() {
	return kPlugins;
}
