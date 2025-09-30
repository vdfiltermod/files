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

// IVDXVideoDecoder and IVDXVideoDecoderModel interfaces

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
	#define _CRT_SECURE_NO_WARNINGS
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "vd2/plugin/vdinputdriver.h"
#include "Unknown.h"

#include "infile.h"
#include "append.h"
#include "InputFileMPEG2.h"

#include "MPEG2Decoder.h"
#include "decode.h"

#include <crtdbg.h>

#define MPEG_BUFFER_BIDIRECTIONAL	(0)
#define MPEG_BUFFER_BACKWARD		(1)
#define MPEG_BUFFER_FORWARD			(2)
#define MPEG_BUFFER_AUXILIARY		(3)

// Chroma formats
#define CHROMA420		1
#define CHROMA422		2
#define CHROMA444		3

//////////////////////////////////////////////////////////////////////////
//
//
//						VideoDecoderMPEG2
//
//
//////////////////////////////////////////////////////////////////////////

VideoDecoderMPEG2::VideoDecoderMPEG2(InputFileMPEG2 *const pp)
	: parentPtr     (pp)
	, mpDecoder     (NULL)
	, pMemBlock     (NULL)
	, mFrameBuffer  (NULL)
{
_RPT0(_CRT_WARN, "VideoDecoderMPEG2 constructed\n");

	parentPtr->AddRef();
}


bool VideoDecoderMPEG2::Init()
{
	// Init() must be called exactly once when the class is created!
	// This ensures that we are good to go from now on.

	unsigned int align;

	const int w = parentPtr->width;
	const int h = parentPtr->height;

	if (w <= 0 || h <= 0) goto Abort;

	pMemBlock = new unsigned char[w * h * 4 + 16];
	if (pMemBlock == NULL) goto Abort;
	mFrameBuffer = pMemBlock;

	align = (unsigned int)((DWORD_PTR)mFrameBuffer & 15);
	if (align != 0) mFrameBuffer += (16 - align);
	memset(mFrameBuffer, 0, w * h * 4);

	mPixmap.pitch = w * 3 + 3;
	align = (unsigned int)((DWORD_PTR)mPixmap.pitch & 3);
	if (align != 0) mPixmap.pitch += (4 - align);

	mPixmap.data    = mFrameBuffer + (h - 1) * mPixmap.pitch;
	mPixmap.palette = NULL;
	mPixmap.w       = w;
	mPixmap.h       = h;
	mPixmap.pitch   = -mPixmap.pitch;
	mPixmap.format  = nsVDXPixmap::kPixFormat_RGB888;
	mPixmap.data2   = NULL;
	mPixmap.pitch2  = 0;
	mPixmap.data3   = NULL;
	mPixmap.pitch3  = 0;

	mpDecoder = CreateMPEG2Decoder();
	if (mpDecoder == NULL) goto Abort;
	if (!mpDecoder->Init(w, h, parentPtr->seq_ext)) goto Abort;

	mPrevFrame = -1;
	return true;

Abort:
	return false;
}


VideoDecoderMPEG2::~VideoDecoderMPEG2()
{
_RPT0(_CRT_WARN, "VideoDecoderMPEG2 destructed\n");

	if (mpDecoder != NULL) mpDecoder->Destruct();
	delete[] pMemBlock;

	parentPtr->Release();
}


const void *VDXAPIENTRY VideoDecoderMPEG2::DecodeFrame(
	const void *inputBuffer, uint32 data_len,
	bool is_preroll, sint64 streamFrame, sint64 targetFrame)
{
	const MPEGSampleInfo *m1, *m2;
	int f1, f2, sframe;
	int buffer, b1, b2;

//	_RPT3(_CRT_WARN, "DecodeFrame(is_preroll = %i, stream = %i, target = %i)\n",
//	(int)is_preroll, (int)streamFrame, (int)targetFrame);

	sframe = (int)streamFrame;
	if (sframe < 0) sframe = (int)targetFrame;
	if ((unsigned int)sframe >= parentPtr->fields) return mFrameBuffer;

	// Update decoder acceleration flags
	f1 = (int)parentPtr->mContext.mpCallbacks->GetCPUFeatureFlags();
	f2 = IMPEG2Decoder::eAccel_None;
	if (f1 & kVDXCPUF_MMX ) f2 |= IMPEG2Decoder::eAccel_MMX;
	if (f1 & kVDXCPUF_ISSE) f2 |= IMPEG2Decoder::eAccel_ISSE;
	mpDecoder->EnableAcceleration(f2);

	// Do we use "matrix coefficients" for YCbCr to RGB?
	mpDecoder->EnableMatrixCoefficients(parentPtr->fAllowMatrix);

	f1 = parentPtr->video_field_map[sframe];

	if (is_preroll) {
		// Just decode, but skip B-frames

		// If this is a laced frame, we actually want the second!
		if ((f1 & 1) && sframe > 0) {
			f2 = f1 >> 1;
			f1 = parentPtr->video_field_map[sframe - 1] >> 1;

			m1 = &(parentPtr->video_sample_list[f1]);
			m2 = &(parentPtr->video_sample_list[f2]);

			buffer = mpDecoder->GetFrameBuffer(f2);

			if (m2->frame_type != MPEG_FRAME_TYPE_B) {
				if (buffer != MPEG_BUFFER_BACKWARD) {
					mpDecoder->SwapFrameBuffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					if (buffer != MPEG_BUFFER_FORWARD) {
						mpDecoder->DecodeFrame((char *)inputBuffer + m1->size, m2->size, f2, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					}
				}
			}

		} else {
			f1 >>= 1;
			m1 = &(parentPtr->video_sample_list[f1]);
			buffer = mpDecoder->GetFrameBuffer(f1);

			if (m1->frame_type != MPEG_FRAME_TYPE_B) {
				if (buffer != MPEG_BUFFER_BACKWARD) {
					mpDecoder->SwapFrameBuffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					if (buffer != MPEG_BUFFER_FORWARD) {
						mpDecoder->DecodeFrame(inputBuffer, data_len, f1, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					}
				}
			}
		}
	}

	if (!is_preroll) {
		// Decode, assemble, and display

		if ((f1 & 1) && sframe > 0) {

			f2 = f1 >> 1;
			f1 = parentPtr->video_field_map[sframe - 1] >> 1;

			m1 = &(parentPtr->video_sample_list[f1]);
			m2 = &(parentPtr->video_sample_list[f2]);

if (inputBuffer != NULL) {

			b1 = mpDecoder->GetFrameBuffer(f1);
			b2 = mpDecoder->GetFrameBuffer(f2);

			if (m1->frame_type != MPEG_FRAME_TYPE_B) {

				if (m2->frame_type != MPEG_FRAME_TYPE_B) {
					// II, IP, PI, PP

					// Do we have f1?
					if (b1 == MPEG_BUFFER_FORWARD) {
						// Maybe we also have f2?
						if (b2 != MPEG_BUFFER_BACKWARD) {
							mpDecoder->DecodeFrame((char *)inputBuffer + m1->size, m2->size, f2, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						}
					} else if (b1 == MPEG_BUFFER_BACKWARD) {
						// Hmmm, got f1 in other buffer
						mpDecoder->SwapFrameBuffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						if (b2 != MPEG_BUFFER_FORWARD) {
							mpDecoder->DecodeFrame((char *)inputBuffer + m1->size, m2->size, f2, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						}
					} else if (b2 == MPEG_BUFFER_FORWARD) {
						// Not got f1, but got f2 in other buffer
						mpDecoder->DecodeFrame(inputBuffer, m1->size, f1, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						mpDecoder->SwapFrameBuffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					} else if (b2 == MPEG_BUFFER_BACKWARD) {
						// Not got f1, but got f2
						mpDecoder->DecodeFrame(inputBuffer, m1->size, f1, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD);
					} else {
						// Ain't got neither
						mpDecoder->DecodeFrame(inputBuffer, m1->size, f1, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						mpDecoder->SwapFrameBuffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						mpDecoder->DecodeFrame((char *)inputBuffer + m1->size, m2->size, f2, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					}

				} else {
					// IB, PB
					if (b1 != MPEG_BUFFER_FORWARD) {
						if (b1 == MPEG_BUFFER_BACKWARD) {
							// Hmm, f1 in wrong buffer
							mpDecoder->SwapFrameBuffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						} else {
							mpDecoder->DecodeFrame(inputBuffer, m1->size, f1, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						}
					}
					if (b2 != MPEG_BUFFER_BIDIRECTIONAL) {
						mpDecoder->DecodeFrame((char *)inputBuffer + m1->size, m2->size, f2, MPEG_BUFFER_BIDIRECTIONAL, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					}
				}
			} else {
				if (m2->frame_type != MPEG_FRAME_TYPE_B) {
					// BI, BP (degenerate)
					if (b2 != MPEG_BUFFER_BACKWARD) {
						mpDecoder->SwapFrameBuffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						if (b2 != MPEG_BUFFER_FORWARD) {
							mpDecoder->DecodeFrame((char *)inputBuffer + m1->size, m2->size, f2, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						}
					}
					if (b1 != MPEG_BUFFER_BIDIRECTIONAL) {
						mpDecoder->DecodeFrame(inputBuffer, m1->size, f1, MPEG_BUFFER_BIDIRECTIONAL, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					}
				} else {
					// BB
					if (b1 != MPEG_BUFFER_BIDIRECTIONAL) {
						mpDecoder->DecodeFrame(inputBuffer, m1->size, f1, MPEG_BUFFER_BIDIRECTIONAL, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					}
					if (b2 != MPEG_BUFFER_AUXILIARY) {
						mpDecoder->DecodeFrame((char *)inputBuffer + m1->size, m2->size, f2, MPEG_BUFFER_AUXILIARY, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					}
				}
			}

}	// inputBuffer != NULL

			// Now assemble the fields into auxiliary buffer
			b1 = mpDecoder->GetFrameBuffer(f1);
			b2 = mpDecoder->GetFrameBuffer(f2);

			if (m2->frame_type == MPEG_FRAME_TYPE_B) {
				if (m1->frame_type == MPEG_FRAME_TYPE_B) {
					buffer = MPEG_BUFFER_AUXILIARY;
				} else {
					buffer = MPEG_BUFFER_BIDIRECTIONAL;
				}
			} else {
				buffer = MPEG_BUFFER_BACKWARD;
			}

			if ((m1->bitflags & 3) != FRAME_PICTURE) {
				if ((m1->bitflags & 3) == TOP_FIELD) {
					// m1 top + m2 bottom
					mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 1, buffer, 1);
					mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 0, b1, 0);
				} else {
					// m1 bottom + m2 top
					mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 0, buffer, 0);
					mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 1, b1, 1);
				}

			} else if ((m2->bitflags & 3) != FRAME_PICTURE) {
				if ((m2->bitflags & 3) == TOP_FIELD) {
					// m1 bottom + m2 top
					mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 0, buffer, 0);
					mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 1, b1, 1);
				} else {
					// m1 top + m2 bottom
					mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 1, buffer, 1);
					mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 0, b1, 0);
				}

			} else {
				if (m1->bitflags & PICTUREFLAG_RFF) {
					if (m1->bitflags & PICTUREFLAG_TFF) {
						// m1 top + m2 bottom
						mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 1, buffer, 1);
						mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 0, b1, 0);
					} else {
						// m1 bottom + m2 top
						mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 0, buffer, 0);
						mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 1, b1, 1);
					}
				} else {
					if (m1->bitflags & PICTUREFLAG_TFF) {
						// m1 bottom + m2 top
						mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 0, buffer, 0);
						mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 1, b1, 1);
					} else {
						// m1 top + m2 bottom
						mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 1, buffer, 1);
						mpDecoder->CopyField(MPEG_BUFFER_AUXILIARY, 0, b1, 0);
					}
				}
			}

			buffer = MPEG_BUFFER_AUXILIARY;

		} else {
			// Frame picture
			f1 >>= 1;
			m1 = &(parentPtr->video_sample_list[f1]);
			buffer = mpDecoder->GetFrameBuffer(f1);

if (inputBuffer != NULL) {

			if (m1->frame_type != MPEG_FRAME_TYPE_B) {
				if (buffer != MPEG_BUFFER_FORWARD) {
					// Got f1 in back buffer?
					if (buffer != MPEG_BUFFER_BACKWARD) {
						mpDecoder->SwapFrameBuffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						if (buffer != MPEG_BUFFER_FORWARD) {
							mpDecoder->DecodeFrame(inputBuffer, m1->size, f1, MPEG_BUFFER_BACKWARD, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
						}
						buffer = MPEG_BUFFER_BACKWARD;
					}
				}
			} else {
				if (buffer != MPEG_BUFFER_BIDIRECTIONAL) {
					mpDecoder->DecodeFrame(inputBuffer, m1->size, f1, MPEG_BUFFER_BIDIRECTIONAL, MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);
					buffer = MPEG_BUFFER_BIDIRECTIONAL;
				}
			}

}	// inputBuffer != NULL

		}

		mpDecoder->ConvertFrame(mPixmap, buffer);
		mPrevFrame = sframe;	//targetFrame;
	}

	return mFrameBuffer;
}


uint32 VDXAPIENTRY VideoDecoderMPEG2::GetDecodePadding()
{
	return 0;
}


void VDXAPIENTRY VideoDecoderMPEG2::Reset()
{
	mpDecoder->ClearFrameBuffers();
	mPrevFrame = -1;
}


bool VDXAPIENTRY VideoDecoderMPEG2::IsFrameBufferValid()
{
	return (mPrevFrame >= 0);
}


const VDXPixmap& VDXAPIENTRY VideoDecoderMPEG2::GetFrameBuffer()
{
	return mPixmap;
}

/*
bool VDXAPIENTRY VideoDecoderMPEG2::SetTargetFormat(int format, bool useDIBAlignment) {
	int w = parentPtr->width;
	int h = parentPtr->height;

	using namespace nsVDXPixmap;

	// Default format:
	// For CHROMA420 this should be YV12
	// For CHROMA422 this should be YUY2
	// For CHROMA444 this should be RGB24

	if (format == kPixFormat_Null)
	{
		const int chroma_format = mpDecoder->GetChromaFormat(); // (parentPtr->seq_ext >> 9) & 3;

		if ((w & 1) == 0 && chroma_format != CHROMA444)
		{
			if ((h & 1) == 0 && chroma_format != CHROMA422)
			{
				if (mpDecoder->IsMPEG2())
					format = kPixFormat_YUV420_Planar_709;
				else
					format = kPixFormat_YUV420_Planar;
			}
			else
			{
				// Height is odd, or format is 4:2:2
				if (mpDecoder->IsMPEG2())
					format = kPixFormat_YUV422_YUYV_709;
				else
					format = kPixFormat_YUV422_YUYV;
			}
		}
		else
		{
			// Width is odd, or format is 4:4:4
			//format = kPixFormat_RGB888;
			if (mpDecoder->IsMPEG2())
				format = kPixFormat_YUV444_Planar_709;
			else
				format = kPixFormat_YUV444_Planar;
		}
	}

	switch (format) {

	case kPixFormat_YUV420_Planar:
	case kPixFormat_YUV420_Planar_FR:
	case kPixFormat_YUV420_Planar_709:
	case kPixFormat_YUV420_Planar_709_FR:
	case kPixFormat_YUV420i_Planar:
	case kPixFormat_YUV420i_Planar_FR:
	case kPixFormat_YUV420i_Planar_709:
	case kPixFormat_YUV420i_Planar_709_FR:
	case kPixFormat_YUV420it_Planar:
	case kPixFormat_YUV420it_Planar_FR:
	case kPixFormat_YUV420it_Planar_709:
	case kPixFormat_YUV420it_Planar_709_FR:
	case kPixFormat_YUV420ib_Planar:
	case kPixFormat_YUV420ib_Planar_FR:
	case kPixFormat_YUV420ib_Planar_709:
	case kPixFormat_YUV420ib_Planar_709_FR:
		// Width and height must be even
		if ((w | h) & 1) return false;
		mPixmap.data		= mFrameBuffer;
		mPixmap.palette		= NULL;
		mPixmap.format		= format;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= w;
		mPixmap.data2		= mFrameBuffer + w * h;
		mPixmap.pitch2		= w >> 1;
		mPixmap.data3		= mFrameBuffer + w * h + (w >> 1) * (h >> 1);
		mPixmap.pitch3		= w >> 1;
		break;

	case kPixFormat_Y8:
	case kPixFormat_Y8_FR:
// Note: according to the SDK, we do not align the pitch
		mPixmap.data		= mFrameBuffer;
		mPixmap.pitch		= w;
		mPixmap.palette		= NULL;
		mPixmap.format		= format;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_YUV422_UYVY:
	case kPixFormat_YUV422_YUYV:
	case kPixFormat_YUV422_UYVY_709:
	case kPixFormat_YUV422_YUYV_709:
	case kPixFormat_YUV422_UYVY_FR:
	case kPixFormat_YUV422_YUYV_FR:
	case kPixFormat_YUV422_UYVY_709_FR:
	case kPixFormat_YUV422_YUYV_709_FR:
		// Width must be even
		if (w & 1) return false;
		mPixmap.data		= mFrameBuffer;
		mPixmap.pitch		= w * 2;
		mPixmap.palette		= NULL;
		mPixmap.format		= format;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_XRGB1555:
	case kPixFormat_RGB565:
		mPixmap.pitch		= (w * 2 + 3) & -4;
		mPixmap.data		= mFrameBuffer + (h - 1) * mPixmap.pitch;
		mPixmap.pitch		= -mPixmap.pitch;
		mPixmap.palette		= NULL;
		mPixmap.format		= format;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_RGB888:
		mPixmap.pitch		= (w * 3 + 3) & -4;
		mPixmap.data		= mFrameBuffer + (h - 1) * mPixmap.pitch;
		mPixmap.pitch		= -mPixmap.pitch;
		mPixmap.palette		= NULL;
		mPixmap.format		= format;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;
	
	case kPixFormat_XRGB8888:
		mPixmap.pitch		= w * 4;
		mPixmap.data		= mFrameBuffer + (h - 1) * mPixmap.pitch;
		mPixmap.pitch		= -mPixmap.pitch;
		mPixmap.palette		= NULL;
		mPixmap.format		= format;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_YUV422_Planar:
	case kPixFormat_YUV422_Planar_709:
	case kPixFormat_YUV422_Planar_FR:
	case kPixFormat_YUV422_Planar_709_FR:
		// Width must be even
		if (w & 1) return false;
		mPixmap.data		= mFrameBuffer;
		mPixmap.pitch		= w;
		mPixmap.palette		= NULL;
		mPixmap.format		= format;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.data2		= mFrameBuffer + w * h;
		mPixmap.pitch2		= w >> 1;
		mPixmap.data3		= mFrameBuffer + w * h + (w >> 1) * h;
		mPixmap.pitch3		= w >> 1;
		break;

	default:
		return false;

	}

	Reset();

	return true;
}
*/


#ifdef _DEBUG
using namespace nsVDXPixmap;

static const struct {
	int format;
	char *name;
} format_names[] = {
	{ kPixFormat_Null,                  "kPixFormat_Null" },
	{ kPixFormat_XRGB1555,              "kPixFormat_XRGB1555" },
	{ kPixFormat_RGB565,                "kPixFormat_RGB565" },
	{ kPixFormat_RGB888,                "kPixFormat_RGB888" },
	{ kPixFormat_XRGB8888,              "kPixFormat_XRGB8888" },
	{ kPixFormat_Y8,                    "kPixFormat_Y8" },
	{ kPixFormat_YUV422_UYVY,           "kPixFormat_YUV422_UYVY" },
	{ kPixFormat_YUV422_YUYV,           "kPixFormat_YUV422_YUYV" },
	{ kPixFormat_YUV444_Planar,         "kPixFormat_YUV444_Planar" },
	{ kPixFormat_YUV422_Planar,         "kPixFormat_YUV422_Planar" },
	{ kPixFormat_YUV420_Planar,         "kPixFormat_YUV420_Planar" },
	{ kPixFormat_YUV411_Planar,         "kPixFormat_YUV411_Planar" },
	{ kPixFormat_YUV410_Planar,         "kPixFormat_YUV410_Planar" },
	{ kPixFormat_YUV422_UYVY_709,       "kPixFormat_YUV422_UYVY_709" },
	{ kPixFormat_Y8_FR,                 "kPixFormat_Y8_FR" },
	{ kPixFormat_YUV422_YUYV_709,       "kPixFormat_YUV422_YUYV_709" },
	{ kPixFormat_YUV444_Planar_709,     "kPixFormat_YUV444_Planar_709" },
	{ kPixFormat_YUV422_Planar_709,     "kPixFormat_YUV422_Planar_709" },
	{ kPixFormat_YUV420_Planar_709,     "kPixFormat_YUV420_Planar_709" },
	{ kPixFormat_YUV411_Planar_709,     "kPixFormat_YUV411_Planar_709" },
	{ kPixFormat_YUV410_Planar_709,     "kPixFormat_YUV410_Planar_709" },
	{ kPixFormat_YUV422_UYVY_FR,        "kPixFormat_YUV422_UYVY_FR" },
	{ kPixFormat_YUV422_YUYV_FR,        "kPixFormat_YUV422_YUYV_FR" },
	{ kPixFormat_YUV444_Planar_FR,      "kPixFormat_YUV444_Planar_FR" },
	{ kPixFormat_YUV422_Planar_FR,      "kPixFormat_YUV422_Planar_FR" },
	{ kPixFormat_YUV420_Planar_FR,      "kPixFormat_YUV420_Planar_FR" },
	{ kPixFormat_YUV411_Planar_FR,      "kPixFormat_YUV411_Planar_FR" },
	{ kPixFormat_YUV410_Planar_FR,      "kPixFormat_YUV410_Planar_FR" },
	{ kPixFormat_YUV422_UYVY_709_FR,    "kPixFormat_YUV422_UYVY_709_FR" },
	{ kPixFormat_YUV422_YUYV_709_FR,    "kPixFormat_YUV422_YUYV_709_FR" },
	{ kPixFormat_YUV444_Planar_709_FR,  "kPixFormat_YUV444_Planar_709_FR" },
	{ kPixFormat_YUV422_Planar_709_FR,  "kPixFormat_YUV422_Planar_709_FR" },
	{ kPixFormat_YUV420_Planar_709_FR,  "kPixFormat_YUV420_Planar_709_FR" },
	{ kPixFormat_YUV411_Planar_709_FR,  "kPixFormat_YUV411_Planar_709_FR" },
	{ kPixFormat_YUV410_Planar_709_FR,  "kPixFormat_YUV410_Planar_709_FR" },
	{ kPixFormat_YUV420i_Planar,        "kPixFormat_YUV420i_Planar" },
	{ kPixFormat_YUV420i_Planar_FR,     "kPixFormat_YUV420i_Planar_FR" },
	{ kPixFormat_YUV420i_Planar_709,    "kPixFormat_YUV420i_Planar_709" },
	{ kPixFormat_YUV420i_Planar_709_FR, "kPixFormat_YUV420i_Planar_709_FR" },
	{ kPixFormat_YUV420it_Planar,       "kPixFormat_YUV420it_Planar" },
	{ kPixFormat_YUV420it_Planar_FR,    "kPixFormat_YUV420it_Planar_FR" },
	{ kPixFormat_YUV420it_Planar_709,   "kPixFormat_YUV420it_Planar_709" },
	{ kPixFormat_YUV420it_Planar_709_FR,"kPixFormat_YUV420it_Planar_709_FR" },
	{ kPixFormat_YUV420ib_Planar,       "kPixFormat_YUV420ib_Planar" },
	{ kPixFormat_YUV420ib_Planar_FR,    "kPixFormat_YUV420ib_Planar_FR" },
	{ kPixFormat_YUV420ib_Planar_709,   "kPixFormat_YUV420ib_Planar_709" },
	{ kPixFormat_YUV420ib_Planar_709_FR,"kPixFormat_YUV420ib_Planar_709_FR" },
	{ kPixFormat_VDXA_RGB,              "kPixFormat_VDXA_RGB" },
	{ kPixFormat_VDXA_YUV,              "kPixFormat_VDXA_YUV" }
};

#endif  // _DEBUG


bool VDXAPIENTRY VideoDecoderMPEG2::SetTargetFormat(int format, bool useDIBAlignment)
{
	const int w = parentPtr->width;
	const int h = parentPtr->height;

	using namespace nsVDXPixmap;

	if (format == kPixFormat_Null)
	{
		const int chroma_format = (parentPtr->seq_ext >> 9) & 3;

		// Note that chroma_format will be zero for MPEG-1!
		// Treat this as the equivalent of CHROMA420 (1).

		// For compatibility with versions of VirtualDub earlier
		// than 1.10.1, do NOT set any new i/FR/709 formats here!
		
		if ((w & 1) == 0 && chroma_format != CHROMA444)
		{
			if ((h & 1) == 0 && chroma_format != CHROMA422)
			{
				// Width and height are even, format is 4:2:0
				format = kPixFormat_YUV420_Planar;
			}
			else
			{
				// Height is odd, or format is 4:2:2
				format = kPixFormat_YUV422_YUYV;
			}
		}
		else
		{
			// Width is odd, or format is 4:4:4
			format = kPixFormat_RGB888;
		}
	}

#ifdef _DEBUG
{
	int i, count = sizeof(format_names) / sizeof(format_names[0]);
	for (i = 0; i < count; ++i)
	{
		if (format_names[i].format == format)
		{
			_RPT1(_CRT_WARN, "SetTargetFormat(%s)\n", format_names[i].name);
			break;
		}
	}
	if (i == count)
	{
		_RPT1(_CRT_WARN, "SetTargetFormat(%i) (unknown!)\n", format);
	}
}
#endif

	switch (format) {

	case kPixFormat_XRGB1555:
	case kPixFormat_RGB565:
		mPixmap.pitch		= (w * 2 + 3) & -4;
		mPixmap.data		= mFrameBuffer + (h - 1) * mPixmap.pitch;
		mPixmap.palette		= NULL;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= -mPixmap.pitch;
		mPixmap.format		= format;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_RGB888:
		mPixmap.pitch		= (w * 3 + 3) & -4;
		mPixmap.data		= mFrameBuffer + (h - 1) * mPixmap.pitch;
		mPixmap.palette		= NULL;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= -mPixmap.pitch;
		mPixmap.format		= format;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_XRGB8888:
		mPixmap.pitch		= w * 4;
		mPixmap.data		= mFrameBuffer + (h - 1) * mPixmap.pitch;
		mPixmap.palette		= NULL;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= -mPixmap.pitch;
		mPixmap.format		= format;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_Y8:
	case kPixFormat_Y8_FR:
		// Note: according to the SDK, we do not align the pitch
		mPixmap.data		= mFrameBuffer;
		mPixmap.palette		= NULL;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= w;
		mPixmap.format		= format;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_YUV420_Planar:
	case kPixFormat_YUV420_Planar_709:
	case kPixFormat_YUV420_Planar_FR:
	case kPixFormat_YUV420_Planar_709_FR:
		// Width and height must be even
		if ((w | h) & 1) return false;
		mPixmap.data		= mFrameBuffer;
		mPixmap.palette		= NULL;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= w;
		mPixmap.format		= format;
		mPixmap.data2		= mFrameBuffer + w * h;
		mPixmap.pitch2		= w >> 1;
		mPixmap.data3		= mFrameBuffer + w * h + (w >> 1) * (h >> 1);
		mPixmap.pitch3		= w >> 1;
		break;

	case kPixFormat_YUV420i_Planar:
	case kPixFormat_YUV420i_Planar_FR:
	case kPixFormat_YUV420i_Planar_709:
	case kPixFormat_YUV420i_Planar_709_FR:
	case kPixFormat_YUV420it_Planar:
	case kPixFormat_YUV420it_Planar_FR:
	case kPixFormat_YUV420it_Planar_709:
	case kPixFormat_YUV420it_Planar_709_FR:
	case kPixFormat_YUV420ib_Planar:
	case kPixFormat_YUV420ib_Planar_FR:
	case kPixFormat_YUV420ib_Planar_709:
	case kPixFormat_YUV420ib_Planar_709_FR:
		return false;   // TODO, interlaced 4:2:0 formats

	case kPixFormat_YUV422_YUYV:
	case kPixFormat_YUV422_YUYV_709:
	case kPixFormat_YUV422_YUYV_FR:
	case kPixFormat_YUV422_YUYV_709_FR:
	case kPixFormat_YUV422_UYVY:
	case kPixFormat_YUV422_UYVY_709:
	case kPixFormat_YUV422_UYVY_FR:
	case kPixFormat_YUV422_UYVY_709_FR:
		// Width must be even
		if (w & 1) return false;
		mPixmap.data		= mFrameBuffer;
		mPixmap.palette		= NULL;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= w * 2;
		mPixmap.format		= format;
		mPixmap.data2		= NULL;
		mPixmap.pitch2		= 0;
		mPixmap.data3		= NULL;
		mPixmap.pitch3		= 0;
		break;

	case kPixFormat_YUV422_Planar:
	case kPixFormat_YUV422_Planar_709:
	case kPixFormat_YUV422_Planar_FR:
	case kPixFormat_YUV422_Planar_709_FR:
		// Width must be even
		if (w & 1) return false;
		mPixmap.data		= mFrameBuffer;
		mPixmap.palette		= NULL;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= w;
		mPixmap.format		= format;
		mPixmap.data2		= mFrameBuffer + w * h;
		mPixmap.pitch2		= w >> 1;
		mPixmap.data3		= mFrameBuffer + w * h + (w >> 1) * h;
		mPixmap.pitch3		= w >> 1;
		break;

	case kPixFormat_YUV444_Planar:
	case kPixFormat_YUV444_Planar_709:
	case kPixFormat_YUV444_Planar_FR:
	case kPixFormat_YUV444_Planar_709_FR:
		mPixmap.data		= mFrameBuffer;
		mPixmap.palette		= NULL;
		mPixmap.w			= w;
		mPixmap.h			= h;
		mPixmap.pitch		= w;
		mPixmap.format		= format;
		mPixmap.data2		= mFrameBuffer + w * h;
		mPixmap.pitch2		= w;
		mPixmap.data3		= mFrameBuffer + w * h * 2;
		mPixmap.pitch3		= w;
		break;

	// VirtualDub seems to silently convert from 4:2:2 (which
	// we do support) to 4:1:0 and 4:1:1 internally, so at the
	// moment we can get away with not supporting these directly.

	case kPixFormat_YUV410_Planar:
	case kPixFormat_YUV410_Planar_709:
	case kPixFormat_YUV410_Planar_FR:
	case kPixFormat_YUV410_Planar_709_FR:
	case kPixFormat_YUV411_Planar:
	case kPixFormat_YUV411_Planar_709:
	case kPixFormat_YUV411_Planar_FR:
	case kPixFormat_YUV411_Planar_709_FR:
		return false;

	case kPixFormat_VDXA_RGB:
	case kPixFormat_VDXA_YUV:
	default:
		return false;
	}

	Reset();
	return true;
}


bool VDXAPIENTRY VideoDecoderMPEG2::SetDecompressedFormat(const VDXBITMAPINFOHEADER *pbih)
{
	return false;
}


const void *VDXAPIENTRY VideoDecoderMPEG2::GetFrameBufferBase()
{
	return mFrameBuffer;
}


bool VDXAPIENTRY VideoDecoderMPEG2::IsDecodable(sint64 sample_num64)
{
	return (mpDecoder->GetFrameBuffer((int)sample_num64) >= 0);
}


//////////////////////////////////////////////////////////////////////////
//
//
//						VideoDecoderModelMPEG2
//
//
//////////////////////////////////////////////////////////////////////////

VideoDecoderModelMPEG2::VideoDecoderModelMPEG2(InputFileMPEG2 *const pp)
	: parentPtr(pp)
{
	parentPtr->AddRef();

_RPT0(_CRT_WARN, "VideoDecoderModelMPEG2 constructed\n");

	mSampleFirst = 0;
	mSampleLast  = parentPtr->fields;
}


VideoDecoderModelMPEG2::~VideoDecoderModelMPEG2()
{
_RPT0(_CRT_WARN, "VideoDecoderModelMPEG2 destructed\n");
	parentPtr->Release();
}


void VDXAPIENTRY VideoDecoderModelMPEG2::Reset()
{
	frame_forw = frame_back = frame_bidir = frame_aux = -1;

//_RPT0(_CRT_WARN, "Reset()\n");
}


sint64 VideoDecoderModelMPEG2::prev_field_IP(sint64 f64) const
{
	if (f64 < mSampleLast)
	{
		const MPEGSampleInfo *m1;
		int f1;

		while (--f64 >= mSampleFirst)
		{
			f1 = parentPtr->video_field_map[f64] >> 1;
			m1 = &(parentPtr->video_sample_list[f1]);
			if (m1->frame_type != MPEG_FRAME_TYPE_B) return f64;
		}
	}

    return -1;
}


sint64 VideoDecoderModelMPEG2::next_field_IP(sint64 f64) const
{
	if (f64 >= mSampleFirst)
	{
		const MPEGSampleInfo *m1;
		int f1;

		while (++f64 < mSampleLast)
		{
			f1 = parentPtr->video_field_map[f64] >> 1;
			m1 = &(parentPtr->video_sample_list[f1]);
			if (m1->frame_type != MPEG_FRAME_TYPE_B) return f64;
		}
	}

    return -1;
}


void VDXAPIENTRY VideoDecoderModelMPEG2::SetDesiredFrame(sint64 frame_num)
{
	const MPEGSampleInfo *m1;
	int f1, f2, prev, next;
	sint64 n;

	stream_desired_frame = stream_current_frame = frame_num;

//	_RPT1(_CRT_WARN, "SetDesiredFrame(%i)\n", (int)frame_num);

	f1 = parentPtr->video_field_map[frame_num];

	// If the desired frame is laced... (aargh)

	if ((f1 & 1) && stream_current_frame > mSampleFirst) {
		const MPEGSampleInfo *m2;

		f2 = f1 >> 1;
		f1 = parentPtr->video_field_map[stream_current_frame - 1] >> 1;

		m1 = &(parentPtr->video_sample_list[f1]);
		m2 = &(parentPtr->video_sample_list[f2]);

		// There are a lot of possibilities here!

		// Just remember, the question we are answering
		// is whether we need to move stream_current_frame
		// from its original position at stream_desired_frame

		if (m1->frame_type == MPEG_FRAME_TYPE_I) {
			// II or IP, do nothing

			if (m2->frame_type == MPEG_FRAME_TYPE_B) {
				// IB

				// Decode f1, then future IP, then f2

// Fix for Clip.m2v?  We need not be concerned with I, only B:
//				if (frame_forw != f1 || frame_bidir != f2) {
				if (frame_bidir != f2) {

					// Find the future reference
					n = next_field_IP(stream_current_frame);
					if (n >= 0) {
						next = parentPtr->video_field_map[n] >> 1;

						if (frame_forw != f1 || frame_back != next) {
							if (frame_back == f1) {
								// Start at next reference
								stream_current_frame = n;
							} else {
								// Start at I frame
								--stream_current_frame;
							}
						}
					}
				}
			}

		} else if (m1->frame_type == MPEG_FRAME_TYPE_P) {

			if (m2->frame_type != MPEG_FRAME_TYPE_B) {
				// PI, or PP

				// Decode previous reference, then f1, then f2

				// If we don't have f1, back to prev I/P
				// Otherwise start from f2

				if ((frame_forw != f1 || frame_back != f2)
						&& frame_back != f1) {

					// Start from f1 if possible
					--stream_current_frame;
					while (stream_current_frame > mSampleFirst) {
						n = prev_field_IP(stream_current_frame);
						if (n < 0) break;	// ouch
						prev = parentPtr->video_field_map[n] >> 1;
						if (frame_back == prev) break;
						stream_current_frame = n;
						if (parentPtr->video_sample_list[prev].frame_type == MPEG_FRAME_TYPE_I) break;
					}
				}

			} else {
				// PB

				// Decode previous reference, then f1,
				// then future reference, then f2

				if (frame_forw != f1 && frame_back != f1) {

					// Missing the P, go back to prev I/P
					--stream_current_frame;
					while (stream_current_frame > mSampleFirst) {
						n = prev_field_IP(stream_current_frame);
						prev = parentPtr->video_field_map[n] >> 1;
						if (frame_back == prev) break;
						stream_current_frame = n;
						if (parentPtr->video_sample_list[prev].frame_type == MPEG_FRAME_TYPE_I) break;
					}

				} else if (frame_bidir != f2) {
					// Got P, but missing B

					// We'll need the future reference
					n = next_field_IP(stream_current_frame);
					if (n >= 0) {
						next = parentPtr->video_field_map[n] >> 1;

					// If P is in forward buffer but we don't have
					// next, start at P.  (Not sure that can happen)

					// If P is in back buffer and we don't have next
					// then start at next.
						if (frame_forw == f1 && frame_back != next) {
							// Something got screwed
							--stream_current_frame;
							while (stream_current_frame > mSampleFirst) {
								n = prev_field_IP(stream_current_frame);
								prev = parentPtr->video_field_map[n] >> 1;
								if (frame_back == prev) break;
								stream_current_frame = n;
								if (parentPtr->video_sample_list[prev].frame_type == MPEG_FRAME_TYPE_I) break;
							}
						} else if (frame_back == f1) {
							stream_current_frame = n;
						}
					}
				}
			}

		} else {
			// BI, BP, or BB

			if (m2->frame_type != MPEG_FRAME_TYPE_B) {
				// BI, or BP

				// Fix for VdubCrash2.mpg
				if (frame_bidir != f1 || (frame_forw != f2 && frame_back != f2 && m2->frame_type != MPEG_FRAME_TYPE_I)) {

					// Do we have the previous reference?
					n = prev_field_IP(stream_current_frame);
					if (n >= 0) {
						prev = parentPtr->video_field_map[n] >> 1;

						// If we don't have it, start there
						if (frame_back != prev) {

							stream_current_frame = n;
							while (stream_current_frame > mSampleFirst) {
								n = prev_field_IP(stream_current_frame);
								prev = parentPtr->video_field_map[n] >> 1;
								if (frame_back == prev) break;
								stream_current_frame = n;
								if (parentPtr->video_sample_list[prev].frame_type == MPEG_FRAME_TYPE_I) break;
							}

						}
					} else {
						// Degenerate; no previous reference
						--stream_current_frame;
					}
				}

			} else {
				// BB (special case)

				if (frame_bidir != f1 || frame_aux != f2) {
					// We're going to need to look for 2 refs
					sint64 n2;

					// Find the references
					n = prev_field_IP(stream_current_frame);
					n2 = next_field_IP(stream_current_frame);

					if (n >= 0 && n2 >= 0) {

						prev = parentPtr->video_field_map[n] >> 1;
						next = parentPtr->video_field_map[n2] >> 1;

						if (frame_forw == prev && frame_back == next) {
							// Wow, we have both references!
							// Start from f1 if we don't have it

//							if (frame_bidir != f1) {
//							// (So far, only seen when moving backwards)
//								--stream_current_frame;
//							}

						} else if (frame_back == prev) {
							// We have the prev reference, start at next
							stream_current_frame = n2;

						} else {
							// Drat, go back to prev I/P

							// (This can happen on a random seek)
							stream_current_frame = n;
							while (stream_current_frame > mSampleFirst) {
								n = prev_field_IP(stream_current_frame);
								prev = parentPtr->video_field_map[n] >> 1;
								if (frame_back == prev) break;
								stream_current_frame = n;
								if (parentPtr->video_sample_list[prev].frame_type == MPEG_FRAME_TYPE_I) break;
							}
						}
					}
				}
			}
		}
		return;
	}


	// No lacing involved, decode as usual

	f1 >>= 1;
	m1 = &(parentPtr->video_sample_list[f1]);

	switch(m1->frame_type) {

	case MPEG_FRAME_TYPE_P:
		if (frame_back != f1 && frame_forw != f1) {
			// Back to previous I/P
			while (stream_current_frame > mSampleFirst) {
				n = prev_field_IP(stream_current_frame);
				if (n < 0) break;	// ouch
				f1 = parentPtr->video_field_map[n] >> 1;
				if (frame_back == f1) break;
				stream_current_frame = n;
				if (parentPtr->video_sample_list[f1].frame_type == MPEG_FRAME_TYPE_I) break;
			}
		}
		break;

	case MPEG_FRAME_TYPE_B:

		if (frame_bidir != f1) {
			sint64 np, nn;

			// Back to previous I/P

			// Find the past reference
			np = prev_field_IP(stream_current_frame);
			if (np >= 0) {
				prev = parentPtr->video_field_map[np] >> 1;
			} else {
				prev = 0;
			}

			// Find the future reference
			nn = next_field_IP(stream_current_frame);
			if (nn >= 0) {
				next = parentPtr->video_field_map[nn] >> 1;
			} else {
				next = parentPtr->vframes - 1;
			}

			if (np >= 0 && nn >= 0) {

				// We only need to move if we're missing one of these
				if (frame_forw != prev || frame_back != next) {
					if (frame_back != prev) {
						// Back to prev I/P
						stream_current_frame = np;

						while (stream_current_frame > mSampleFirst) {
							n = prev_field_IP(stream_current_frame);
							if (n < 0) break;	// ouch
							f1 = parentPtr->video_field_map[n] >> 1;
							if (frame_back == f1) break;
							stream_current_frame = n;
							if (parentPtr->video_sample_list[f1].frame_type == MPEG_FRAME_TYPE_I) break;
						}
					} else {
						// Already got prev, start at next
						stream_current_frame = nn;
					}
				}

			}
		}
	}
}


sint64 VDXAPIENTRY VideoDecoderModelMPEG2::GetNextRequiredSample(bool& is_preroll)
{
	const MPEGSampleInfo *m1, *m2;
	int f1, f2;
	sint64 current;

	current = stream_current_frame;

//	_RPT2(_CRT_WARN, "GetNextRequiredSample(current = %i, desired = %i)\n",
//		(int)current, (int)stream_desired_frame);

	f1 = parentPtr->video_field_map[current];

	// Only decode I/P frames, until we reach the desired frame

	if (current < stream_desired_frame) {
		f1 >>= 1;
		if (parentPtr->video_sample_list[f1].frame_type != MPEG_FRAME_TYPE_B) {
			if (frame_back != f1) {
				frame_forw = frame_back;
				frame_back = f1;
			}
		}
		stream_current_frame = next_field_IP(current);

		// Watch out for this weird situation:
		if (stream_current_frame == (stream_desired_frame - 1)) {
			f1 = parentPtr->video_field_map[stream_current_frame];
			f2 = parentPtr->video_field_map[stream_desired_frame];
			if ((f1 & 1) == 1 && f1 == (f2 + 1)) {
				// We don't need f1, so skip to desired
				stream_current_frame = stream_desired_frame;
			}
		}

//_RPT1(_CRT_WARN, "GetNextRequiredSample returning %i, True\n", (int)current);

		is_preroll = true;
		return current;
	}

	if (current > stream_desired_frame) {
		// We're one frame away from being finished
		f1 >>= 1;
		if (parentPtr->video_sample_list[f1].frame_type != MPEG_FRAME_TYPE_B) {
			if (frame_back != f1) {
				frame_forw = frame_back;
				frame_back = f1;
			}
		}
		stream_current_frame = stream_desired_frame;

//_RPT1(_CRT_WARN, "GetNextRequiredSample returning %i, True\n", (int)current);

		is_preroll = true;
		return current;
	}

	// We've reached the desired frame.  But in the
	// case of a laced frame, we must emulate everything
	// streamGetFrame does to ensure it can be assembled.

	if ((f1 & 1) && current > mSampleFirst) {
		f2 = f1 >> 1;
		f1 = parentPtr->video_field_map[current - 1] >> 1;

		m1 = &(parentPtr->video_sample_list[f1]);
		m2 = &(parentPtr->video_sample_list[f2]);

		if (m1->frame_type != MPEG_FRAME_TYPE_B) {

			if (m2->frame_type != MPEG_FRAME_TYPE_B) {
				// II, IP, PI, PP

				// Do we have f1?
				if (frame_forw == f1) {
					if (frame_back == f2) goto Neg1;
					frame_back = f2;
				} else if (frame_back == f1) {
					// Hmmm, got f1 in wrong buffer
					frame_back = frame_forw;
					frame_forw = f1;
					if (frame_back == f2) goto Neg1;
					frame_back = f2;
				} else if (frame_forw == f2) {
					// Not got f1, but got f2 in wrong buffer
					frame_forw = f1;
					frame_back = f2;
				} else if (frame_back == f2) {
					// Huh? Not got f1, but f2 in correct buffer!
					// (streamGetFrame will take care of this)
					frame_forw = f1;
				} else {
					// Ain't got neither f1 nor f2 (triple negative)
					frame_forw = f1;
					frame_back = f2;
				}

			} else {
				// IB or PB
				if (frame_forw != f1) {
					if (frame_back == f1) {
						// Hmm, f1 in wrong buffer
						frame_back = frame_forw;
						frame_forw = f1;
					} else {
						frame_back = f1;
					}
				}
				if (frame_bidir == f2) goto Neg1;
				frame_bidir = f2;
			}
		} else {
			if (m2->frame_type != MPEG_FRAME_TYPE_B) {
				// BI or BP
				// Fix for Hannah Montana.mpg, frame 3727:
				// If f2 is a P-frame not in the back buffer,
				// then we still need a previous reference;
				// Do NOT return -1 in this case.
				if (frame_back != f2)
				{
					frame_forw = frame_back;
					frame_back = f2;
					if (m2->frame_type == MPEG_FRAME_TYPE_I)
					{
						// This happens in Hannah Montana.mpg, frame 3663:
						if (frame_bidir == f1) goto Neg1;
					}
				}
				else
				{
					if (frame_bidir == f1) goto Neg1;
				}
				frame_bidir = f1;
			} else {
				// BB (special case)
				if (frame_bidir == f1 && frame_aux == f2) goto Neg1;
				frame_bidir = f1;
				frame_aux = f2;
			}
		}


	} else {
		// No lacing, thank goodness
		f1 >>= 1;
		m1 = &(parentPtr->video_sample_list[f1]);

		if (m1->frame_type != MPEG_FRAME_TYPE_B) {
			if (frame_back == f1) goto Neg1;
//			if (frame_forw == f1) goto Neg1;
			frame_forw = frame_back;
			frame_back = f1;
		} else {
			if (frame_bidir == f1) goto Neg1;
			frame_bidir = f1;
		}
	}

//_RPT1(_CRT_WARN, "GetNextRequiredSample returning %i, False\n", (int)current);

	is_preroll = false;
	return current;

Neg1:
	// Finished!

//_RPT0(_CRT_WARN, "GetNextRequiredSample returning -1, False\n");

	frame_aux = -1;
	is_preroll = false;
	return -1;
}


int VDXAPIENTRY VideoDecoderModelMPEG2::GetRequiredCount()
{
	// Return int = the number of frames which need to be decoded

	// Perhaps the easiest way to do this is to emulate
	// VirtualDub calls to GetNextRequiredSample.

	// First, save ALL tracking variables...

	sint64 saved_stream_current_frame = stream_current_frame;
	sint64 saved_stream_desired_frame = stream_desired_frame;

	int saved_frame_forw  = frame_forw;
	int saved_frame_back  = frame_back;
	int saved_frame_bidir = frame_bidir;
	int saved_frame_aux   = frame_aux;

	int needed = 1;

	for (;;)
	{
		bool p;
		sint64 next = GetNextRequiredSample(p);
		if (next == -1) break;

		int f1 = parentPtr->video_field_map[next];

		if ((f1 & 1) && next > mSampleFirst)
		{
			f1 = parentPtr->video_field_map[next - 1] >> 1;
		}
		++needed;
	}

	frame_aux   = saved_frame_aux;
	frame_bidir = saved_frame_bidir;
	frame_back  = saved_frame_back;
	frame_forw  = saved_frame_forw;

	stream_desired_frame = saved_stream_desired_frame;
	stream_current_frame = saved_stream_current_frame;

//_RPT1(_CRT_WARN, "GetRequiredCount returning %i\n", needed);

	return needed;
}

