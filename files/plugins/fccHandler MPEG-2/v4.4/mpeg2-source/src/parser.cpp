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

// MPEG System, Video, and Audio Parsers

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
	#define _CRT_SECURE_NO_WARNINGS
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "vd2/plugin/vdinputdriver.h"
#include "Unknown.h"

#include "infile.h"
#include "append.h"
#include "InputFileMPEG2.h"
#include "InputOptionsMPEG2.h"
#include "DataVector.h"
#include "parser.h"
#include "video.h"
#include "audio.h"

#include "resource.h"

#include <crtdbg.h>

#define VIDPKT_TYPE_SEQUENCE_START		0x1B3
#define VIDPKT_TYPE_SEQUENCE_END		0x1B7
#define VIDPKT_TYPE_GROUP_START			0x1B8
#define VIDPKT_TYPE_PICTURE_START		0x100
#define VIDPKT_TYPE_SLICE_START_MIN		0x101
#define VIDPKT_TYPE_SLICE_START_MAX		0x1AF
#define VIDPKT_TYPE_EXT_START			0x1B5
#define VIDPKT_TYPE_USER_START			0x1B2

#define SEQUENCE_EXTENSION_ID			1
#define SEQUENCE_DISPLAY_EXTENSION_ID	2
#define PICTURE_CODING_EXTENSION_ID		8


/************************************************************************
 *
 *                  MPEGAudioParser
 *
 ************************************************************************/

class MPEGAudioParser {
private:
	unsigned int    lFirstHeader;

protected:
	unsigned int    header;
	MPEGSampleInfo  msi;
	int             bytes;
	int             skip;

public:
	MPEGAudioParser();

	DataVector audio_stream_blocks;
	DataVector audio_stream_samples;

	virtual void Parse(const unsigned char *pData, int len, unsigned int pkt);
	virtual void Finish() { return; };
};


MPEGAudioParser::MPEGAudioParser()
	: audio_stream_blocks   (sizeof(MPEGPacketInfo))
	, audio_stream_samples  (sizeof(MPEGSampleInfo))
	, lFirstHeader          (0)
	, header                (0)
	, bytes                 (0)
	, skip                  (0)
{
}


void MPEGAudioParser::Parse(const unsigned char *pData, int len, unsigned int pkt)
{
	const unsigned char *src = pData;

	while (len > 0)
	{
		if (skip > 0)
		{
			int tc = skip;
			if (tc > len) tc = len;

			len  -= tc;
			skip -= tc;
			src  += tc;

			// Audio frame finished?
			if (skip == 0)
			{
				msi.pktindex  = pkt;
				msi.pktoffset = (unsigned short)(src - pData);
				audio_stream_samples.Add(&msi);
			}
			continue;
		}

		// Collect header bytes.
		header = (header << 8) + *src++;
		++bytes;
		--len;

		if (bytes == 4)
		{
			MPEGAudioHeader hdr(header);
			--bytes;

			if (lFirstHeader)
			{
				// Check for consistency
				if (!hdr.IsConsistent(lFirstHeader)) continue;
			}
			else
			{
				if (!hdr.IsValid()) continue;
				lFirstHeader = header;
			}

			// Okay, we like the header.
			bytes = 0;

			// Setup the sample information.
			// Don't add the sample, in case it's incomplete.
			msi.header = mpeg_compact_header(header);
			msi.size   = hdr.GetFrameSize();

			// Now skip the remainder of the sample.
			skip = msi.size - 4;
		}
	}
}


/************************************************************************
 *
 *                  AC3AudioParser
 *
 ************************************************************************/

class AC3AudioParser : public MPEGAudioParser {
private:
	unsigned short syncword;
public:
	AC3AudioParser();
	void Parse(const unsigned char *pData, int len, unsigned int curpkt);
};


AC3AudioParser::AC3AudioParser()
	: syncword  (0)
{
}


void AC3AudioParser::Parse(const unsigned char *pData, int len, unsigned int pkt)
{
	const unsigned char *src = pData;

	while (len > 0)
	{
		if (skip > 0)
		{
			int tc = skip;
			if (tc > len) tc = len;

			len  -= tc;
			skip -= tc;
			src  += tc;

			// Audio frame finished?
			if (skip == 0)
			{
				msi.pktindex  = pkt;
				msi.pktoffset = (unsigned short)(src - pData);
				audio_stream_samples.Add(&msi);
			}
			continue;
		}

		if (syncword == 0x0B77)
		{
			// Collect header bytes.
			header = (header << 8) + *src++;
			++bytes;
			--len;

			if (bytes == 6)
			{
				// We have all 8 bytes (including the syncword)
				bytes    = 0;
				syncword = 0;

				skip = ac3_sync_info(header, NULL, NULL, NULL);
				if (skip != 0)
				{
					msi.header = ac3_compact_header(header);
					msi.size   = skip;
					skip -= 8;
				}
			}
			continue;
		}

		// Search for syncword
		syncword = (syncword << 8) + *src++;
		--len;
	}
}


/************************************************************************
 *
 *                  PCMAudioParser
 *
 ************************************************************************/

class PCMAudioParser : public MPEGAudioParser {
public:
	void Parse(const unsigned char *src, int len, unsigned int pkt);
	void Finish();
};


void PCMAudioParser::Parse(const unsigned char *src, int len, unsigned int pkt)
{
	// Since LPCM is raw audio, there isn't anything to parse,
	// thus the "src" data pointer is always NULL except when
	// sending the initial LPCM header (6 bytes).

	if (src != NULL)
	{
		// src[4] = 5th byte of 6 byte LPCM header:
		// xx......  00=16-,01=20-,10=24-bit (11=forbidden)
		// ..xx....  00=48,01=96 KHz (others reserved)
		// ....x...  reserved
		// .....xxx  number of channels - 1

		const int chans = (src[4] & 7) + 1;
		const int bits  = 16 + ((src[4] & 0xC0) >> 4);

		bytes = chans * (PCM_BUFFER_SIZE / 8) * bits;

		msi.header    = src[4];		// this never changes
		msi.pktindex  = 0;
		msi.pktoffset = 0;
		msi.size      = 0;
		return;
	}

	msi.size += len;

	while (msi.size >= (unsigned int)bytes)
	{
		unsigned int tc = msi.size - bytes;

		// Finish off this block
		msi.size = bytes;
		audio_stream_samples.Add(&msi);

		// Start a new block
		msi.size      = tc;
		msi.pktindex  = pkt;
		msi.pktoffset = len - tc;
	}
}

void PCMAudioParser::Finish()
{
	// Finish off the final block

	if (msi.size > 0)
	{
		// Make sure we keep only whole samples
		const int chans = (msi.header & 7) + 1;
		const int bits  = 16 + ((msi.header & 0xC0) >> 4);

		msi.size -= msi.size % (chans * bits);
		if (msi.size > 0)
		{
			audio_stream_samples.Add(&msi);
		}
	}
}


/************************************************************************
 *
 *                  MPEGVideoParser
 *
 ************************************************************************/

class MPEGVideoParser {

private:
	unsigned char   buf[8];
	MPEGSampleInfo  msi;
	__int64         bytepos;
	__int64         last_access_unit;
	unsigned int    header;

	int     idx;
	int     bytes;

	bool    fPicturePending;
	bool    fFoundSequenceStart;
	bool    fFoundSeqDispExt;

	bool    sequence_header();
	bool    picture_start_code();
	void    extension_start_code();

public:

	VDXFraction mFrameRate;
	DataVector  video_stream_samples;

	int             width;
	int             height;
	int             display_width;
	int             display_height;
	unsigned int    seq_ext;
	int             aspect_ratio_info;
	bool            progressive_sequence;
	int             matrix_coefficients;

	MPEGVideoParser();

	bool Parse(const unsigned char *pData, int len, unsigned int curpkt);
};


MPEGVideoParser::MPEGVideoParser()
	: video_stream_samples  (sizeof(MPEGSampleInfo))
	, bytepos               (0)
	, last_access_unit      (-1i64)
	, idx                   (0)
	, bytes                 (0)
	, header                (0xFFFFFFFF)
	, fPicturePending       (false)
	, fFoundSequenceStart   (false)
	, fFoundSeqDispExt      (false)
	, width                 (0)
	, height                (0)
	, display_width         (0)
	, display_height        (0)
	, seq_ext               (0)
	, aspect_ratio_info     (0)
	, matrix_coefficients   (5)
{
	mFrameRate.mNumerator   = 1;
	mFrameRate.mDenominator = 1;
	msi.frame_type          = 0;
}


inline bool MPEGVideoParser::picture_start_code()
{
	// xxxxxxxx xx......	temporal reference
	// ........ ..xxx...	picture coding type
	// ........ ........

	header = 0xFFFFFFFF;

	// Fix for Shakira: don't add picture if it is corrupt

	int ft = (buf[1] >> 3) & 7;

	switch (ft) {
		case MPEG_FRAME_TYPE_I:
		case MPEG_FRAME_TYPE_P:
		case MPEG_FRAME_TYPE_B:
			break;
		default:
			return false;
	}

	msi.frame_type  = (unsigned char)ft;
	msi.bitflags    = PICTUREFLAG_PF | FRAME_PICTURE;
	fPicturePending = true;

	return true;
}


// Fix for Kylie: limit what res we will support
#define MPEG_MAX_WIDTH		1920
#define MPEG_MAX_HEIGHT		1088
// 1920*1080*4 = 8MB buffer

inline bool MPEGVideoParser::sequence_header()
{
	static const VDXFraction frates[16] = {
		{15000, 1000},		// 0000 forbidden
		{24000, 1001},		// 0001 23.976
		{24000, 1000},		// 0010 24
		{25000, 1000},		// 0011 25
		{30000, 1001},		// 0100 29.97
		{30000, 1000},		// 0101 30
		{50000, 1000},		// 0110 50
		{60000, 1001},		// 0111 59.94
		{60000, 1000},		// 1000 60
		{15000, 1000},		// 1001 reserved
		{15000, 1000},		// 1010 reserved
		{15000, 1000},		// 1011 reserved
		{15000, 1000},		// 1100 reserved
		{15000, 1000},		// 1101 reserved
		{15000, 1000},		// 1110 reserved
		{15000, 1000}		// 1111 reserved
	};

// bytes 0 to 3
// xxxxxxxx xxxx.... ........ ........	vertical_size
// ........ ....xxxx xxxxxxxx ........	horizontal_size
// ........ ........ ........ xxxx....	pel_aspect_ratio
// ........ ........ ........ ....xxxx	picture_rate

// bytes 4 to 7
// xxxxxxxx xxxxxxxx xx...... ........  bit_rate
// ........ ........ ..1..... ........  marker_bit
// ........ ........ ...xxxxx xxxxx...  vbv_buffer_size
// ........ ........ ........ .....x..  constrained_parameters_flag
// ........ ........ ........ ......x.  load_intra_quantiser_matrix
// ........ ........ ........ .......x  load_non_intra_quantiser_matrix

	// Verify that this is a valid sequence header
	const int w  = (buf[0] << 4) | (buf[1] >> 4);
	const int h  = ((buf[1] & 15) << 8) | buf[2];
	const int ar = buf[3] >> 4;
	const int pr = buf[3] & 15;
	const int br = (buf[4] << 10) | (buf[5] << 2) | (buf[6] >> 6);

	// For robustness, we will only fail if:
	//	a) This appears to be a valid sequence header, and
	//	b) width or height doesn't match previous sequence headers

	header = 0xFFFFFFFF;

	if (   w != 0 && w <= MPEG_MAX_WIDTH
	    && h != 0 && h <= MPEG_MAX_HEIGHT
	    && ar != 0 && pr != 0 && br != 0
	    && (buf[6] & 0x20) != 0)
	{
		if (fFoundSequenceStart)
		{
			// We already have a sequence, so check the new
			// values against the original sequence header.
			// For robustness, only check width and height.

			if (w != width && h != height) return false;
		}
		else
		{
			// First sequence header; save the values.

			width               = w;
			height              = h;
			display_width       = w;
			display_height      = h;
			aspect_ratio_info   = ar;
			mFrameRate          = frates[pr];
			fFoundSequenceStart = true;
		}
	}

	return true;
}


inline void MPEGVideoParser::extension_start_code()
{
	// EXTENSION_START_CODE

	// Note: The caller has already checked the following:
	// 1) fFoundSequenceStart is true

	switch (buf[0] >> 4)
	{

	// Execute a break if we either consume or ignore this extension.
	// If we need more bytes to decide, change bytes to that amount
	// and return; do not break in that case!

	case SEQUENCE_EXTENSION_ID:
	{
		// Have we already parsed a sequence_extension?
		if (seq_ext != 0) break;

		// Are there at least 6 bytes in the buffer?
		if (bytes < 6)
		{
			bytes = 6;
			return;
		}

		//  0:  xxxx....  SEQUENCE_EXTENSION_ID
		//      ....x...  Escape bit
		//      .....xxx  Profile identification
		//  1:  xxxx....  Level identification
		//      ....x...  progressive_sequence
		//      .....xx.  chroma_format
		//      .......x  horizontal_size_extension[1]
		//  2:  x.......  horizontal_size_extension[0]
		//      .xx.....  vertical_size_extension
		//      ...xxxxx  bit_rate_extension[11..7]
		//  3:  xxxxxxx.  bit_rate_extension[6..0]
		//      .......1  marker_bit
		//  4:  xxxxxxxx  vbv_buffer_size_extension
		//  5:  x.......  low_delay
		//      .xx.....  frame_rate_extension_n
		//      ...xxxxx  frame_rate_extension_d

		seq_ext = *(unsigned int *)buf;

		progressive_sequence = (buf[1] >> 3) & 1;
//		width  = (width  & 0xFFF) | ((buf[1] & 1) << 13) | ((buf[2] & 0x80) << 5);
//		height = (height & 0xFFF) | ((buf[2] & 0x60) << 7);
		matrix_coefficients = 1;
		break;
	}

	case SEQUENCE_DISPLAY_EXTENSION_ID:
	{
		// Have we already parsed a sequence_display_extension?
		if (fFoundSeqDispExt) break;

		// Have we parsed a sequence_extension yet?
		if (seq_ext == 0) break;

		//  0:  xxxx....  SEQUENCE_DISPLAY_EXTENSION_ID
		//      ....xxx.  video_format
		//      .......x  colour_description
		//  1:  xxxxxxxx  colour_primaries
		//  2:  xxxxxxxx  transfer_characteristics
		//  3:  xxxxxxxx  matrix_coefficients
		//1 4:  xxxxxxxx  display_horizontal_size[13..6]
		//2 5:  xxxxxx..  display_horizontal_size[5..0]
		//      ......1.  marker_bit
		//      .......x  display_vertical_size[13]
		//3 6:  xxxxxxxx  display_vertical_size[12..5]
		//4 7:  xxxxx...  display_vertical_size[4..0]
		
		if (buf[0] & 1)
		{
			// colour_description is present;
			// Are there are least 8 bytes in the buffer?
			if (bytes < 8)
			{
				bytes = 8;
				return;
			}
			matrix_coefficients = buf[3];
			display_width  = (buf[4] << 6) | (buf[5] >> 2);
			display_height = ((buf[5] & 1) << 13) | (buf[6] << 5) | (buf[7] >> 3);
		}
		else
		{
			// colour_description is not present;
			// Are there are least 5 bytes in the buffer?
			if (bytes < 5)
			{
				bytes = 5;
				return;
			}
			display_width  = (buf[1] << 6) | (buf[2] >> 2);
			display_height = ((buf[2] & 1) << 13) | (buf[3] << 5) | (buf[4] >> 3);
		}

		fFoundSeqDispExt = true;
		break;
	}

	case PICTURE_CODING_EXTENSION_ID:
	{
		// Is a picture currently pending?
		if (!fPicturePending) break;

		// Have we parsed a sequence_extension yet?
		if (seq_ext == 0) break;

		// Are there at least 5 bytes in the buffer?
		if (bytes < 5)
		{
			bytes = 5;
			return;
		}

		//  0:  xxxx....  PICTURE_CODING_EXTENSION_ID
		//      ....xxxx  f_code[0][0], forward horizontal
		//  1:  xxxx....  f_code[0][1], forward vertical
		//      ....xxxx  f_code[1][0], backward horizontal
		//  2:  xxxx....  f_code[1][1], backward vertical
		//      ....xx..  intra_dc_precision
		//      ......xx  picture_structure
		//  3:  x.......  top_field_first
		//      .x......  frame_pred_frame_dct
		//      ..x.....  concealment_vectors
		//      ...x....  q_scale_type
		//      ....x...  intra_vlc_format
		//      .....x..  alternate_scan
		//      ......x.  repeat_first_field
		//      .......x  chroma_420_type
		//  4:  x.......  progressive_frame
		//      .x......  composite_display_flag
		//      ..x.....  v_axis
		//      ...xxx..  field_sequence
		//      ......x.  sub_carrier
		//      .......x  burst_amplitude[6]
		//      xxxxxx..  burst_amplitude[5..0]
		//      ......xx  sub_carrier_phase[7..6]
		//      xxxxxx..  sub_carrier_phase[5..0]

		msi.bitflags = buf[2] & 3;
		if (buf[3] & 0x80) msi.bitflags |= PICTUREFLAG_TFF;
		if (buf[3] & 0x02) msi.bitflags |= PICTUREFLAG_RFF;
		if (buf[4] & 0x80) msi.bitflags |= PICTUREFLAG_PF;
		break;
	}

	default:
		break;
	}

	header = 0xFFFFFFFF;
}


bool MPEGVideoParser::Parse(const unsigned char *pData, int len, unsigned int curpkt)
{
	const unsigned char *src = pData;

	while (len > 0)
	{
		// bytes is zero when the class is created,
		// thus (idx < bytes) is false the first time

		if (idx < bytes)
		{
			int tc = bytes - idx;
			if (tc > len) tc = len;

			memcpy(buf + idx, src, tc);

			len -= tc;
			idx += tc;
			src += tc;

			// Finished?
			if (idx >= bytes)
			{
				switch (header) {

				case VIDPKT_TYPE_PICTURE_START:
					if (picture_start_code() && last_access_unit == -1i64)
					{
						last_access_unit = bytepos + ((src - 6) - pData);
					}
					break;

				case VIDPKT_TYPE_SEQUENCE_START:
					if (!sequence_header()) return false;

					// A sequence header is ALWAYS an access unit
					last_access_unit = bytepos + ((src - 12) - pData);
					break;

				case VIDPKT_TYPE_EXT_START:
					extension_start_code();
					break;
				}
			}
			continue;
		}

		// Look for a valid MPEG header
		header = (header << 8) + *src++;
		--len;

		if ((header & 0xFFFFFF00) == 0x100)
		{
			if (fPicturePending &&
			      (header == VIDPKT_TYPE_SEQUENCE_START
			    || header == VIDPKT_TYPE_GROUP_START
			    || header == VIDPKT_TYPE_PICTURE_START
			    || header == VIDPKT_TYPE_SEQUENCE_END))
			{
				__int64 cur_pos = bytepos + ((src - 4) - pData);

				msi.pktindex  = curpkt;
				msi.pktoffset = (unsigned short)(src - pData);

				// The spec defines all of the above as
				// access units, except for sequence end,
				// which is part of the current access unit.

				// Note: Do not mark GOP as an access
				// unit because it isn't useful to us.

				msi.size = (unsigned int)(cur_pos - last_access_unit);

				video_stream_samples.Add(&msi);

				last_access_unit = -1i64;
				fPicturePending  = false;
			}

			// (a - b) - c  =  (a - c) - b

			switch (header) {

			case VIDPKT_TYPE_SEQUENCE_START:
				idx   = 0;
				bytes = 8;
				break;

			case VIDPKT_TYPE_PICTURE_START:
				// Don't add pictures until we've parsed the sequence!
				if (fFoundSequenceStart)
				{
					idx   = 0;
					bytes = 2;
				}
				break;

			case VIDPKT_TYPE_EXT_START:
				if (fFoundSequenceStart)
				{
					idx   = 0;
					bytes = 1;
				}
				break;
			
			default:
				header = 0xFFFFFFFF;
				break;
			}
		}
	}

	bytepos += src - pData;
	return true;
}


/************************************************************************
 *
 *                  MPEGFileParser
 *
 ************************************************************************/

MPEGFileParser::MPEGFileParser(InputFileMPEG2 *const pp)
	: parentPtr     (pp)
	, mFile         (pp->mFile)
	, vidstream     (0)
	, fAbort        (false)
	, hWndStatus    (NULL)
	, hWndParent    (NULL)
	, videoParser   (NULL)
	, fIsMPEG2      (false)
	, fInterleaved  (false)
	, fIsScrambled  (false)
	, lFirstCode    (-1)
	, audio_streams (0)
	, video_stream_blocks(sizeof(MPEGPacketInfo))
{
	_RPT0(_CRT_WARN, "MPEGFileParser constructed\n");

	for (int i = 0; i < 48; ++i)
	{
		audioParser[i] = NULL;
	}
	memset(packet_buffer, 0, sizeof(packet_buffer));
}


MPEGFileParser::~MPEGFileParser()
{
	_RPT0(_CRT_WARN, "MPEGFileParser destructed\n");

	if (hWndParent != NULL) EnableWindow(hWndParent, TRUE);
	if (hWndStatus != NULL) DestroyWindow(hWndStatus);

	for (int i = 47; i >= 0; --i)
	{
		delete audioParser[i];
	}
	delete videoParser;
}


int MPEGFileParser::NextStartCode()
{
	unsigned int code = 0xFFFFFFFF;
	while (!mFile.inEOF())
	{
		code = (code << 8) + mFile.inByte();
		if ((code & 0xFFFFFF00) == 0x100) return (int)code;
	}
	return 0;
}


bool MPEGFileParser::Skip(int bytes)
{
	return mFile.inSeek(mFile.inPos() + bytes);
}


int MPEGFileParser::Read()
{
	if (!mFile.inEOF()) return mFile.inByte();
	return EOF;
}


int MPEGFileParser::Read(void *buffer, int bytes)
{
	return mFile.inRead(buffer, bytes);
}


inline bool MPEGFileParser::Verify()
{
	// Check the validity of an MPEG file

	unsigned char buf[4];
	unsigned long hdr;
	long lFirstPES;
	long lFirstSeq;
    long count;
	int i;

//	fIsMPEG2 = false;
//	fInterleaved = false;
//	lFirstCode = -1;
	lFirstPES = -1;
	lFirstSeq = -1;

	// Search for system layer
	// Emotion.m2v has empty PES headers (0x1E0) but no system!

	mFile.inSeek(0);
	Read(buf, 4);
	hdr = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
	count = 4;

	while (count < SEARCH_SYSTEM_LIMIT)
	{
		if (hdr == 0x1BA)
		{
			// Verify pack header
			// (This is very robust)

			Read(buf, 4);
			count += 4;

			if  (  (buf[0] & 0xC4) == 0x44
			    && (buf[2] & 0x04) == 0x04)
			{
				//  0:  01......  (MPEG-2 pack identifier)
				//      ..xxx...  system_clock_reference_base[32..30]
				//      .....1..  marker_bit
				//      ......xx  system_clock_reference_base[29..28]
				//  1:  xxxxxxxx  system_clock_reference_base[27..20]
				//  2:  xxxxx...  system_clock_reference_base[19..15]
				//      .....1..  marker_bit
				//      ......xx  system_clock_reference_base[14..13]
				//  3:  xxxxxxxx  system_clock_reference_base[12..5]
				//  4:  xxxxx...  system_clock_reference_base[4..0]
				//      .....1..  marker_bit
				//      ......xx  system_clock_reference_extension[8..7]
				//  5:  xxxxxxx.  system_clock_reference_extension[6..0]
				//      .......1  marker_bit
				//  6:  xxxxxxxx  program_mux_rate[21..14]
				//  7:  xxxxxxxx  program_mux_rate[13..6]
				//  8:  xxxxxx..  program_mux_rate[5..0]
				//      ......1.  marker_bit
				//      .......1  marker_bit
				//  9:  xxxxx...  reserved
				//      .....xxx  pack_stuffing_length

				// We can discard the first two bytes; no start
				// code could begin with them.  However, hold on
				// to the third byte until we have verified the
				// next three bytes.  If we fail to verify, let
				// the search resume starting from that byte.

				buf[0] = buf[3];
				Read(buf + 1, 3);
				count += 3;

				//  0:  xxxxxxxx  system_clock_reference_base[12..5]
				//  1:  xxxxx...  system_clock_reference_base[4..0]
				//      .....1..  marker_bit
				//      ......xx  system_clock_reference_extension[8..7]
				//  2:  xxxxxxx.  system_clock_reference_extension[6..0]
				//      .......1  marker_bit
				//  3:  xxxxxxxx  program_mux_rate[21..14]

				if  (  (buf[1] & 4) == 4
				    && (buf[2] & 1) == 1)
				{
					// Again, we discard the first two bytes
					// but keep the third in case we fail.

					buf[0] = buf[3];
					Read(buf + 1, 3);
					count += 3;

					//  0:  xxxxxxxx  program_mux_rate[21..14]
					//  1:  xxxxxxxx  program_mux_rate[13..6]
					//  2:  xxxxxx..  program_mux_rate[5..0]
					//      ......1.  marker_bit
					//      .......1  marker_bit
					//  3:  xxxxx...  reserved
					//      .....xxx  pack_stuffing_length

					const int mux   = (buf[0] << 14)
					                + (buf[1] <<  6)
					                + (buf[2] >>  2);

					if (mux != 0 && (buf[2] & 3) == 3)
					{
						// Yes!! We have a pack.

						Skip(buf[3] & 7);

						fInterleaved = true;
						fIsMPEG2     = true;
						lFirstCode   = count - (4 + 10);
						break;
					}
				}
			}
			else if  (  (buf[0] & 0xF1) == 0x21
			         && (buf[2] & 0x01) == 0x01)
			{
				//  0:  0010....  (MPEG-1 pack identifier)
				//      ....xxx.  system_clock_reference[32..30]
				//      .......1  marker_bit
				//  1:  xxxxxxxx  system_clock_reference[29..22]
				//  2:  xxxxxxx.  system_clock_reference[21..15]
				//      .......1  marker_bit
				//  3:  xxxxxxxx  system_clock_reference[14..7]
				//  4:  xxxxxxx.  system_clock_reference[6..0]
				//      .......1  marker_bit
				//  5:  1.......  marker_bit
				//      .xxxxxxx  mux_rate[21..15]
				//  6:  xxxxxxxx  mux_rate[14..7]
				//  7:  xxxxxxx.  mux_rate[6..0]
				//      .......1  marker_bit

				buf[0] = buf[3];
				Read(buf + 1, 3);
				count += 3;

				//  0:  xxxxxxxx  system_clock_reference[14..7]
				//  1:  xxxxxxx.  system_clock_reference[6..0]
				//      .......1  marker_bit
				//  2:  1.......  marker_bit
				//      .xxxxxxx  mux_rate[21..15]
				//  3:  xxxxxxxx  mux_rate[14..7]

				if  (  (buf[1] & 0x01) == 0x01
				    && (buf[2] & 0x80) == 0x80)
				{
					// Almost there!

					buf[0] = buf[1];
					buf[1] = buf[2];
					buf[2] = buf[3];
					buf[3] = (unsigned char)Read();
					++count;

					//  0:  xxxxxxx.  system_clock_reference[6..0]
					//      .......1  marker_bit
					//  1:  1.......  marker_bit
					//      .xxxxxxx  mux_rate[21..15]
					//  2:  xxxxxxxx  mux_rate[14..7]
					//  3:  xxxxxxx.  mux_rate[6..0]
					//      .......1  marker_bit

					const long mux	= (buf[1] << 15)
									+ (buf[2] << 7)
									+ (buf[3] >> 1);

					if (mux != 0 && (buf[3] & 1) == 1)
					{
						// Yes!! We have a pack.

						fInterleaved = true;
						lFirstCode   = count - (4 + 8);
						break;
					}
				}
			}

			// Damn, we were fooled.
			// Back up four bytes and continue from there.

			hdr = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
			count -= 4;
			continue;
		}

		// .vdr files contain PES packets, but no pack headers!
		// To support these, let's say that we must find at least
		// two PES packets AND one sequence start code...

		if ((hdr >= 0x1C0 && hdr <= 0x1EF) || hdr == 0x1BD)
		{
			Read(buf, 4);
			count += 4;

			// The legal values for buf[2] and buf[3] are:
			// 10.. ....  MPEG-2
			// 01.. ....  MPEG-1
			// 0010 ....  MPEG-1
			// 0011 ....  MPEG-1
			// 0000 1111  MPEG-1
			// 1111 1111  MPEG-1

			if ((buf[0] != 0 || buf[1] != 0) &&
				(buf[2] == 0x0F || buf[2] == 0xFF ||
				(buf[2] & 0xC0) == 0x80 ||
				(buf[2] & 0xC0) == 0x40 ||
				(buf[2] & 0xE0) == 0x20)) {

				if (lFirstPES != -1) {
					if (lFirstSeq != -1) {
						fIsMPEG2 = ((buf[2] & 0xC0) == 0x80);
						lFirstCode = lFirstPES;
						fInterleaved = true;
						break;
					}
				} else {
					lFirstPES = count - 8;
				}
			}

			// Fooled; back up four bytes
			hdr = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
			count -= 4;
			continue;
		}

		// If we hit a sequence header, remember it

		if (hdr == VIDPKT_TYPE_SEQUENCE_START) {

			Read(buf, 4);
			count += 4;

			// wwwwwwww wwwwhhhh hhhhhhhh aaaarrrr
			// bbbbbbbb bbbbbbbb bb1vvvvv vvvvvxx.

			const int w = (buf[0] << 4) + (buf[1] >> 4);
			const int h = ((buf[1] & 15) << 4) + buf[2];
			const int ar = buf[3] >> 4;
			const int pr = buf[3] & 15;

			if (w > 0 && w <= MPEG_MAX_WIDTH
				&& h > 0 && h <= MPEG_MAX_HEIGHT
				&& ar != 0 && pr != 0) {

				// In this case, we can discard the whole buffer
				// because no start code can begin at any byte

				Read(buf, 4);
				count += 4;

				const int br = (buf[0] << 10) + (buf[1] << 2) + (buf[2] >> 6);

				if (br != 0 && (buf[2] & 0x20) == 0x20) {
					// Yay!

					// Let's say now that IF we've found multiple
					// valid sequence headers, AND we've searched
					// ~70000 bytes for system headers, then it's
					// fairly certain this is an elementary stream

					if (lFirstSeq != -1) {
						// We found at least one sequence
						// header prior to this one
						if (count >= 70000) {
							lFirstCode = lFirstSeq;
							break;
						}
					} else {
						lFirstSeq = count - 12;
					}
				}
			}

			// Damn, fooled again
			// Back up four bytes

			hdr = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
			count -= 4;
			continue;
		}

		if ((i = Read()) == EOF) break;
		hdr = (hdr << 8) + i;
		++count;
    }

    if (lFirstCode < 0) {
		// If we found at least one sequence header, use that
		if (lFirstSeq != -1) {
			lFirstCode = lFirstSeq;
		} else {
			return false;
		}
	}

	if (!fInterleaved) {
		// No system layer...
		// Jump back to the sequence header,
		// and check for sequence extension

		mFile.inSeek(lFirstCode + 8);

		if (NextStartCode() == VIDPKT_TYPE_EXT_START) {
			fIsMPEG2 = ((Read() >> 4) == SEQUENCE_EXTENSION_ID);
		}
	}

	// Looks good to me
	return true;
}

bool MPEGFileParser::ParsePack(int stream_id) {
	// Parse one elementary stream packet header
	// Return "false" if packet is incomplete
	MPEGPacketInfo mpi;
	long pack_length;
	int i;

	bool complete = false;

	do {	// actually not a loop

		if (fInterleaved) {
			pack_length = Read();
			pack_length = (pack_length << 8) + Read();

			// Pack length of zero is not an error
			if (pack_length == 0) {
				complete = true;
				break;
			}

			if ((stream_id >= 0x1C0 && stream_id <= 0x1EF) || stream_id == 0x1BD) {
				// MPEG Audio, MPEG Video, or private_stream_1

				// Parse PES header
				i = Read();
				--pack_length;

				if ((i & 0xC0) == 0x80) {	// MPEG-2 PES
					int data_length;

					if (i & 0x30) fIsScrambled = true;

					i = Read();				// PTS/DTS flags
					data_length = Read();	// PES_header_data_length
					pack_length -= 2;

					if (i & 0x80) {			// PTS follows
						Skip(5);
						pack_length -= 5;
						data_length -= 5;
						if (i & 0x40) {		// DTS follows
							Skip(5);
							pack_length -= 5;
							data_length -= 5;
						}
					}

					if (data_length > 0) {
						Skip(data_length);
						pack_length -= data_length;
					}

				} else {				// MPEG-1 PES

					while (i & 0x80) {
						if (--pack_length <= 0) break;
						i = Read();
					}

					if (i & 0x40) {
						Skip(1);		// P-STD_buffer size
						i = Read();
						pack_length -= 2;
					}

					if (i & 0x20) {		// PTS follows
						Skip(4);
						pack_length -= 4;
						if (i & 0x10) {	// DTS follows
							Skip(5);
							pack_length -= 5;
						}
					}
				}
            }

			if (pack_length <= 0) {
				// PES packet with no payload is not an error
				complete = (pack_length == 0);
				break;
			}

		} else {
			// Elementary video
			stream_id = 0x1E0;
			pack_length = NON_INTERLEAVED_PACK_SIZE;
		}
        
		if (stream_id == 0x1BD) {
			// private_stream_1, AC3 or LPCM
			
			i = Read();		// substream
			--pack_length;
			
			if ((i & 0xF8) == 0x80) {
				// Dolby Digital AC-3, substreams 0x80 to 0x87
				int id = parentPtr->audio_stream_list[i - 128];

				if (id == 0) {
					// map to parsers 32 to 39
					if (audioParser[i - 96] = new AC3AudioParser) {
						id = i - 95;
						parentPtr->audio_stream_list[i - 128] = id;
					}
				}

				if (id > 0) {
					--id;

			// 1 byte  = # of syncwords (i.e., access units) in packet
			// 2 bytes = 1-based offset of first syncword (0x0B77)
					Skip(3);
					pack_length -= 3;

					mpi.file_pos = (mFile.inPos() << 16) + pack_length;
					audioParser[id]->audio_stream_blocks.Add(&mpi);

					i = Read(packet_buffer, pack_length);
					complete = (i >= pack_length);
					audioParser[id]->Parse(packet_buffer, i,
						audioParser[id]->audio_stream_blocks.Length() - 1);
					pack_length = 0;
				}
			
			} else if ((i & 0xF8) == 0xA0) {
				// Uncompressed LPCM

				unsigned char buf[6];
				int frame_offset;

				Read(buf, 6);
				pack_length -= 6;

				frame_offset = (buf[1] << 8) + buf[2];

	//	0:	xxxxxxxx	number of frames
	//	1:	xxxxxxxx	offset of first frame (MSB)
	//	2:	xxxxxxxx	offset of first frame (LSB)
	//	3:	x.......	emphasis
	//		.x......	mute
	//		..x.....	reserved
	//		...xxxxx	frame number % 20
	//	4:	xx......	00=16-,01=20-,10=24-bit (11=forbidden)
	//		..xx....	00=48,01=96 KHz (others reserved?)
	//		....x...	reserved
	//		.....xxx	number of channels - 1
	//	5:	xxxxxxxx	dynamic range control

	// We can support 48K or 96K 16- or 24-bit mono or stereo

				if (	buf[0] > 0
					&&	frame_offset > 0
					&&	frame_offset < pack_length
					&&	(buf[4] & 0xC0) != 0xC0
					&&	(buf[4] & 0x26) == 0) {

					// Looks good
					// Uncompressed LPCM 0xA0 to 0xA7
					int id = parentPtr->audio_stream_list[i - 128];

					if (id == 0) {
						// map to parsers 40 to 47
						if (audioParser[i - 120] = new PCMAudioParser) {
							id = i - 119;
							parentPtr->audio_stream_list[i - 128] = id;

							// Prime him with the first header
							audioParser[i - 120]->Parse(buf, 6, 0);
						}
					}

					if (id > 0) {
						--id;

						mpi.file_pos = (mFile.inPos() << 16) + pack_length;
						audioParser[id]->audio_stream_blocks.Add(&mpi);

						i = Read(packet_buffer, pack_length);
						complete = (i >= pack_length);

						audioParser[id]->Parse(NULL, i,
							audioParser[id]->audio_stream_blocks.Length() - 1);
						pack_length = 0;
					}
				}
			}

		} else if (stream_id >= 0x1E0 && stream_id <= 0x1EF) {

			// MPEG video (only the first stream found)
			if (!vidstream) vidstream = stream_id;

			if (stream_id == vidstream) {
				mpi.file_pos = (mFile.inPos() << 16) + pack_length;
				video_stream_blocks.Add(&mpi);
				i = Read(packet_buffer, pack_length);
				complete = (i >= pack_length);
				if (!videoParser->Parse(packet_buffer, i, video_stream_blocks.Length() - 1)) {
					complete = false;	// end of sequence
				}
				pack_length = 0;
			}

		} else if (stream_id >= 0x1C0 && stream_id <= 0x1DF) {
			// MPEG audio 0xC0 to 0xDF

			int id = parentPtr->audio_stream_list[stream_id - 0x180];

			if (id == 0) {
				// map to parsers 0 to 31
				if (audioParser[stream_id - 0x1C0] = new MPEGAudioParser) {
					id = stream_id - 0x1BF;
					parentPtr->audio_stream_list[stream_id - 0x180] = id;
				}
			}

			if (id > 0) {
				--id;

				mpi.file_pos = (mFile.inPos() << 16) + pack_length;
				audioParser[id]->audio_stream_blocks.Add(&mpi);

				i = Read(packet_buffer, pack_length);
				complete = (i >= pack_length);
				audioParser[id]->Parse(packet_buffer, i,
					audioParser[id]->audio_stream_blocks.Length() - 1);
				pack_length = 0;
			}
		}

    } while (false);

	if (pack_length > 0) return Skip(pack_length);
	return complete;
}

__int64 MPEGFileParser::PacketPTS(int pkt, int stream_id) {
	unsigned long hdr;

	// Return the Presentation Time Stamp for PES
	// packet "pkt" of stream number "stream_id"

	// For video packets, stream_id >= 0x1E0 and <= 0x1EF
	// For audio packets, it is the stream number (0 to 47)

	__int64 offset;
	__int64 limit;
	int id;

	stream_id &= 0xFF;

	if (stream_id < 0xE0) {
		// Audio
		id = stream_id;

		// For private_stream_1, we need to account
		// for the following:
		// substream number is 1 byte
		// AC-3 has an additional 3 byte header
		// LPCM has an additional 6 byte header

		if (id < 32) {				// MPEG
			stream_id += 0x1C0;
			limit = 0;
		} else if (id < 40) {		// AC-3
			stream_id = 0x1BD;
			limit = -4;
		} else {					// LPCM
			stream_id = 0x1BD;
			limit = -7;
		}

		limit += parentPtr->audio_packet_list[id][pkt].file_pos >> 16;

	} else {

		// Video
		limit = parentPtr->video_packet_list[pkt].file_pos >> 16;
		stream_id += 0x100;
	}

	// The largest possible PES packet header is 264
	// bytes (cf. ISO/IEC 13818-1), therefore we start
	// searching at least that far behind the PES data:

	// bytes      description
	//  3         packet_start_code_prefix
	//  1         stream_id
	//  2         PES_packet_length
	//  2         miscellaneous flags
	//  1         PES_header_data_length
	//  0..255	  remainder of PES header

	// After parsing each PES header, we'll know we have
	// the right PES packet if our position exactly matches
	// the sample's starting position.

	offset = limit - 264;
	if (offset < 0) offset = 0;

	mFile.inSeek(offset);
	hdr = 0xFFFFFFFF;

	while (mFile.inPos() < limit) {

		// Just blaze through the bytes
		hdr = (hdr << 8) + Read();

		if (hdr == (unsigned long)stream_id) {
			// This may be the packet we want...
			// Parse the PTS (if any) in the PES header
			__int64 pts;

			Skip(2);	// pack length
			int ptsflags = Read();

			if ((ptsflags & 0xC0) == 0x80) {	// MPEG-2
				int data_length;

				ptsflags = Read();
				data_length = Read();

				if (ptsflags & 0x80) {
			// 0010ppp1 pppppppp ppppppp1 pppppppp ppppppp1
					pts = (Read() & 0x0E) >> 1;
					pts = (pts << 8) + Read();
					pts = (pts << 7) + (Read() >> 1);
					pts = (pts << 8) + Read();
					pts = (pts << 7) + (Read() >> 1);
					data_length -= 5;
				}
				Skip(data_length);

			} else {
				// MPEG-1
				while (ptsflags & 0x80) {
					ptsflags = Read();
					// Don't endless loop
					if (mFile.inPos() >= limit) break;
				}
				if (ptsflags & 0x40) {
					Read();
					ptsflags = Read();
				}
				if (ptsflags & 0x20) {
					pts = (ptsflags & 0x0E) >> 1;
					pts = (pts << 8) + Read();
					pts = (pts << 7) + (Read() >> 1);
					pts = (pts << 8) + Read();
					pts = (pts << 7) + (Read() >> 1);
					if (ptsflags & 0x10) Skip(5);
					ptsflags = 0x80;
				}
			}

			// Finally, if this is the PES packet we want,
			// our position will exactly match the stored
			// position of the PES data for the sample:

			if (mFile.inPos() == limit) {
				if (ptsflags & 0x80) return pts;
				break;	// No PTS on this packet
			}
		}
	}

	return -1;	// PTS not found
}

inline void MPEGFileParser::DoFixUps()
{
	MPEGSampleInfo *msi;
	unsigned int index;
	unsigned int limit;
	unsigned int i;
	int offset;
	int id;

	// Determine the packets in which each video and audio
	// sample originates, and the offsets in those packets.

	// The parser has stored the packet and offset at which
	// the END of the sample was found. (In the case of video,
	// it's actually 4 bytes past the end of the sample.)

	for (i = 0; i < parentPtr->vframes; ++i) {
		msi = &(parentPtr->video_sample_list[i]);
		index = msi->pktindex;
		offset = msi->pktoffset - (msi->size + 4);

		if (offset < 0) {
			// Sample began in an earlier packet
			while (index) {
				--index;
				offset += (long)(parentPtr->video_packet_list[index].file_pos & 0xFFFF);
				if (offset >= 0) {
					msi->pktindex = index;
					msi->pktoffset = (unsigned short)offset;
					break;
				}
			}
		} else {
			// Sample begins in this packet
			msi->pktoffset = (unsigned short)offset;
		}
	}

	// Note: LPCM doesn't need fixing up

	for (id = 0; id < 40; ++id) {
		for (i = 0; i < parentPtr->aframes[id]; ++i) {
			msi = &(parentPtr->audio_sample_list[id][i]);
			index = msi->pktindex;
			offset = msi->pktoffset - msi->size;

			if (offset < 0) {
				// Sample began in an earlier packet
				while (index) {
					--index;
					offset += (long)(parentPtr->audio_packet_list[id][index].file_pos & 0xFFFF);
					if (offset >= 0) {
						msi->pktindex = index;
						msi->pktoffset = (unsigned short)offset;
						break;
					}
				}
			} else {
				// Sample begins in this packet
				msi->pktoffset = (unsigned short)offset;
			}
		}
	}

	// Now hunt for associated Presentation Time Stamps
	// They are required at least every 0.7 seconds;
	// We'll give 'em about 2 seconds

	parentPtr->vfirstPTS = 0;
	for (id = 0; id < 48; ++id) {
		parentPtr->afirstPTS[id] = 0;
	}
	if (!fInterleaved) return;

	// Find the first I-frame with PTS

	limit = (int)((2.0 * (double)videoParser->mFrameRate.mNumerator) /
		(double)videoParser->mFrameRate.mDenominator + 0.5);

	if (limit > parentPtr->vframes) limit = parentPtr->vframes;

	for (i = 0; i < limit; ++i) {
		const MPEGSampleInfo *msi = &(parentPtr->video_sample_list[i]);

		if (msi->frame_type == MPEG_FRAME_TYPE_I) {
			__int64 pts = PacketPTS(msi->pktindex, vidstream);
			if (pts != -1) {
				// Got it!
				// Remove all frames prior to this one
				if (i > 0) {
					for (unsigned int j = i; j < parentPtr->vframes; ++j) {
						parentPtr->video_sample_list[j - i] =
						parentPtr->video_sample_list[j];
					}
				}
				parentPtr->vframes -= i;
				parentPtr->vfirstPTS = pts;
				break;
			}
		}
	}

	// Find the first audio frame with PTS

	for (id = 0; id < 48; ++id) {

		if (parentPtr->aframes[id] > 0) {
			int srate;
			long lFramesPerSec;
			unsigned short header = parentPtr->audio_sample_list[id][0].header;

			if (id < 32) {
				MPEGAudioCompacted hdr(header);
				srate = (long)hdr.GetSamplingRateHz();
				if (hdr.GetLayer() == 1) {
					lFramesPerSec = (long)((double)srate / 384.0 + 0.5);
				} else {
					lFramesPerSec = (long)((double)srate / 1152.0 + 0.5);
				}
			} else if (id < 40) {
				if (ac3_compact_info(header, &srate, NULL, NULL)) {
					lFramesPerSec = (long)((double)srate / 1536.0 + 0.5);
				}
			} else {
				srate = (header & 0x30)? 96000: 48000;
				lFramesPerSec = (long)((double)srate / 1536.0 + 0.5);
			}

			limit = lFramesPerSec * 2;
			if (limit > parentPtr->aframes[id]) limit = parentPtr->aframes[id];

			for (i = 0; i < limit; ++i) {
				const MPEGSampleInfo *msi = &(parentPtr->audio_sample_list[id][i]);
				__int64 pts = PacketPTS(msi->pktindex, id);
				if (pts != -1) {
					// Got it!
					// Remove all frames prior to this one
					if (i > 0) {
						for (unsigned int j = i; j < parentPtr->aframes[id]; ++j) {
							parentPtr->audio_sample_list[id][j - i] =
								parentPtr->audio_sample_list[id][j];
						}
						parentPtr->aframes[id] -= i;
					}
					parentPtr->afirstPTS[id] = pts;
					break;
				}
			}
		}
	}
}

void MPEGFileParser::Parse(HMODULE hModule) {
	int i;

	// Verify the MPEG file.  If successful,
	// This sets up the following variables:
	//		lFirstCode
	//		fInterleaved
	//		fIsMPEG2

	if (!Verify()) {
		parentPtr->mContext.mpCallbacks->SetError("Couldn't detect file type");
		return;
	}

	if (!fInterleaved && lFirstCode >= 4) {
		// Beware of Transport Streams!
		// These will appear valid, however:
		//	- fInterleaved will be false
		//	- lFirstCode will be at least 4
		//	- The first byte will be 0x47

		mFile.inSeek(0);
		if (Read() == 0x47) {
			MessageBox(NULL, "File matches the profile of a "
				"Transport Stream and may not decode correctly.",
				"Warning", MB_ICONEXCLAMATION | MB_SETFOREGROUND | MB_TASKMODAL | MB_OK);
		}
	}

	// Begin file parsing!  This is a royal bitch!

	videoParser = new MPEGVideoParser;

	mFile.inSeek(lFirstCode);

	// Get a handle to the main VirtualDub window
	EnumThreadWindows(GetCurrentThreadId(), FindContextWindow, (LPARAM)&hWndParent);
	if (hWndParent == NULL) hWndParent = GetActiveWindow();
	if (hWndParent != NULL) EnableWindow(hWndParent, FALSE);

	if ((parentPtr->uiFlags & IVDXInputFileDriver::kFlagQuiet) == 0) {
		// pop up the dialog...
		hWndStatus = CreateDialogParamA(hModule,
			MAKEINTRESOURCEA(IDD_PROGRESS), hWndParent,
			ParseDialogProc, (LPARAM)this);
	}

	do {
		if (fInterleaved) {
			i = NextStartCode();

			if (i >= 0x1BB) {
				if (!ParsePack(i)) break;

			} else {
				if (i == 0x1B9) {		// ISO 11172 end code
					// Don't break unless we have found video
					if (videoParser->video_stream_samples.Length() > 0) break;
				}
				if (i == 0) break;		// end of file
			}

		} else {

			// Elementary video
			if (!ParsePack(0x1E0)) break;
		}

		if (!guiDlgMessageLoop(hWndStatus)) break;

	} while (!fAbort);

	parentPtr->fUserAborted = fAbort;

	// Finish off any final LPCM audio packets
	for (i = 40; i < 48; ++i) {
		if (audioParser[i] != NULL) {
			audioParser[i]->Finish();
		}
	}

    // Construct stream and packet lookup tables, and
	// transfer them to the parent (I don't like this, but...)

	parentPtr->video_packet_list = (MPEGPacketInfo *)
        video_stream_blocks.MakeArray();
    parentPtr->vpackets = video_stream_blocks.Length();

	if (fInterleaved) {
		int id;

		for (i = 0; i < 96; ++i) {
			if (parentPtr->audio_stream_list[i] != 0) {
				bool valid = false;
				id = parentPtr->audio_stream_list[i] - 1;

				if (audioParser[id]) {
					// Only add streams which are not empty
					if (audioParser[id]->audio_stream_samples.Length()) {
						parentPtr->audio_packet_list[id] = (MPEGPacketInfo *)
							audioParser[id]->audio_stream_blocks.MakeArray();
						parentPtr->apackets[id] = audioParser[id]->audio_stream_blocks.Length();

						parentPtr->audio_sample_list[id] = (MPEGSampleInfo *)
							audioParser[id]->audio_stream_samples.MakeArray();
						parentPtr->aframes[id] = audioParser[id]->audio_stream_samples.Length();
						valid = true;
					}
				}
				if (!valid) parentPtr->audio_stream_list[i] = 0;
			}
		}
	}

    parentPtr->video_sample_list = (MPEGSampleInfo *)
        videoParser->video_stream_samples.MakeArray();
    parentPtr->vframes = videoParser->video_stream_samples.Length();

	DoFixUps();

	// Transfer the rest of the info
	parentPtr->width = videoParser->width;
	parentPtr->height = videoParser->height;
	parentPtr->mFrameRate = videoParser->mFrameRate;
	parentPtr->seq_ext = videoParser->seq_ext;
	parentPtr->progressive_sequence = videoParser->progressive_sequence;
	parentPtr->aspect_ratio = videoParser->aspect_ratio_info;
	parentPtr->matrix_coefficients = videoParser->matrix_coefficients;
	parentPtr->display_width = videoParser->display_width;
	parentPtr->display_height = videoParser->display_height;

//	parentPtr->fInterleaved = this->fInterleaved;
	parentPtr->lFirstCode = this->lFirstCode;

	if (fInterleaved)
		parentPtr->system = fIsMPEG2? 2: 1;
	else
		parentPtr->system = 0;
}

BOOL CALLBACK MPEGFileParser::FindContextWindow(HWND hwnd, LPARAM lParam) {
	if (GetParent(hwnd) == NULL) {
		*(HWND *)lParam = hwnd;
		return FALSE;
	}
	return TRUE;
}

bool MPEGFileParser::guiDlgMessageLoop(HWND hDlg) {
	MSG msg;

	while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) return false;

		if (!hDlg || !IsWindow(hDlg) || !IsDialogMessageA(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}

	return true;
}

INT_PTR CALLBACK MPEGFileParser::ParseDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	MPEGFileParser *thisPtr = (MPEGFileParser *)GetWindowLongPtrA(hDlg, DWLP_USER);
	char buf[64];

	switch(uMsg) {

	case WM_INITDIALOG:
		{
			SetWindowLongPtrA(hDlg, DWLP_USER, lParam);
			thisPtr = (MPEGFileParser *)lParam;
			InputOptionsMPEG2::InitDialogTitle(hDlg, NULL);

	// Important note:
	// The value of "fIsMPEG2" depends on "fInterleaved."
	// If fInterleaved is true, fIsMPEG2 refers to the system layer
	// If fInterleaved is false, fIsMPEG2 refers to the video stream

			if (thisPtr->fInterleaved)
			{
				_snprintf(buf, sizeof(buf),
					"Parsing interleaved MPEG-%i file",
					(thisPtr->fIsMPEG2)? 2: 1);
			}
			else
			{
				_snprintf(buf, sizeof(buf),
					"Parsing MPEG-%i video file",
					(thisPtr->fIsMPEG2)? 2: 1);
			}

			buf[sizeof(buf) - 1] = '\0';
			SetDlgItemTextA(hDlg, IDC_STATIC_MESSAGE, (LPSTR)buf);

			SendMessageA(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 16384));
			SetTimer(hDlg, 1, 250, (TIMERPROC)NULL);
			ShowWindow(hDlg, SW_SHOW);

			return TRUE;
		}

	case WM_TIMER:
		{
			__int64 file_len = thisPtr->mFile.inSize();
			__int64 file_cpos = thisPtr->mFile.inPos();

			SendMessageA(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETPOS,
				(WPARAM)((file_cpos * 16384) / file_len), 0);

			_snprintf(buf, sizeof(buf),
				"%I64iK of %I64iK",
				file_cpos / 1024,
				(file_len + 512) / 1024);

			buf[sizeof(buf) - 1] = '\0';

			SendDlgItemMessageA(hDlg, IDC_CURRENT_VALUE, WM_SETTEXT, 0, (LPARAM)buf);
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL) thisPtr->fAbort = true;

		return TRUE;
	}

	return FALSE;
}

