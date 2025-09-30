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

// MPEG, AC-3, and LPCM audio interfaces (IVDXAudioSource)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmreg.h>
#include <crtdbg.h>

#include "vd2/plugin/vdinputdriver.h"
#include "Unknown.h"

#include "infile.h"
#include "append.h"
#include "InputFileMPEG2.h"
#include "audio.h"

#include "vd2/Priss/decoder.h"
#include "vd2/system/cpuaccel.h"

#ifdef _DEBUG
#include "vd2/system/vdtypes.h"
#endif

static const int bitrate[2][3][16] = {
		{
			{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 },	// MPEG-1 layer I
			{ 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 },	// MPEG-1 layer II
			{ 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 },	// MPEG-1 layer III
		},
		{
			{ 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 },	// MPEG-2 layer I
			{ 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 },	// MPEG-2 layer II
			{ 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 },	// MPEG-2 layer III
		}
};


static const long samp_freq[2][2][4] = {
	{
		{ 44100, 48000, 32000, 0 },		// MPEG-1
		{ 22050, 24000, 16000, 0 },		// MPEG-2
	},
	{
		{     0,     0,     0, 0 },		// impossible
		{ 11025, 12000,  8000, 0 },		// MPEG-2.5
	}
};

#ifdef _DEBUG
// The following allows us to use "vdtypes.h" without modification.
VDAssertResult VDAssert(const char *exp, const char *file, int line)
{
	return kVDAssertBreak;
}
#endif

unsigned short mpeg_compact_header(const unsigned long hdr) {
	return (unsigned short)(
		(hdr & 0xFE00) |
		((hdr << 1) & 0x01E0) |
		((hdr >> 16) & 0x001F)
	);
}

unsigned MPEGAudioCompacted::GetBitrateKbps() const {
	return bitrate[IsMPEG2()][GetLayer()-1][GetBitrateIndex()];
}

unsigned MPEGAudioCompacted::GetSamplingRateHz() const {
	return samp_freq[IsMPEG25()][IsMPEG2()][GetSamplingRateIndex()];
}

unsigned MPEGAudioCompacted::GetFrameSize() const {
	const unsigned	bitrate		= GetBitrateKbps();
	const unsigned	freq		= GetSamplingRateHz();
	const unsigned	padding		= IsPadded();

	if (GetLayer() == 1)
		return 4 * (12000 * bitrate / freq + padding);

	if (IsMPEG2())
		return (72000 * bitrate / freq + padding);

	return (144000 * bitrate / freq + padding);
}

unsigned MPEGAudioCompacted::GetPayloadSizeL3() const {
	static const unsigned sideinfo_size[2][2]={17,32,9,17};

	unsigned size = GetFrameSize() - sideinfo_size[IsMPEG2()][IsStereo()];

	if (IsCRCProtected()) size -= 2;

	return size;
}


unsigned MPEGAudioHeader::GetBitrateKbps() const {
	return bitrate[IsMPEG2()][GetLayer()-1][GetBitrateIndex()];
}

unsigned MPEGAudioHeader::GetSamplingRateHz() const {
	return samp_freq[IsMPEG25()][IsMPEG2()][GetSamplingRateIndex()];
}

bool MPEGAudioHeader::IsValid() const {
	return IsSyncValid()					// need at least 11 bits for sync mark
		&& (!IsMPEG25() || IsMPEG2())		// either twelfth bit is set or it's MPEG-2.5
		&& GetLayer() != 4					// layer IV invalid
		&& GetBitrateIndex() != 15			// bitrate=1111 invalid
		&& GetSamplingRateIndex() != 3		// sampling_rate=11 reserved
		;
}

bool MPEGAudioHeader::IsConsistent(uint32 hdr) const {
	uint32 headerdiff = (hdr ^ mHeader);

	// do not allow MPEG version, layer, or sampling rate to change
	if (headerdiff & (kMaskSampleRate | kMaskVersion | kMaskLayer | kMaskMPEG25))
		return false;

	// only layer III decoders must accept VBR
	if (GetLayer() != 3 && (headerdiff & kMaskBitrate))
		return false;

	return true;
}

unsigned MPEGAudioHeader::GetFrameSize() const {
	const unsigned	bitrate		= GetBitrateKbps();
	const unsigned	freq		= GetSamplingRateHz();
	const unsigned	padding		= IsPadded();

	if (GetLayer() == 1)
		return 4 * (12000 * bitrate / freq + padding);

// Fix from VirtualDub 1.8.1:
//	if (IsMPEG2())
	if (IsMPEG2() && GetLayer() == 3)
		return (72000 * bitrate / freq + padding);

	return (144000 * bitrate / freq + padding);
}

unsigned MPEGAudioHeader::GetPayloadSizeL3() const {

	static const unsigned sideinfo_size[2][2]={17,32,9,17};

	unsigned size = GetFrameSize() - sideinfo_size[IsMPEG2()][IsStereo()];

	if (IsCRCProtected())
		size -= 2;

	return size;
}


///////////////////////////////////////////////////////////////////////
//
//
//						AudioSourceMPEG2
//
//
///////////////////////////////////////////////////////////////////////

class AudioSourceMPEG2 : public vdxunknown<IVDXStreamSource>, public IVDXAudioSource, IVDMPEGAudioBitsource {

protected:
	VDXWAVEFORMATEX mRawFormat;

private:
	InputFileMPEG2 *const parentPtr;
	const unsigned int id;

	IVDMPEGAudioDecoder *iad;

	char pkt_buffer[8192];
	short sample_buffer[1152 * 2][2];

	long lCurrentPacket;
	long mSamplesPerFrame;
	int layer;

	sint64 mLength;

	char *pDecoderPoint;
	char *pDecoderLimit;

	void UpdateCPUflags();

public:
	AudioSourceMPEG2(InputFileMPEG2 *const pp, const unsigned int stream_id);
	~AudioSourceMPEG2();

	int VDXAPIENTRY AddRef();
	int VDXAPIENTRY Release();
	void *VDXAPIENTRY AsInterface(uint32 iid);

	void		VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo&);
	bool		VDXAPIENTRY Read(sint64 lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);

	const void *VDXAPIENTRY GetDirectFormat();
	int			VDXAPIENTRY GetDirectFormatLen();

	ErrorMode VDXAPIENTRY GetDecodeErrorMode();
	void VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode);
	bool VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode);

	bool VDXAPIENTRY IsVBR();
	sint64 VDXAPIENTRY TimeToPositionVBR(sint64 us);
	sint64 VDXAPIENTRY PositionToTimeVBR(sint64 samples);

	void VDXAPIENTRY GetAudioSourceInfo(VDXAudioSourceInfo& info);

	// IVDMPEGAudioBitsource methods

	int read(void *buffer, int bytes);
};

// The following is for Priss:
static long prissext;
extern long __cdecl CPUGetEnabledExtensions() { return prissext; }

AudioSourceMPEG2::AudioSourceMPEG2(InputFileMPEG2 *const pp, const unsigned int stream_id)
	: parentPtr(pp)
	, id(stream_id)
{
	parentPtr->AddRef();

_RPT0(_CRT_WARN, "AudioSourceMPEG2 constructed\n");

	iad = NULL;
	mLength = 0;

	// Sanity checks
	if (id >= 48U) return;
	if (parentPtr->audio_sample_list[id] == NULL) return;
	if (parentPtr->aframes[id] <= 0) return;

	UpdateCPUflags();

	iad = VDCreateMPEGAudioDecoder();
	if (iad == NULL) return;

	pDecoderPoint = pDecoderLimit = pkt_buffer;
	lCurrentPacket = -1;

	iad->Init();
	iad->SetSource(this);

	MPEGAudioCompacted header(parentPtr->audio_sample_list[id][0].header);

	layer = header.GetLayer();
	if (layer == 1) {
		mSamplesPerFrame = 384;
	} else if ((layer == 3) && header.IsMPEG2()) {
		mSamplesPerFrame = 576;
	} else {
		mSamplesPerFrame = 1152;
	}

	mRawFormat.mFormatTag		= VDXWAVEFORMATEX::kFormatPCM;

	mRawFormat.mChannels		= header.IsStereo()? 2: 1;
	mRawFormat.mSamplesPerSec	= header.GetSamplingRateHz();
	mRawFormat.mBlockAlign		= mRawFormat.mChannels * 2;
	mRawFormat.mAvgBytesPerSec	= mRawFormat.mSamplesPerSec * mRawFormat.mBlockAlign;
	mRawFormat.mBitsPerSample	= 16;
	mRawFormat.mExtraSize		= 0;

	mLength = parentPtr->aframes[id] * mSamplesPerFrame;
}

AudioSourceMPEG2::~AudioSourceMPEG2() {
_RPT0(_CRT_WARN, "AudioSourceMPEG2 destructed\n");
	delete iad;
	parentPtr->Release();
}

void AudioSourceMPEG2::UpdateCPUflags()
{
	// Save static CPU extensions for Priss to retrieve.
	// Priss only checks CPUF_SUPPORTS_MMX and CPUF_SUPPORTS_SSE.

	prissext = 0;
	uint32 flags = parentPtr->mContext.mpCallbacks->GetCPUFeatureFlags();

	if (flags & kVDXCPUF_MMX)
	{
		prissext = CPUF_SUPPORTS_MMX;
		if (flags & kVDXCPUF_SSE)
		{
			prissext |= CPUF_SUPPORTS_SSE;
		}
	}
}

int VDXAPIENTRY AudioSourceMPEG2::AddRef() {
	return vdxunknown<IVDXStreamSource>::AddRef();
}

int VDXAPIENTRY AudioSourceMPEG2::Release() {
	return vdxunknown<IVDXStreamSource>::Release();
}

void *VDXAPIENTRY AudioSourceMPEG2::AsInterface(uint32 iid) {
	if (iid == IVDXAudioSource::kIID)
		return static_cast<IVDXAudioSource *>(this);

	if (iid == IVDXStreamSource::kIID)
		return static_cast<IVDXStreamSource *>(this);

	return NULL;
}


void VDXAPIENTRY AudioSourceMPEG2::GetStreamSourceInfo(VDXStreamSourceInfo& srcInfo)
{
	if (iad != NULL) {
		srcInfo.mSampleRate.mNumerator = mRawFormat.mSamplesPerSec;
		srcInfo.mSampleRate.mDenominator = 1;
		srcInfo.mSampleCount = mLength;
	} else {
		srcInfo.mSampleRate.mNumerator = 1;
		srcInfo.mSampleRate.mDenominator = 1;
		srcInfo.mSampleCount = 0;
	}
}


bool VDXAPIENTRY AudioSourceMPEG2::Read(sint64 lStart64, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead)
{
	long lAudioPacket;
	MPEGSampleInfo *msi;
	long offset, samples;
	long len, ba;

	if (lStart64 < 0 || lStart64 >= mLength || iad == NULL)
	{
		if (lSamplesRead) *lSamplesRead = 0;
		if (lBytesRead) *lBytesRead = 0;
		return true;
	}

	ba = mRawFormat.mBlockAlign;

	lAudioPacket	= (long)lStart64 / mSamplesPerFrame;
	offset			= (long)lStart64 % mSamplesPerFrame;
	samples			= mSamplesPerFrame - offset;

	if ((uint32)samples > lCount) samples = lCount;

	if (lpBuffer == NULL)
	{
		if (lSamplesRead) *lSamplesRead = samples;
		if (lBytesRead) *lBytesRead = samples * ba;
		return true;
	}

	if (cbBuffer < (uint32)(samples * ba))
	{
		if (cbBuffer < (uint32)ba)
		{
			if (lSamplesRead) *lSamplesRead = samples;
			if (lBytesRead) *lBytesRead = samples * ba;
			return false;
		}
		samples = cbBuffer / ba;
	}

	// Because of the overlap from the subband synthesis window, we must
	// decode the previous packet in order to avoid glitches in the output.
	// For layer III, we must also ensure that at least 511 bytes are present
	// in the bit reservoir, as packets may dig that far back using
	// main_data_begin.  An MPEG-1 stereo stream may use up to 32 bytes of
	// the data payload for sideband information.

	if (lCurrentPacket != lAudioPacket)
	{
		try {
			long nDecodeStart;

			// Keep Priss up-to-date
			UpdateCPUflags();

			if (layer!=3 || lCurrentPacket<0 || lCurrentPacket+1 != lAudioPacket)
			{
				iad->Reset();

				lCurrentPacket = lAudioPacket;
				--lCurrentPacket;
				if (lCurrentPacket < 0) lCurrentPacket = 0;

				nDecodeStart = lCurrentPacket;

				// layer III: add packets to preload bit reservoir
				if (layer == 3)
				{
					long nReservoirDelay = 511;		// main_data_start is 9 bits (0..511)
					
					while(lCurrentPacket > 0 && nReservoirDelay > 0)
					{
						--lCurrentPacket;
						nReservoirDelay -= MPEGAudioCompacted(parentPtr->audio_sample_list[id][lCurrentPacket].header).GetPayloadSizeL3();
					}
				}
			}
			else
			{
				lCurrentPacket	= lAudioPacket;
				nDecodeStart	= lAudioPacket;
			}

			do {
				msi = &(parentPtr->audio_sample_list[id][lCurrentPacket]);
				len = msi->size;

				parentPtr->ReadSample(pkt_buffer, lCurrentPacket, len, id + 1);

				pDecoderPoint = (char *)pkt_buffer;
				pDecoderLimit = (char *)pkt_buffer + len;

				try {
					iad->SetDestination((sint16 *)sample_buffer);
					iad->ReadHeader();

					if (lCurrentPacket < nDecodeStart) {
						iad->PrereadFrame();
					} else {
						iad->DecodeFrame();
					}
				} catch(...) {
					throw;
				}

			} while (++lCurrentPacket <= lAudioPacket);

			--lCurrentPacket;

		} catch(int i) {
			//parentPtr->mContext.mpCallbacks->SetError("MPEG audio decoder internal error.");
			parentPtr->mContext.mpCallbacks->SetError(iad->GetErrorString(i));
			if (lSamplesRead) *lSamplesRead = 0;
			if (lBytesRead) *lBytesRead = 0;
			return false;
		}
	}

	memcpy(lpBuffer, (char *)sample_buffer + offset * ba, samples * ba);

	if (lSamplesRead) *lSamplesRead = samples;
	if (lBytesRead) *lBytesRead = samples * ba;

	return true;
}

const void *VDXAPIENTRY AudioSourceMPEG2::GetDirectFormat() {
	if (iad != NULL) return &mRawFormat;
	return NULL;
}

int VDXAPIENTRY AudioSourceMPEG2::GetDirectFormatLen() {
	if (iad != NULL) return sizeof(mRawFormat);
	return 0;
}

IVDXStreamSource::ErrorMode VDXAPIENTRY AudioSourceMPEG2::GetDecodeErrorMode() {
	return IVDXStreamSource::kErrorModeReportAll;
}

void VDXAPIENTRY AudioSourceMPEG2::SetDecodeErrorMode(IVDXStreamSource::ErrorMode mode) {
}

bool VDXAPIENTRY AudioSourceMPEG2::IsDecodeErrorModeSupported(IVDXStreamSource::ErrorMode mode) {
	return mode == IVDXStreamSource::kErrorModeReportAll;
}

bool VDXAPIENTRY AudioSourceMPEG2::IsVBR() {
	return false;
}

sint64 VDXAPIENTRY AudioSourceMPEG2::TimeToPositionVBR(sint64 us) {
	if (iad == NULL) return 0;

	return (sint64)(0.5 +
		((double)us * (double)mRawFormat.mSamplesPerSec) / 1000000.0
	);
}

sint64 VDXAPIENTRY AudioSourceMPEG2::PositionToTimeVBR(sint64 samples) {
	if (iad == NULL) return 0;

	return (sint64)(0.5 +
		((double)samples * 1000000.0) / (double)mRawFormat.mSamplesPerSec
	);
}

void VDXAPIENTRY AudioSourceMPEG2::GetAudioSourceInfo(VDXAudioSourceInfo& info) {
	info.mFlags = 0;
}

// IVDMPEGAudioBitsource methods

int AudioSourceMPEG2::read(void *buffer, int bytes) {
	const int remains = (int)(pDecoderLimit - pDecoderPoint);
	if (bytes > remains) bytes = remains;
	if (bytes > 0) {
		memcpy(buffer, pDecoderPoint, bytes);
		pDecoderPoint += bytes;
		return bytes;
	}
	return 0;
}



//////////////////////////////////////////////////////////////////////////

class AudioSourceMPEG : public vdxunknown<IVDXStreamSource>, public IVDXAudioSource {
public:
	AudioSourceMPEG(InputFileMPEG2 *const pp, unsigned int stream_id);
	~AudioSourceMPEG();

	int VDXAPIENTRY AddRef();
	int VDXAPIENTRY Release();
	void *VDXAPIENTRY AsInterface(uint32 iid);

	void		VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo&);
	bool		VDXAPIENTRY Read(sint64 lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);

	const void *VDXAPIENTRY GetDirectFormat();
	int			VDXAPIENTRY GetDirectFormatLen();

	ErrorMode VDXAPIENTRY GetDecodeErrorMode();
	void VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode);
	bool VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode);

	bool VDXAPIENTRY IsVBR();
	sint64 VDXAPIENTRY TimeToPositionVBR(sint64 us);
	sint64 VDXAPIENTRY PositionToTimeVBR(sint64 samples);

	void VDXAPIENTRY GetAudioSourceInfo(VDXAudioSourceInfo& info);

protected:
	char *mRawFormat;

	InputFileMPEG2 *const parentPtr;
	const unsigned int id;

private:
	// The largest MPEG audio frame is 1441 bytes
	unsigned char frame_buffer[1536];

	int mSamplesPerFrame;
	uint32 mFrameSize;
	int mFormatSize;

	uint32 prev_block;
	uint32 cur_block;
	sint64 cur_pos;

	uint32 mLength;
};


AudioSourceMPEG::AudioSourceMPEG(InputFileMPEG2 *const pp, unsigned int stream_id)
	: parentPtr(pp)
	, id(stream_id)
{
	parentPtr->AddRef();

	mFormatSize = 0;
	mRawFormat = NULL;
	mLength = 0;

	prev_block = -1;
	cur_block = 0;
	cur_pos = 0;

	if (parentPtr->aframes[id] > 0) {
		int layer;

		MPEGAudioCompacted hdr(parentPtr->audio_sample_list[id][0].header);

		layer = hdr.GetLayer();

		if (layer == 1 || layer == 2) {

			mRawFormat = new char[sizeof(MPEG1WAVEFORMAT)];
			if (mRawFormat != NULL) {
				MPEG1WAVEFORMAT *wfex1 = (MPEG1WAVEFORMAT *)mRawFormat;
				uint32 total;
				bool pad;

				mFrameSize = hdr.GetFrameSize();

				if (layer == 1) {
					mSamplesPerFrame = 384;
				} else {
					mSamplesPerFrame = 1152;
				}
				if (hdr.IsMPEG2()) mSamplesPerFrame >>= 1;

				// Check the stream for padding; also take this
				// opportunity to set the mode and modext flags
				total = 0;
				pad = false;
				wfex1->fwHeadMode = 0;
				wfex1->fwHeadModeExt = 0;

				for (unsigned int i = 0; i < parentPtr->aframes[id]; ++i) {
					MPEGAudioCompacted h(parentPtr->audio_sample_list[id][i].header);
					total += parentPtr->audio_sample_list[id][i].size;
					if (h.IsPadded() != hdr.IsPadded()) pad = true;
					wfex1->fwHeadMode |= (1 << h.GetMode());
					wfex1->fwHeadModeExt |= (1 << h.GetModeExt());
				}

				// If padded, mLength is the total number
				// of bytes, and nBlockAlign is 1

				if (pad) {
					mLength = total;
					wfex1->wfx.nBlockAlign = 1;
					// Use the unpadded frame size
					if (layer == 1) {
						if (!hdr.IsPadded()) mFrameSize += 4;
					} else {
						if (!hdr.IsPadded()) ++mFrameSize;
					}
				} else {
					// Unpadded
					wfex1->wfx.nBlockAlign = mFrameSize;
					mLength = parentPtr->aframes[id];
				}

	// Calculate nAvgBytesPerSec exactly!
	// a / (b / c) = (a * c) / b

	// framespersecond = samplespersec / samplesperframe
	// nseconds = nframes / (samplespersec / samplesperframe)
	// nseconds = (nframes * samplesperframe) / samplespersec

	// avgbytespersec = round:
	// (totalbytes * samplespersec) / (nframes * samplesperframe)

				wfex1->wfx.nAvgBytesPerSec = (DWORD)(
					((double)total * (double)hdr.GetSamplingRateHz())
					/ ((double)parentPtr->aframes[id] * mSamplesPerFrame)
					+ 0.5);


				// For 44100 Hz MPEG audio at 224 kbps
				// the Qdesign Layer 2 codec writes the following:
				// wFormatTag = 0x50
				// nChannels = 2
				// nSamplesPerSec = 44100
				// nBlockAlign = 731 (unpadded frame size)
				// nAvgBytesPerSec = 28000
				// wBitsPerSample = 0
				// cbSize = 22
				// fwHeadLayer = 2
				// dwHeadBitrate = 224000
				// fwHeadMode = 3
				// fwHeadModeExt = 15
				// wHeadEmphasis = 1
				// fwHeadFlags = 0x18 (ID_MPEG1 | PROTECTIONBIT)

				mFormatSize = sizeof(MPEG1WAVEFORMAT);

				wfex1->wfx.wFormatTag		= WAVE_FORMAT_MPEG;
				wfex1->wfx.nChannels		= hdr.IsStereo()? 2: 1;
				wfex1->wfx.nSamplesPerSec	= hdr.GetSamplingRateHz();
//				wfex1->wfx.nBlockAlign		= mFrameSize;
//				wfex1->wfx.nAvgBytesPerSec	= hdr.GetBitrateKbps() * 125;
				wfex1->wfx.wBitsPerSample	= 0;
				wfex1->wfx.cbSize			= 22;
				wfex1->fwHeadLayer			= layer;
				wfex1->dwHeadBitrate		= hdr.GetBitrateKbps() * 1000;
//				wfex1->fwHeadMode			= hdr.IsStereo()? 3: (1 << hdr.GetMode());
//				wfex1->fwHeadModeExt		= hdr.IsStereo()? 0x0F: 0;
				wfex1->wHeadEmphasis		= 1;
				wfex1->fwHeadFlags			= hdr.IsMPEG2()? 0: ACM_MPEG_ID_MPEG1;
				if (hdr.IsCRCProtected()) wfex1->fwHeadFlags |= ACM_MPEG_PROTECTIONBIT;
				wfex1->dwPTSLow				= 0;
				wfex1->dwPTSHigh			= 0;
			}

		} else if (layer == 3) {

			mRawFormat = new char[sizeof(MPEGLAYER3WAVEFORMAT)];
			if (mRawFormat != NULL) {
				MPEGLAYER3WAVEFORMAT *wfex3 = (MPEGLAYER3WAVEFORMAT *)mRawFormat;
				uint32 total;
				bool pad;

				mFrameSize = hdr.GetFrameSize();

				if (hdr.IsMPEG2()) {
					mSamplesPerFrame = 576;
				} else {
					mSamplesPerFrame = 1152;
				}

				// Check the stream for padding
				total = 0;
				pad = false;

				for (unsigned int i = 0; i < parentPtr->aframes[id]; ++i) {
					MPEGAudioCompacted h(parentPtr->audio_sample_list[id][i].header);
					total += parentPtr->audio_sample_list[id][i].size;
					if (h.IsPadded() != hdr.IsPadded()) pad = true;
				}

				if (pad) {
					wfex3->fdwFlags	= MPEGLAYER3_FLAG_PADDING_ON;
					wfex3->wfx.nBlockAlign = 1;
					// Write the unpadded frame size,
					// but use the padded frame size
					if (hdr.IsPadded()) {
						wfex3->nBlockSize = mFrameSize - 1;
					} else {
						wfex3->nBlockSize = mFrameSize++;
					}
					mLength = total;

				} else {
					// Unpadded
					wfex3->fdwFlags	= MPEGLAYER3_FLAG_PADDING_ISO;
					wfex3->wfx.nBlockAlign = mFrameSize;
					wfex3->nBlockSize = mFrameSize;
					mLength = parentPtr->aframes[id];
				}

				// Calculate nAvgBytesPerSec exactly!

				wfex3->wfx.nAvgBytesPerSec = (DWORD)(
					((double)total * (double)hdr.GetSamplingRateHz())
					/ ((double)parentPtr->aframes[id] * mSamplesPerFrame)
					+ 0.5);

				mFormatSize = sizeof(MPEGLAYER3WAVEFORMAT);

				wfex3->wfx.wFormatTag		= WAVE_FORMAT_MPEGLAYER3;
				wfex3->wfx.nChannels		= hdr.IsStereo()? 2: 1;
				wfex3->wfx.nSamplesPerSec	= hdr.GetSamplingRateHz();
//				wfex3->wfx.nAvgBytesPerSec	= hdr.GetBitrateKbps() * 125;
//				wfex3->wfx.nBlockAlign		= mFrameSize;
				wfex3->wfx.wBitsPerSample	= 0;
				wfex3->wfx.cbSize			= MPEGLAYER3_WFX_EXTRA_BYTES;
				wfex3->wID					= MPEGLAYER3_ID_MPEG;
//				wfex3->fdwFlags				= MPEGLAYER3_FLAG_PADDING_ON;
//				wfex3->nBlockSize			= mFrameSize;
				wfex3->nFramesPerBlock		= 1;
				wfex3->nCodecDelay			= 1393;
			}
		}
	}
}

AudioSourceMPEG::~AudioSourceMPEG() {
	delete[] mRawFormat;
	parentPtr->Release();
}

int VDXAPIENTRY AudioSourceMPEG::AddRef() {
	return vdxunknown<IVDXStreamSource>::AddRef();
}

int VDXAPIENTRY AudioSourceMPEG::Release() {
	return vdxunknown<IVDXStreamSource>::Release();
}

void *VDXAPIENTRY AudioSourceMPEG::AsInterface(uint32 iid) {
	if (iid == IVDXVideoSource::kIID)
		return static_cast<IVDXAudioSource *>(this);

	return vdxunknown<IVDXStreamSource>::AsInterface(iid);
}

void VDXAPIENTRY AudioSourceMPEG::GetStreamSourceInfo(VDXStreamSourceInfo& srcInfo) {
	const WAVEFORMATEX *wfex = (WAVEFORMATEX *)mRawFormat;
	srcInfo.mSampleRate.mNumerator = wfex->nAvgBytesPerSec;
	srcInfo.mSampleRate.mDenominator = wfex->nBlockAlign;
	srcInfo.mSampleCount = mLength;
}

bool VDXAPIENTRY AudioSourceMPEG::Read(sint64 lStart64, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	const MPEGSampleInfo *msi;
	long samples;
	long bytes;

	if ((uint32)lStart64 >= mLength) {
		if (lSamplesRead) *lSamplesRead = 0;
		if (lBytesRead) *lBytesRead = 0;
		return true;
	}

	if (((WAVEFORMATEX *)mRawFormat)->nBlockAlign == 1) {

//_RPT4(_CRT_WARN, "AudioSourceMPEG::Read %i, %i, %08X, %i\n",
//	(int)lStart64, (int)lCount, (int)lpBuffer, (int)cbBuffer);

		if (lpBuffer == NULL) {
			if (lSamplesRead) *lSamplesRead = mFrameSize;
			if (lBytesRead) *lBytesRead = mFrameSize;
			return true;
		}

		if (lCount < 1 || cbBuffer < 1) {
			if (lSamplesRead) *lSamplesRead = 1;
			if (lBytesRead) *lBytesRead = 1;
			return false;
		}

		const MPEGSampleInfo *msi = &(parentPtr->audio_sample_list[id][cur_block]);
		uint32 offset;

		if (lStart64 < cur_pos) {
			prev_block = -1;
			while (cur_block > 0) {
				cur_pos -= msi->size;
				--cur_block;
				--msi;
				if (lStart64 >= cur_pos) break;
			}
		} else {
			while (lStart64 >= (cur_pos + msi->size)) {
				if (cur_block == (parentPtr->aframes[id] - 1U)) {
					parentPtr->mContext.mpCallbacks->SetError("Attempt to read past end of audio");
					if (lSamplesRead) *lSamplesRead = 0;
					if (lBytesRead) *lBytesRead = 0;
					return false;
				}
				cur_pos += msi->size;
				++cur_block;
				++msi;
			}
		}

		bytes = 0;
		samples = 0;
		offset = (uint32)(lStart64 - cur_pos);

		if (cur_block != prev_block) {
			parentPtr->ReadSample(frame_buffer, cur_block, msi->size, id + 1);
		}

		while (lCount > 0) {
			uint32 remains = msi->size - offset;
			if (remains > lCount) remains = lCount;
			if ((bytes + remains) > cbBuffer) break;
			memcpy(lpBuffer, frame_buffer + offset, remains);
			lCount -= remains;
			bytes += remains;
			if (lCount == 0) break;
			if (cur_block == (parentPtr->aframes[id] - 1U)) {
				parentPtr->mContext.mpCallbacks->SetError("Attempt to read past end of audio");
				break;
			}
			lpBuffer = (char *)lpBuffer + remains;
			cur_pos += msi->size;
			++cur_block;
			++msi;
			parentPtr->ReadSample(frame_buffer, cur_block, msi->size, id + 1);
			offset = 0;
		}

		samples = bytes;
		prev_block = cur_block;

	} else {

		if (lpBuffer == NULL || cbBuffer < mFrameSize) {
			if (lSamplesRead) *lSamplesRead = 1;
			if (lBytesRead) *lBytesRead = mFrameSize;
			return (lpBuffer == NULL);
		}

		uint32 lStart = (uint32)lStart64;

		if (lCount == 0 || (lCount * mFrameSize) > cbBuffer) {
			lCount = cbBuffer / mFrameSize;
		}

		bytes = 0;
		samples = 0;
		msi = &(parentPtr->audio_sample_list[id][lStart]);

		while (lCount > 0) {
			if (cbBuffer < (uint32)msi->size) break;
			parentPtr->ReadSample(lpBuffer, lStart, msi->size, id + 1);
			bytes += msi->size;
			++samples;
			++msi;

			lpBuffer = (char *)lpBuffer + msi->size;
			cbBuffer -= msi->size;
			++lStart;
			--lCount;
		}

	}

	if (lSamplesRead) *lSamplesRead = samples;
	if (lBytesRead) *lBytesRead = bytes;
	return true;
}

const void *VDXAPIENTRY AudioSourceMPEG::GetDirectFormat() {
	return mRawFormat;
}

int VDXAPIENTRY AudioSourceMPEG::GetDirectFormatLen() {
	return mFormatSize;
}

IVDXStreamSource::ErrorMode VDXAPIENTRY AudioSourceMPEG::GetDecodeErrorMode() {
	return IVDXStreamSource::kErrorModeReportAll;
}

void VDXAPIENTRY AudioSourceMPEG::SetDecodeErrorMode(IVDXStreamSource::ErrorMode mode) {
}

bool VDXAPIENTRY AudioSourceMPEG::IsDecodeErrorModeSupported(IVDXStreamSource::ErrorMode mode) {
	return mode == IVDXStreamSource::kErrorModeReportAll;
}

bool VDXAPIENTRY AudioSourceMPEG::IsVBR() {
	// TODO
	return false;
}

sint64 VDXAPIENTRY AudioSourceMPEG::TimeToPositionVBR(sint64 us) {
	return 0;
}

sint64 VDXAPIENTRY AudioSourceMPEG::PositionToTimeVBR(sint64 samples) {
	return 0;
}

void VDXAPIENTRY AudioSourceMPEG::GetAudioSourceInfo(VDXAudioSourceInfo& info) {
	info.mFlags = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//					AudioSourceAC3
//
//
//////////////////////////////////////////////////////////////////////////

// Adapted from liba52
// (These functions are global)

static const int rate[19] = {
	32, 40, 48, 56, 64, 80, 96, 112, 128, 160,
	192, 224, 256, 320, 384, 448, 512, 576, 640
};

// Bug fix for version 2.3
// The third entry doesn't account for dsurmod!

static const unsigned short lfeon[8] = {
//	0x1000, 0x1000, 0x1000, 0x0400,
	0x1000, 0x1000, 0x0400, 0x0400,
	0x0400, 0x0100, 0x0400, 0x0100
};

static const char chan[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };

// bytes 4-7
// xx...... ........ ........ ........  fscod (00=48, 01=44.1, 10=32, 11=reserved)
// ..xxxxxx ........ ........ ........  frmsizecod (0 to 37)
// ........ xxxxx... ........ ........  bsid (01000 = version 8)
// ........ .....xxx ........ ........  bsmod
// ........ ........ xxx..... ........  acmod (#channels)
// ........ ........ ...x.x.x ........  lfe (position depends on acmod)

unsigned short ac3_compact_header(const unsigned long hdr) {
	const int acmod = (hdr >> 13) & 7;
	int bsid = (hdr >> 19) & 31;

	if (bsid > 8) bsid -= 8; else bsid = 0;

	return (unsigned short)(
		((hdr >> 16) & 0xFF00) |
		(bsid << 4) | (acmod << 1) |
		(!!(hdr & lfeon[acmod]))
	);
}

// Compact ac3 header info by eliminating unneeded bits
// xx...... ........  fscod (00=48, 01=44.1, 10=32, 11=reserved)
// ..xxxxxx ........  frmsizecod (0 to 37)
// ........ xxxx....  bsid (1000 = version 8)
// ........ ....xxx.  acmod (#channels)
// ........ .......x  lfe (derived at compaction time)

int ac3_compact_info(const unsigned short hdr, int *srate, int *brate, int *chans) {
	const int frmsizecod = (hdr >> 8) & 0x3F;
	const int acmod = (hdr >> 1) & 7;
	const int bitrate = rate[frmsizecod >> 1];
	int bsid = (hdr >> 4) & 15;

// We need not verify anything here, because
// any compacted header has already been verified.

	if (chans) *chans = chan[acmod] + (hdr & 1);
	if (brate) *brate = bitrate >> bsid;

	switch (hdr & 0xC000) {
		case 0:
			if (srate) *srate = 48000 >> bsid;
			return (4 * bitrate);
		case 0x4000:
			if (srate) *srate = 44100 >> bsid;
			return (2 * (320 * bitrate / 147 + (frmsizecod & 1)));
		case 0x8000:
			if (srate) *srate = 32000 >> bsid;
			return (6 * bitrate);
	}
	return 0;
}

// bytes 4-7
// xx...... ........ ........ ........  fscod (00=48, 01=44.1, 10=32, 11=reserved)
// ..xxxxxx ........ ........ ........  frmsizecod (0 to 37)
// ........ xxxxx... ........ ........  bsid (01000 = version 8)
// ........ .....xxx ........ ........  bsmod
// ........ ........ xxx..... ........  acmod (#channels)
// ........ ........ ...x.x.x ........  lfe (position depends on acmod)

int ac3_sync_info(const unsigned long hdr, long *srate, int *brate, int *chans) {

	if (hdr < 0xC0000000) {
		const int frmsizecod = (hdr >> 24) & 63;
		if (frmsizecod < 38) {
			int bsid = (hdr >> 19) & 31;
			if (bsid < 12) {
				const int acmod = (hdr >> 13) & 7;
				const int bitrate = rate[frmsizecod >> 1];

				if (bsid > 8) bsid -= 8; else bsid = 0;
				if (chans) *chans = chan[acmod] + !!(hdr & lfeon[acmod]);
				if (brate) *brate = bitrate >> bsid;

				switch (hdr >> 30) {
					case 0:
						if (srate) *srate = 48000 >> bsid;
						return (4 * bitrate);
					case 1:
						if (srate) *srate = 44100 >> bsid;
						return (2 * (320 * bitrate / 147 + (frmsizecod & 1)));
					case 2:
						if (srate) *srate = 32000 >> bsid;
						return (6 * bitrate);
				}
			}
		}
	}
	return 0;
}


class AudioSourceAC3 : public vdxunknown<IVDXStreamSource>, public IVDXAudioSource {
public:
	AudioSourceAC3(InputFileMPEG2 *const pp, const unsigned int stream_id);
	~AudioSourceAC3();

	int VDXAPIENTRY AddRef();
	int VDXAPIENTRY Release();
	void *VDXAPIENTRY AsInterface(uint32 iid);

	void		VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo&);
	bool		VDXAPIENTRY Read(sint64 lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);

	const void *VDXAPIENTRY GetDirectFormat();
	int			VDXAPIENTRY GetDirectFormatLen();

	ErrorMode VDXAPIENTRY GetDecodeErrorMode();
	void VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode);
	bool VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode);

	bool VDXAPIENTRY IsVBR();
	sint64 VDXAPIENTRY TimeToPositionVBR(sint64 us);
	sint64 VDXAPIENTRY PositionToTimeVBR(sint64 samples);

	void VDXAPIENTRY GetAudioSourceInfo(VDXAudioSourceInfo& info);

protected:
	VDXWAVEFORMATEX mRawFormat;

private:
	InputFileMPEG2 *const parentPtr;
	const unsigned int id;
	sint64 mLength;
};

AudioSourceAC3::AudioSourceAC3(InputFileMPEG2 *const pp, const unsigned int stream_id)
	: parentPtr(pp)
	, id(stream_id)
{
	int frmsize, chans, brate;
	int srate;

	parentPtr->AddRef();
	mLength = 0;

_RPT0(_CRT_WARN, "AudioSourceAC3 constructed\n");

	// Sanity checks
	if (id >= 48U) return;
	if (parentPtr->audio_sample_list[id] == NULL) return;
	if (parentPtr->aframes[id] <= 0) return;

	frmsize = ac3_compact_info(
		parentPtr->audio_sample_list[id][0].header,
		&srate, &brate, &chans);

	if (frmsize) {
		mLength = parentPtr->aframes[id];

		mRawFormat.mFormatTag		= 0x2000;
		mRawFormat.mChannels		= chans;
		mRawFormat.mSamplesPerSec	= srate;
		mRawFormat.mAvgBytesPerSec	= brate * 125;
		mRawFormat.mBlockAlign		= frmsize;
		mRawFormat.mBitsPerSample	= 0;
		mRawFormat.mExtraSize		= 0;
	}
}

AudioSourceAC3::~AudioSourceAC3() {
_RPT0(_CRT_WARN, "AudioSourceAC3 destructed\n");
	parentPtr->Release();
}

int VDXAPIENTRY AudioSourceAC3::AddRef() {
	return vdxunknown<IVDXStreamSource>::AddRef();
}

int VDXAPIENTRY AudioSourceAC3::Release() {
	return vdxunknown<IVDXStreamSource>::Release();
}

void *VDXAPIENTRY AudioSourceAC3::AsInterface(uint32 iid) {
	if (iid == IVDXAudioSource::kIID)
		return static_cast<IVDXAudioSource *>(this);

	if (iid == IVDXStreamSource::kIID)
		return static_cast<IVDXStreamSource *>(this);

	return NULL;
}

void VDXAPIENTRY AudioSourceAC3::GetStreamSourceInfo(VDXStreamSourceInfo& srcInfo) {
	if (mLength) {
		srcInfo.mSampleRate.mNumerator = mRawFormat.mAvgBytesPerSec;
		srcInfo.mSampleRate.mDenominator = mRawFormat.mBlockAlign;
		srcInfo.mSampleCount = mLength;
	} else {
		srcInfo.mSampleRate.mNumerator = 0;
		srcInfo.mSampleRate.mDenominator = 0;
		srcInfo.mSampleCount = 0;
	}
}

bool VDXAPIENTRY AudioSourceAC3::Read(sint64 lStart64, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {

	int ba = mRawFormat.mBlockAlign;

	if (lStart64 < 0 || lStart64 >= mLength) {
		if (lSamplesRead) *lSamplesRead = 0;
		if (lBytesRead) *lBytesRead = 0;
		return true;
	}

	if (lpBuffer == NULL || lCount < 1 || cbBuffer < (uint32)ba) {
		if (lSamplesRead) *lSamplesRead = 1;
		if (lBytesRead) *lBytesRead = ba;
		return (lpBuffer == NULL);
	}

	if (lSamplesRead) *lSamplesRead = lCount;
	lCount *= ba;
	parentPtr->ReadSample(lpBuffer, (long)lStart64, lCount, id + 1);

	if (lBytesRead) *lBytesRead = lCount;

	return true;
}

const void *VDXAPIENTRY AudioSourceAC3::GetDirectFormat() {
	if (mLength) return &mRawFormat;
	return NULL;
}

int VDXAPIENTRY AudioSourceAC3::GetDirectFormatLen() {
	if (mLength) return sizeof(mRawFormat);
	return 0;
}

IVDXStreamSource::ErrorMode VDXAPIENTRY AudioSourceAC3::GetDecodeErrorMode() {
	return IVDXStreamSource::kErrorModeReportAll;
}

void VDXAPIENTRY AudioSourceAC3::SetDecodeErrorMode(IVDXStreamSource::ErrorMode mode) {
}

bool VDXAPIENTRY AudioSourceAC3::IsDecodeErrorModeSupported(IVDXStreamSource::ErrorMode mode) {
	return mode == IVDXStreamSource::kErrorModeReportAll;
}

bool VDXAPIENTRY AudioSourceAC3::IsVBR() {
	return false;
}

	// sample = (ms * dwRate) / (dwScale * 1000)
	// sample * dwScale * 1000 = (ms * dwRate)
	// ms = (sample * dwScale * 1000) / dwRate

//	srcInfo.mSampleRate.mNumerator = mRawFormat.mAvgBytesPerSec;
//	srcInfo.mSampleRate.mDenominator = mRawFormat.mBlockAlign;

sint64 VDXAPIENTRY AudioSourceAC3::TimeToPositionVBR(sint64 us) {
	if (mLength == 0) return 0;

	return (sint64)(0.5 +
		((double)mRawFormat.mAvgBytesPerSec * (double)us) /
		((double)mRawFormat.mBlockAlign * 1000000.0)
	);
}

sint64 VDXAPIENTRY AudioSourceAC3::PositionToTimeVBR(sint64 samples) {
	if (mLength == 0) return 0;

	return (sint64)(0.5 +
		((double)mRawFormat.mBlockAlign * (double)samples * 1000000.0)
		/ (double)mRawFormat.mAvgBytesPerSec
	);
}

void VDXAPIENTRY AudioSourceAC3::GetAudioSourceInfo(VDXAudioSourceInfo& info) {
	info.mFlags = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//					AudioSourceLPCM
//
//
//////////////////////////////////////////////////////////////////////////

class AudioSourceLPCM : public vdxunknown<IVDXStreamSource>, public IVDXAudioSource {
public:
	AudioSourceLPCM(InputFileMPEG2 *const pp, const unsigned int stream_id);
	~AudioSourceLPCM();

	int VDXAPIENTRY AddRef();
	int VDXAPIENTRY Release();
	void *VDXAPIENTRY AsInterface(uint32 iid);

	void		VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo&);
	bool		VDXAPIENTRY Read(sint64 lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);

	const void *VDXAPIENTRY GetDirectFormat();
	int			VDXAPIENTRY GetDirectFormatLen();

	ErrorMode VDXAPIENTRY GetDecodeErrorMode();
	void VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode);
	bool VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode);

	bool VDXAPIENTRY IsVBR();
	sint64 VDXAPIENTRY TimeToPositionVBR(sint64 us);
	sint64 VDXAPIENTRY PositionToTimeVBR(sint64 samples);

	void VDXAPIENTRY GetAudioSourceInfo(VDXAudioSourceInfo& info);

protected:
	VDXWAVEFORMATEX mRawFormat;

private:
	InputFileMPEG2 *const parentPtr;
	const unsigned int id;

	sint64 mLength;

	// The output is in blocks of 1536 samples,
	// always mono or stereo, and always 16-bit...
	// But our input may be multichannel 24-bit audio,
	// so we will need an asymmetric read buffer.

	// The buffer size we need is
	//		PCM_BUFFER_SIZE (1536 samples)
	//		* (maximum number of channels we support)
	//		* (maximum bits per sample / 8)

	unsigned char sample_buffer[PCM_BUFFER_SIZE * 2 * 3];
	long lCurrentPacket;

	inline void LPCM16_to_WAVE16(void *src, int len);
	inline void LPCM20M_to_WAVE16M(void *src, int len);
	inline void LPCM20S_to_WAVE16S(void *src, int len);
	inline void LPCM24M_to_WAVE16M(void *src, int len);
	inline void LPCM24S_to_WAVE16S(void *src, int len);
};

AudioSourceLPCM::AudioSourceLPCM(InputFileMPEG2 *const pp, const unsigned int stream_id)
	: parentPtr(pp)
	, id(stream_id)
{
	unsigned short hdr;

	parentPtr->AddRef();
	mLength = 0;

_RPT0(_CRT_WARN, "AudioSourceLPCM constructed\n");

	// Sanity checks
	if (id >= 48U) return;
	if (parentPtr->audio_sample_list[id] == NULL) return;
	if (parentPtr->aframes[id] <= 0) return;

	mLength = parentPtr->aframes[id] * PCM_BUFFER_SIZE;
	lCurrentPacket = -1;

	// audio_first_header is:
	// 11111111 ........ ........ ........	non-zero
	// ........ x....... ........ ........	emphasis
	// ........ .x...... ........ ........	mute
	// ........ ..x..... ........ ........	reserved
	// ........ ...xxxxx ........ ........	frame number % 20
	// ........ ........ xxxxxxxx ........	dynamic range control
	// ........ ........ ........ xx......	00=16-,01=20-,10=24-bit (11=forbidden)
	// ........ ........ ........ ..xx....	00=48,01=96 KHz (others reserved?)
	// ........ ........ ........ ....x...	reserved
	// ........ ........ ........ .....xxx	number of channels - 1

	hdr = parentPtr->audio_sample_list[id][0].header;

	mRawFormat.mFormatTag		= VDXWAVEFORMATEX::kFormatPCM;
	mRawFormat.mChannels		= (hdr & 7)? 2: 1;
	mRawFormat.mSamplesPerSec	= (hdr & 0x30)? 96000: 48000;
	mRawFormat.mBlockAlign		= mRawFormat.mChannels * 2;
	mRawFormat.mAvgBytesPerSec	= mRawFormat.mSamplesPerSec * mRawFormat.mBlockAlign;
	mRawFormat.mBitsPerSample	= 16;
	mRawFormat.mExtraSize		= 0;
}

AudioSourceLPCM::~AudioSourceLPCM() {
_RPT0(_CRT_WARN, "AudioSourceLPCM destructed\n");
	parentPtr->Release();
}

int VDXAPIENTRY AudioSourceLPCM::AddRef() {
	return vdxunknown<IVDXStreamSource>::AddRef();
}

int VDXAPIENTRY AudioSourceLPCM::Release() {
	return vdxunknown<IVDXStreamSource>::Release();
}

void *VDXAPIENTRY AudioSourceLPCM::AsInterface(uint32 iid) {
	if (iid == IVDXAudioSource::kIID)
		return static_cast<IVDXAudioSource *>(this);

	if (iid == IVDXStreamSource::kIID)
		return static_cast<IVDXStreamSource *>(this);

	return NULL;
}

void VDXAPIENTRY AudioSourceLPCM::GetStreamSourceInfo(VDXStreamSourceInfo& srcInfo) {
	if (mLength) {
		srcInfo.mSampleRate.mNumerator = mRawFormat.mSamplesPerSec;
		srcInfo.mSampleRate.mDenominator = 1;
		srcInfo.mSampleCount = mLength;
	} else {
		srcInfo.mSampleRate.mNumerator = 0;
		srcInfo.mSampleRate.mDenominator = 0;
		srcInfo.mSampleCount = 0;
	}
}

// All of the following are "in place" conversions

// None of them are optimized, but clarity
// here was more important than speed <g>

#if defined(_AMD64_) || defined(_M_AMD64)

inline void AudioSourceLPCM::LPCM16_to_WAVE16(void *src, int len) {

	unsigned char *esi = (unsigned char *)src;
	unsigned char i;

	while (len >= 2) {
		i = esi[0]; esi[0] = esi[1]; esi[1] = i;
		esi += 2;
		len -= 2;
	}
}

inline void AudioSourceLPCM::LPCM20S_to_WAVE16S(void *src, int len) {

	unsigned char *esi = (unsigned char *)src;
	unsigned char *edi = esi;
	unsigned char i;

	while (len >= 10) {
		i = esi[0]; edi[0] = esi[1]; edi[1] = i;
		i = esi[2]; edi[2] = esi[3]; edi[3] = i;
		i = esi[4]; edi[4] = esi[5]; edi[5] = i;
		i = esi[6]; edi[6] = esi[7]; edi[7] = i;
		esi += 10;
		edi += 8;
		len -= 10;
	}
}

inline void AudioSourceLPCM::LPCM20M_to_WAVE16M(void *src, int len) {
	
	unsigned char *esi = (unsigned char *)src;
	unsigned char *edi = esi;
	unsigned char i;
	
	while (len >= 5) {
		i = esi[0]; edi[0] = esi[1]; edi[1] = i;
		i = esi[2]; edi[2] = esi[3]; edi[3] = i;
		esi += 5;
		edi += 4;
		len -= 5;
	}
}

inline void AudioSourceLPCM::LPCM24M_to_WAVE16M(void *src, int len) {
	
	unsigned char *esi = (unsigned char *)src;
	unsigned char *edi = esi;
	unsigned char i;
	
	while (len >= 6) {
		i = esi[0]; edi[0] = esi[1]; edi[1] = i;
		i = esi[2]; edi[2] = esi[3]; edi[3] = i;
		esi += 6;
		edi += 4;
		len -= 6;
	}
}

inline void AudioSourceLPCM::LPCM24S_to_WAVE16S(void *src, int len) {

	unsigned char *esi = (unsigned char *)src;
	unsigned char *edi = esi;
	unsigned char i;

	while (len >= 12) {
		i = esi[0]; edi[0] = esi[1]; edi[1] = i;
		i = esi[2]; edi[2] = esi[3]; edi[3] = i;
		i = esi[4]; edi[4] = esi[5]; edi[5] = i;
		i = esi[6]; edi[6] = esi[7]; edi[7] = i;
		esi += 12;
		edi += 8;
		len -= 12;
	}
}

#else	// _AMD64_ || _M_AMD64

inline void AudioSourceLPCM::LPCM16_to_WAVE16(void *src, int len) {

// This is just a straightforward byte swap...

	__asm {
		mov		esi, src
		mov		ecx, len

		sar		ecx, 2
		js		done		; len < 0 not allowed
		jz		part2		; less than 4 bytes left

		align 16
loop1:	mov		eax, [esi]	; [3][2][1][0]
		bswap	eax
		rol		eax, 16
		mov		[esi], eax
		add		esi, 4
		dec		ecx
		jg		loop1

part2:	test	len, 2
		jz		done		; less than 2 bytes left

		mov		ax, word ptr [esi]
		xchg	ah, al
		mov		word ptr [esi], ax
done:
	}
}

inline void AudioSourceLPCM::LPCM20S_to_WAVE16S(void *src, int len) {
    __asm {
		mov		esi, src
		mov		ecx, len
		mov		edi, esi

	// This is how the LPCM stream is coded:
	//	buf[0]				=    top 8 bits of Left sample 1
	//  buf[1]				= middle 8 bits of Left sample 1
	//	buf[2]				=    top 8 bits of Right sample 1
	//	buf[3]				= middle 8 bits of Right sample 1
	//	buf[4]				=    top 8 bits of Left sample 2
	//  buf[5]				= middle 8 bits of Left sample 2
	//	buf[6]				=    top 8 bits of Right sample 2
	//	buf[7]				= middle 8 bits of Right sample 2
	//	buf[8] bits [7..4]	= bottom 4 bits of Left sample 1
	//  buf[8] bits [3..0]	= bottom 4 bits of Right sample 1
	//	buf[9] bits [7..4]	= bottom 4 bits of Left sample 2
	//  buf[9] bits [3..0]	= bottom 4 bits of Right sample 2

	// We convert to Intel 16-bit as shown:
	//	buf[0] = buf[1] = middle 8 bits of Left sample 1
	//	buf[1] = buf[0] =    top 8 bits of Left sample 1
	//	buf[2] = buf[3]	= middle 8 bits of Right sample 1
	//	buf[3] = buf[2] =    top 8 bits of Right sample 1
	//	buf[4] = buf[5] = middle 8 bits of Left sample 1
	//	buf[5] = buf[4] =    top 8 bits of Left sample 1
	//	buf[6] = buf[7]	= middle 8 bits of Right sample 1
	//	buf[7] = buf[6] =    top 8 bits of Right sample 1

		align 16
loop1:	sub		ecx, 5
		jl		done

		mov		eax, [esi]		; 3 2 1 0
		add		esi, 4
		bswap	eax				; 0 1 2 3
		ror		eax, 16			; 2 3 0 1
		mov		[edi], eax
		add		edi, 4

		sub		ecx, 5
		jl		done

		mov		eax, [esi]		; 7 6 5 4
		add		esi, 6
		bswap	eax				; 4 5 6 7
		ror		eax, 16			; 6 7 4 5
		mov		[edi], eax
		add		edi, 4

		jmp		loop1
done:
	}
}

inline void AudioSourceLPCM::LPCM20M_to_WAVE16M(void *src, int len) {
    __asm {
		mov		esi, src
		mov		ecx, len
		mov		edi, esi

	// This is how the LPCM stream is coded:
	//	buf[0]				=    top 8 bits of Sample 1
	//  buf[1]				= middle 8 bits of Sample 1
	//	buf[2]				=    top 8 bits of Sample 2
	//	buf[3]				= middle 8 bits of Sample 2
	//	buf[4] bits [7..4]	= bottom 4 bits of Sample 1
	//  buf[4] bits [3..0]	= bottom 4 bits of Sample 2

	// We convert to Intel 16-bit as shown:
	//	buf[0] = buf[1] = middle 8 bits of Sample 1
	//	buf[1] = buf[0] =    top 8 bits of Sample 1
	//	buf[2] = buf[3]	= middle 8 bits of Sample 2
	//	buf[3] = buf[2] =    top 8 bits of Sample 2

		align 16
loop1:	sub		ecx, 5
		jl		done

		mov		eax, [esi]		; 3 2 1 0
		add		esi, 5
		bswap	eax				; 0 1 2 3
		ror		eax, 16			; 2 3 0 1
		mov		[edi], eax
		add		edi, 4

		jmp		loop1
done:
	}
}

inline void AudioSourceLPCM::LPCM24M_to_WAVE16M(void *src, int len) {
    __asm {
		mov		esi, src
		mov		ecx, len
		mov		edi, esi

	// This is how the LPCM stream is coded:
	//	buf[0] =    top 8 bits of Sample 1
	//  buf[1] = middle 8 bits of Sample 1
	//	buf[2] =    top 8 bits of Sample 2
	//	buf[3] = middle 8 bits of Sample 2
	//	buf[4] = bottom 8 bits of Sample 1
	//  buf[5] = bottom 8 bits of Sample 2

	// We convert to Intel 16-bit as shown:
	//	buf[0] = buf[1] = middle 8 bits of Sample 1
	//	buf[1] = buf[0] =    top 8 bits of Sample 1
	//	buf[2] = buf[3]	= middle 8 bits of Sample 2
	//	buf[3] = buf[2] =    top 8 bits of Sample 2

		align 16
loop1:	sub		ecx, 6
		jl		done

		mov		eax, [esi]		; 3 2 1 0
		add		esi, 6
		bswap	eax				; 0 1 2 3
		ror		eax, 16			; 2 3 0 1
		mov		[edi], eax
		add		edi, 4

		jmp		loop1
done:
	}
}

inline void AudioSourceLPCM::LPCM24S_to_WAVE16S(void *src, int len) {
    __asm {
		mov		esi, src
		mov		ecx, len
		mov		edi, esi

	// This is how the LPCM stream is coded:
	//	buf[0] =    top 8 bits of Left sample 1
	//  buf[1] = middle 8 bits of Left sample 1
	//	buf[2] =    top 8 bits of Right sample 1
	//	buf[3] = middle 8 bits of Right sample 1
	//	buf[4] =    top 8 bits of Left sample 2
	//  buf[5] = middle 8 bits of Left sample 2
	//	buf[6] =    top 8 bits of Right sample 2
	//	buf[7] = middle 8 bits of Right sample 2
	//	buf[8] = bottom 8 bits of Left sample 1
	//  buf[9] = bottom 8 bits of Right sample 1
	//	buf[A] = bottom 8 bits of Left sample 2
	//	buf[B] = bottom 8 bits of Right sample 2

	// We convert to Intel 16-bit as shown:
	//	buf[0] = buf[1] = middle 8 bits of Left sample 1
	//	buf[1] = buf[0] =    top 8 bits of Left sample 1
	//	buf[2] = buf[3]	= middle 8 bits of Right sample 1
	//	buf[3] = buf[2] =    top 8 bits of Right sample 1
	//	buf[4] = buf[5] = middle 8 bits of Left sample 2
	//	buf[5] = buf[4] =    top 8 bits of Left sample 2
	//	buf[6] = buf[7] = middle 8 bits of Right sample 2
	//	buf[7] = buf[6] =    top 8 bits of Right sample 2

		align 16
loop1:	sub		ecx, 6
		jl		done

		mov		eax, [esi]		; 3 2 1 0
		add		esi, 4
		bswap	eax				; 0 1 2 3
		ror		eax, 16			; 2 3 0 1
		mov		[edi], eax
		add		edi, 4

		sub		ecx, 6
		jl		done

		mov		eax, [esi]		; 7 6 5 4
		add		esi, 8
		bswap	eax				; 4 5 6 7
		ror		eax, 16			; 6 7 4 5
		mov		[edi], eax
		add		edi, 4

		jmp		loop1
done:
	}
}

#endif	// _AMD64_ || _M_AMD64

bool VDXAPIENTRY AudioSourceLPCM::Read(sint64 lStart64, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	long lAudioPacket;
	long samples, offset;
	int ba = mRawFormat.mBlockAlign;

	if (lStart64 < 0 || lStart64 >= mLength) {
		if (lSamplesRead) *lSamplesRead = 0;
		if (lBytesRead) *lBytesRead = 0;
		return true;
	}

	lAudioPacket	= (long)lStart64 / PCM_BUFFER_SIZE;
	offset			= (long)lStart64 % PCM_BUFFER_SIZE;
	samples			= PCM_BUFFER_SIZE - offset;

	if ((uint32)samples > lCount) samples = lCount;

	if (lpBuffer == NULL) {
		if (lSamplesRead) *lSamplesRead = samples;
		if (lBytesRead) *lBytesRead = samples * ba;
		return true;

	} else if (cbBuffer < (uint32)(samples * ba)) {
		if (cbBuffer < (uint32)ba) {
			if (lSamplesRead) *lSamplesRead = 0;
			if (lBytesRead) *lBytesRead = 0;
			return false;
		}
		samples = cbBuffer / ba;
	}

	if (lCurrentPacket != lAudioPacket) {
		const MPEGSampleInfo *msi = &(parentPtr->audio_sample_list[id][lAudioPacket]);
		long len = msi->size;

		parentPtr->ReadSample(sample_buffer, lAudioPacket, len, id + 1);

	// audio_first_header is:
	// 11111111 ........ ........ ........	non-zero
	// ........ x....... ........ ........	emphasis
	// ........ .x...... ........ ........	mute
	// ........ ..x..... ........ ........	reserved
	// ........ ...xxxxx ........ ........	frame number % 20
	// ........ ........ xxxxxxxx ........	dynamic range control
	// ........ ........ ........ xx......	00=16-,01=20-,10=24-bit (11=forbidden)
	// ........ ........ ........ ..xx....	00=48,01=96 KHz (others reserved?)
	// ........ ........ ........ ....x...	reserved
	// ........ ........ ........ .....xxx	number of channels - 1

		switch (msi->header & 0xC0) {

		case 0x00:	// 16-bit LPCM
			LPCM16_to_WAVE16(sample_buffer, len);
			break;

		case 0x40:	// 20-bit LPCM
			if (msi->header & 1) {
				LPCM20S_to_WAVE16S(sample_buffer, len);
			} else {
				LPCM20M_to_WAVE16M(sample_buffer, len);
			}
			break;

		case 0x80:	// 24-bit LPCM
			if (msi->header & 1) {
				LPCM24S_to_WAVE16S(sample_buffer, len);
			} else {
				LPCM24M_to_WAVE16M(sample_buffer, len);
			}
			break;
		}

		lCurrentPacket = lAudioPacket;
	}

	memcpy(lpBuffer, (char *)sample_buffer + offset * ba, samples * ba);

	if (lSamplesRead) *lSamplesRead = samples;
	if (lBytesRead) *lBytesRead = samples * ba;
	return true;
}

const void *VDXAPIENTRY AudioSourceLPCM::GetDirectFormat() {
	if (mLength) return &mRawFormat;
	return NULL;
}

int VDXAPIENTRY AudioSourceLPCM::GetDirectFormatLen() {
	if (mLength) return sizeof(mRawFormat);
	return 0;
}

IVDXStreamSource::ErrorMode VDXAPIENTRY AudioSourceLPCM::GetDecodeErrorMode() {
	return IVDXStreamSource::kErrorModeReportAll;
}

void VDXAPIENTRY AudioSourceLPCM::SetDecodeErrorMode(IVDXStreamSource::ErrorMode mode) {
}

bool VDXAPIENTRY AudioSourceLPCM::IsDecodeErrorModeSupported(IVDXStreamSource::ErrorMode mode) {
	return mode == IVDXStreamSource::kErrorModeReportAll;
}

bool VDXAPIENTRY AudioSourceLPCM::IsVBR() {
	return false;
}

sint64 VDXAPIENTRY AudioSourceLPCM::TimeToPositionVBR(sint64 us) {
	if (mLength <= 0) return 0;

	return (sint64)(0.5 +
		((double)us * (double)mRawFormat.mSamplesPerSec) / 1000000.0
	);
}

sint64 VDXAPIENTRY AudioSourceLPCM::PositionToTimeVBR(sint64 samples) {
	if (mLength <= 0) return 0;

	return (sint64)(0.5 +
		((double)samples * 1000000.0) / (double)mRawFormat.mSamplesPerSec
	);
}

void VDXAPIENTRY AudioSourceLPCM::GetAudioSourceInfo(VDXAudioSourceInfo& info) {
	info.mFlags = 0;
}


IVDXAudioSource *CreateAudioSourceMPEG2(InputFileMPEG2 *const pp, const unsigned int stream_id) {
	return new AudioSourceMPEG2(pp, stream_id);
}

IVDXAudioSource *CreateAudioSourceMPEG(InputFileMPEG2 *const pp, const unsigned int stream_id) {
	return new AudioSourceMPEG(pp, stream_id);
}

IVDXAudioSource *CreateAudioSourceLPCM(InputFileMPEG2 *const pp, const unsigned int stream_id) {
	return new AudioSourceLPCM(pp, stream_id);
}

IVDXAudioSource *CreateAudioSourceAC3(InputFileMPEG2 *const pp, const unsigned int stream_id) {
	return new AudioSourceAC3(pp, stream_id);
}

