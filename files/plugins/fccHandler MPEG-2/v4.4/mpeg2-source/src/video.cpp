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

// MPEG-2 video source (IVDXVideoSource)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "vd2/plugin/vdinputdriver.h"
#include "Unknown.h"

#include "infile.h"
#include "append.h"
#include "InputFileMPEG2.h"

// Support Direct Stream Copy?
//#define SUPPORT_DSC

#include "video.h"
#include "MPEG2Decoder.h"
#include "decode.h"

#include <crtdbg.h>


///////////////////////////////////////////////////////////////////////////////

VideoSourceMPEG2::VideoSourceMPEG2(InputFileMPEG2* const pp)
	: parentPtr(pp)
{
	parentPtr->AddRef();

_RPT0(_CRT_WARN, "VideoSourceMPEG2 constructed\n");

	read_buffer2    = NULL;
	read_buffer1    = new unsigned char[parentPtr->largest_framesize * 2];

	if (read_buffer1 != NULL)
		read_buffer2 = read_buffer1 + parentPtr->largest_framesize;

	buffer1_frame   = -1;
	buffer2_frame   = -1;

	mSampleFirst    = 0;
	mSampleLast     = parentPtr->fields;
	
#ifdef SUPPORT_DSC
	DirectFormat    = NULL;
	DirectTested    = false;
	DirectFormatLen = 0;
#endif
}


VideoSourceMPEG2::~VideoSourceMPEG2()
{
_RPT0(_CRT_WARN, "VideoSourceMPEG2 destructed\n");
#ifdef SUPPORT_DSC
	delete[] DirectFormat;
#endif
	delete[] read_buffer1;
	parentPtr->Release();
}


int VDXAPIENTRY VideoSourceMPEG2::AddRef()
{
	return vdxunknown<IVDXStreamSource>::AddRef();
}


int VDXAPIENTRY VideoSourceMPEG2::Release()
{
	return vdxunknown<IVDXStreamSource>::Release();
}


void *VDXAPIENTRY VideoSourceMPEG2::AsInterface(uint32 iid)
{
	if (iid == IVDXVideoSource::kIID)
		return static_cast<IVDXVideoSource *>(this);

	if (iid == IVDXStreamSource::kIID)
		return static_cast<IVDXStreamSource *>(this);

	return NULL;

//	return vdxunknown<IVDXStreamSource>::AsInterface(iid);
}


void VDXAPIENTRY VideoSourceMPEG2::GetStreamSourceInfo(VDXStreamSourceInfo& srcInfo)
{
	srcInfo.mSampleRate  = parentPtr->mFrameRate;
	srcInfo.mSampleCount = parentPtr->fields;

	switch (parentPtr->aspect_ratio) {

		// MPEG-1 SAR
		// See ISO/IEC 11172-2 section 2.4.3.2, pel_aspect_ratio

		case 1:		// (1.0000) VGA	etc.
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 10000;
			break;

		case 2:		// (0.6735)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 6735;
			break;

		case 3:		// (0.7031) 16:9, 625 line
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 7031;
			break;

		case 4:		// (0.7615)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 7615;
			break;

		case 5:		// (0.8055)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 8055;
			break;

		case 6:		// (0.8437) 16:9, 525 line
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 8437;
			break;

		case 7:		// (0.8935)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 8935;
			break;

		case 8:		// (0.9157) CCIR601, 625 line
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 9157;
			break;

		case 9:		// (0.9815)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 9815;
			break;

		case 10:	// (1.0255)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 10255;
			break;

		case 11:	// (1.0695)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 10695;
			break;

		case 12:	// (1.0950) CCIR601, 525 line
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 10950;
			break;

		case 13:	// (1.1575)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 11575;
			break;

		case 14:	// (1.2015)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 12015;
			break;


		// MPEG-2 SAR/DAR
		// See ISO/IEC 13818-2 section 6.3.3, aspect_ratio_information

		// SAR = DAR * (horizontal_size / vertical_size)

		case 17:	// (1.0) SAR (Square Sample)
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 10000;
			break;

		case 18:	// (4:3) DAR
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator =
				(uint32)((3.0 / 4.0) * 10000.0 *
					((double)parentPtr->width /
					(double)parentPtr->height) + 0.5
				);
			break;

		case 19:	// (16:9) DAR
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator =
				(uint32)((9.0 / 16.0) * 10000.0 *
					((double)parentPtr->width /
					(double)parentPtr->height) + 0.5
				);
			break;

		case 20:	// (2.21:1) DAR
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator =
				(uint32)((1.0 / 2.21) * 10000.0 *
					((double)parentPtr->width /
					(double)parentPtr->height) + 0.5
				);
			break;

		default:
			srcInfo.mPixelAspectRatio.mNumerator   = 10000;
			srcInfo.mPixelAspectRatio.mDenominator = 10000;
			break;
	};
}


bool VDXAPIENTRY VideoSourceMPEG2::Read(sint64 lStart64, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead)
{
	const MPEGSampleInfo *m1, *m2;
	long lStart, f1, f2, len;
	int cadence;

//_RPT1(_CRT_WARN, "Read(start = %i)\n", (int)lStart64);

	if (lStart64 < mSampleFirst || lStart64 >= mSampleLast)
	{
		if (lSamplesRead) *lSamplesRead = 0;
		if (lBytesRead) *lBytesRead = 0;
		return true;
	}

	lStart = (long)lStart64;

	f1 = parentPtr->video_field_map[lStart];
	cadence = f1 & 1;
	f1 >>= 1;

	if (cadence != 0 && lStart64 > mSampleFirst)
	{
		f2 = f1;
		f1 = parentPtr->video_field_map[lStart - 1] >> 1;

		m1 = &(parentPtr->video_sample_list[f1]);
		m2 = &(parentPtr->video_sample_list[f2]);

		len = m1->size + m2->size;
	}
	else
	{
		m1 = &(parentPtr->video_sample_list[f1]);
		len = m1->size;
	}

	// We only support reading one frame
	// at a time; "lCount" is never used.

	if (lpBuffer == NULL || cbBuffer < (uint32)len)
	{
		if (lSamplesRead) *lSamplesRead = 1;
		if (lBytesRead) *lBytesRead = len;
		return (lpBuffer == NULL);
	}

	if (cadence)
	{
		if (buffer1_frame == f1)
		{
			memcpy(lpBuffer, read_buffer1, m1->size);
			if (buffer2_frame != f2)
			{
				parentPtr->ReadSample(read_buffer2, f2, m2->size, 0);
				buffer2_frame = f2;
			}
			memcpy((char *)lpBuffer + m1->size, read_buffer2, m2->size);
		}
		else if (buffer2_frame == f1)
		{
			memcpy(lpBuffer, read_buffer2, m1->size);
			if (buffer1_frame != f2)
			{
				parentPtr->ReadSample(read_buffer1, f2, m2->size, 0);
				buffer1_frame = f2;
			}
			memcpy((char *)lpBuffer + m1->size, read_buffer1, m2->size);
		}
		else if (buffer1_frame == f2)
		{
			parentPtr->ReadSample(read_buffer2, f1, m1->size, 0);
			memcpy(lpBuffer, read_buffer2, m1->size);
			buffer2_frame = f1;
			memcpy((char *)lpBuffer + m1->size, read_buffer1, m2->size);
		}
		else if (buffer2_frame == f2)
		{
			parentPtr->ReadSample(read_buffer1, f1, m1->size, 0);
			memcpy(lpBuffer, read_buffer1, m1->size);
			buffer1_frame = f1;
			memcpy((char *)lpBuffer + m1->size, read_buffer2, m2->size);
		}
		else
		{
			parentPtr->ReadSample(read_buffer1, f1, m1->size, 0);
			parentPtr->ReadSample(read_buffer2, f2, m2->size, 0);
			buffer1_frame = f1;
			buffer2_frame = f2;
			memcpy(lpBuffer, read_buffer1, m1->size);
			memcpy((char *)lpBuffer + m1->size, read_buffer2, m2->size);
		}
	}
	else
	{
		if (buffer1_frame == f1)
		{
			memcpy(lpBuffer, read_buffer1, m1->size);
		}
		else if (buffer2_frame == f1)
		{
			memcpy(lpBuffer, read_buffer2, m1->size);
		}
		else
		{
			parentPtr->ReadSample(read_buffer2, f1, m1->size, 0);
			memcpy(lpBuffer, read_buffer2, m1->size);
			buffer2_frame = f1;
		}
	}

	if (lSamplesRead) *lSamplesRead = 1;
	if (lBytesRead) *lBytesRead = len;
	return true;
}


const void *VDXAPIENTRY VideoSourceMPEG2::GetDirectFormat()
{
#ifdef SUPPORT_DSC
	if (!DirectTested && parentPtr->video_sample_list != NULL)
	{
		// Create format for Direct Stream Copy.
		// (Attempt this only once.)
		DirectTested = true;
		
		// Read in the first video frame; this will have
		// the sequence_header and sequence_extension (if any).
		
		parentPtr->ReadSample(read_buffer1, 0, parentPtr->video_sample_list[0].size, 0);
		
		if (*(DWORD *)read_buffer1 == 0xB3010000)
		{
			const unsigned char *src = read_buffer1 + 4;
			unsigned long hdr = 0xFFFFFFFF;
			long remains = parentPtr->video_sample_list[0].size - 4;
			bool valid = false;
			
			// Locate end of sequence header
			while (remains > 0)
			{
				hdr = (hdr << 8) + *src++;
				--remains;
				if ((hdr & 0xFFFFFF00) == 0x100)
				{
					valid = true;
					break;
				}
			}

			if (parentPtr->seq_ext)
			{
				valid = false;
				// Locate end of sequence extension
				if (hdr == 0x1B5 && remains > 0)
				{
					if ((*src >> 4) == 1)
					{
						hdr = 0xFFFFFFFF;
						do {
							hdr = (hdr << 8) + *src++;
							--remains;
							if ((hdr & 0xFFFFFF00) == 0x100)
							{
								valid = true;
								break;
							}
						} while (remains > 0);
					}
				}
			}

			if (valid)
			{
				remains = (src - 4) - read_buffer1;
			
				DirectFormat = new unsigned char[sizeof(BITMAPINFOHEADER) + remains];
				if (DirectFormat != NULL)
				{
					BITMAPINFOHEADER *bm = (BITMAPINFOHEADER *)DirectFormat;
					DirectFormatLen = sizeof(BITMAPINFOHEADER) + remains;

					bm->biSize          = DirectFormatLen;
					bm->biWidth         = parentPtr->width;
					bm->biHeight        = parentPtr->height;
					bm->biPlanes        = 1;
					bm->biBitCount      = 24;
					bm->biCompression   = (parentPtr->seq_ext != 0)? '2GPM': '1GPM';
					bm->biSizeImage     = ((bm->biWidth * 3 + 3) & -4) * bm->biHeight;
					bm->biXPelsPerMeter = 0;
					bm->biYPelsPerMeter = 0;
					bm->biClrUsed       = 0;
					bm->biClrImportant  = 0;
					
					memcpy(DirectFormat + sizeof(BITMAPINFOHEADER), read_buffer1, remains);
				}
			}
		}
	}
	
	return DirectFormat;
#else
	return NULL;
#endif
}


int VDXAPIENTRY VideoSourceMPEG2::GetDirectFormatLen()
{
#ifdef SUPPORT_DSC
	if (!DirectTested) GetDirectFormat();
	return DirectFormatLen;
#else
	return 0;
#endif
}


IVDXStreamSource::ErrorMode VDXAPIENTRY VideoSourceMPEG2::GetDecodeErrorMode()
{
	return IVDXStreamSource::kErrorModeReportAll;
}


void VDXAPIENTRY VideoSourceMPEG2::SetDecodeErrorMode(IVDXStreamSource::ErrorMode mode)
{
}


bool VDXAPIENTRY VideoSourceMPEG2::IsDecodeErrorModeSupported(IVDXStreamSource::ErrorMode mode)
{
	return mode == IVDXStreamSource::kErrorModeReportAll;
}


bool VDXAPIENTRY VideoSourceMPEG2::IsVBR()
{
	return false;
}


// sample = (ms * dwRate) / (dwScale * 1000)
// sample * dwScale * 1000 = (ms * dwRate)
// ms = (sample * dwScale * 1000) / dwRate

sint64 VDXAPIENTRY VideoSourceMPEG2::TimeToPositionVBR(sint64 us)
{
	return (sint64)(0.5 +
		((double)parentPtr->mFrameRate.mNumerator * (double)us)
		/ ((double)parentPtr->mFrameRate.mDenominator * 1000000.0)
	);
}


sint64 VDXAPIENTRY VideoSourceMPEG2::PositionToTimeVBR(sint64 samples)
{
	return (sint64)(0.5 +
		((double)parentPtr->mFrameRate.mDenominator * (double)samples * 1000000.0)
		/ (double)parentPtr->mFrameRate.mNumerator
	);
}


void VDXAPIENTRY VideoSourceMPEG2::GetVideoSourceInfo(VDXVideoSourceInfo& info)
{
	info.mFlags         = VDXVideoSourceInfo::kFlagNone;
	info.mWidth         = parentPtr->width;
	info.mHeight        = parentPtr->height;
	info.mDecoderModel  = VDXVideoSourceInfo::kDecoderModelCustom;
}


bool VDXAPIENTRY VideoSourceMPEG2::CreateVideoDecoderModel(IVDXVideoDecoderModel **ppModel)
{
	*ppModel = new VideoDecoderModelMPEG2(parentPtr);
	if (*ppModel == NULL) return false;
	(*ppModel)->AddRef();
	return true;
}


bool VDXAPIENTRY VideoSourceMPEG2::CreateVideoDecoder(IVDXVideoDecoder **ppDecoder)
{
	VideoDecoderMPEG2 *pMpeg2;
	*ppDecoder = NULL;

	pMpeg2 = new VideoDecoderMPEG2(parentPtr);
	if (pMpeg2 == NULL) return false;

	if (!pMpeg2->Init())
	{
		delete pMpeg2;
		return false;
	}
	*ppDecoder = static_cast<IVDXVideoDecoder *>(pMpeg2);

	(*ppDecoder)->AddRef();
	return true;
}


void VDXAPIENTRY VideoSourceMPEG2::GetSampleInfo(sint64 sample_num, VDXVideoFrameInfo& frameInfo)
{
	// Defaults if we fail
	frameInfo.mBytePosition = -1;
	frameInfo.mFrameType = kVDXVFT_Null;
	frameInfo.mTypeChar = ' ';

	if (sample_num >= mSampleFirst && sample_num < mSampleLast)
	{
		long f1 = parentPtr->video_field_map[(uint32)sample_num];

		if (f1 & 1)
		{
			// Reconstructed frames are always interlaced and do not
			// have a byte position.  For the purpose of this function
			// they are analogous to bi-directional frames, since they
			// depend on two other frames but aren't neeeded in the
			// decoding process.

			frameInfo.mFrameType = kVDXVFT_Bidirectional;
			frameInfo.mTypeChar  = 'r';
		}
		else
		{
			f1 >>= 1;
			const MPEGSampleInfo& msi = parentPtr->video_sample_list[f1];

			switch (msi.frame_type)
			{
				case MPEG_FRAME_TYPE_I:
					frameInfo.mFrameType = kVDXVFT_Independent;
					frameInfo.mTypeChar = (msi.bitflags & PICTUREFLAG_PF)? 'I': 'i';
					frameInfo.mBytePosition = parentPtr->GetSamplePosition(f1, 0);
					break;
				case MPEG_FRAME_TYPE_P:
					frameInfo.mFrameType = kVDXVFT_Predicted;
					frameInfo.mTypeChar  = (msi.bitflags & PICTUREFLAG_PF)? 'P': 'p';
					frameInfo.mBytePosition = parentPtr->GetSamplePosition(f1, 0);
					break;
				case MPEG_FRAME_TYPE_B:
					frameInfo.mFrameType = kVDXVFT_Bidirectional;
					frameInfo.mTypeChar  = (msi.bitflags & PICTUREFLAG_PF)? 'B': 'b';
					frameInfo.mBytePosition = parentPtr->GetSamplePosition(f1, 0);
					break;
			}
		}
	}
}


bool VDXAPIENTRY VideoSourceMPEG2::IsKey(sint64 sample_num)
{
	if (sample_num >= mSampleFirst && sample_num < mSampleLast)
	{
		long f1 = parentPtr->video_field_map[(uint32)sample_num];

		if ((f1 & 1) == 0)
		{
			if (parentPtr->video_sample_list[f1 >> 1].frame_type == MPEG_FRAME_TYPE_I)
			{
				return true;
			}
		}
	}

	return false;
}


sint64 VDXAPIENTRY VideoSourceMPEG2::GetFrameNumberForSample(sint64 sample_num)
{
	return sample_num;
}


sint64 VDXAPIENTRY VideoSourceMPEG2::GetSampleNumberForFrame(sint64 frame_num)
{
	return frame_num;
}


sint64 VDXAPIENTRY VideoSourceMPEG2::GetRealFrame(sint64 frame_num)
{
	return frame_num;
}


sint64 VDXAPIENTRY VideoSourceMPEG2::GetSampleBytePosition(sint64 sample_num)
{
	if (sample_num >= mSampleFirst && sample_num < mSampleLast)
	{
		long f1 = parentPtr->video_field_map[(uint32)sample_num];

		// Do not return a byte position for reconstructed frames.
		if ((f1 & 1) == 0)
		{
			return parentPtr->GetSamplePosition(f1 >> 1, 0);
		}
	}

	return -1;
}
