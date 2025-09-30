#ifndef f_AUDIO_H
#define f_AUDIO_H

#define PCM_BUFFER_SIZE	1536	// samples

// MPEG audio frame header (ISO/IEC 11172-3):

// 11111111 111..... ........ ........  syncword (all bits set)
// ........ ...xx... ........ ........  ID (11=1, 10=2, 00=2.5)
// ........ .....xx. ........ ........  layer (01=III, 10=II, 11=I)
// ........ .......x ........ ........  protection_bit (0=yes, 1=no)
// ........ ........ xxxx.... ........  bitrate_index (1111=forbidden)
// ........ ........ ....xx.. ........  sampling_frequency (00=44.1, 01=48, 10=32)
// ........ ........ ......x. ........  padding_bit (0=no, 1=yes)
// ........ ........ .......x ........  private_bit (unused)
// ........ ........ ........ xx......  mode (00=stereo,01=joint,10=dual,11=mono)
// ........ ........ ........ ..xx....  mode_extension
// ........ ........ ........ ....x...  copyright (0=no, 1=yes)
// ........ ........ ........ .....x..  original/copy (0=original, 1=copy)
// ........ ........ ........ ......xx  emphasis

// Compacted MPEG audio header for storage:
//		= (header & 0xFE00)
//		| ((header << 1) & 0x01E0)
//		| ((header >> 16) & 0x001F)

// xxxx.... ........	bitrate (1111 = forbidden)
// ....xx.. ........	sampling rate (11 = reserved)
// ......x. ........	padded (0=no, 1=yes)
// .......x x.......	mode (00=stereo, 01=joint, 10=dual, 11=mono)
// ........ .xx.....	mode extension
// ........ ...xx...	ID (11=1, 10=2, 00=2.5)
// ........ .....xx.	layer (01=III, 10=II, 11=I)
// ........ .......x	crc (0=yes, 1=no)


struct MPEGAudioCompacted {
	const unsigned short mHeader;

	MPEGAudioCompacted(unsigned short hdr) : mHeader(hdr) {}

	bool		IsMPEG2() const					{ return !(mHeader & 0x0008); }
	bool		IsMPEG25() const				{ return IsMPEG2() && !(mHeader & 0x0010); }
	unsigned	GetLayer() const				{ return 4 - ((mHeader >> 1) & 3); }
	bool		IsCRCProtected() const			{ return !(mHeader & 0x0001); }
	unsigned	GetBitrateIndex() const			{ return (mHeader >> 12) & 15; }
	unsigned	GetSamplingRateIndex() const	{ return (mHeader >> 10) & 3; }
	bool		IsPadded() const				{ return 0 != (mHeader & 0x0200); }
	bool		IsStereo() const				{ return (mHeader & 0x0180) != 0x0180; }

	unsigned	GetBitrateKbps() const;			//{ return bitrate[IsMPEG2()][GetLayer()-1][GetBitrateIndex()]; }
	unsigned	GetSamplingRateHz() const;		//{ return samp_freq[IsMPEG25()][IsMPEG2()][GetSamplingRateIndex()]; }

	unsigned	GetMode() const					{ return (mHeader >> 7) & 3; }
	unsigned	GetModeExt() const				{ return (mHeader >> 5) & 3; }

	unsigned	GetFrameSize() const;
	unsigned	GetPayloadSizeL3() const;
};


struct MPEGAudioHeader {
	enum {
		kMaskNone		= 0x00000000,
		kMaskSync		= 0xFFE00000,
		kMaskMPEG25		= 0x00100000,
		kMaskVersion	= 0x00080000,
		kMaskLayer		= 0x00060000,
		kMaskCRC		= 0x00010000,
		kMaskBitrate	= 0x0000F000,
		kMaskSampleRate	= 0x00000C00,
		kMaskPadding	= 0x00000200,
		kMaskPrivate	= 0x00000100,
		kMaskMode		= 0x000000C0,
		kMaskModeExt	= 0x00000030,
		kMaskCopyright	= 0x00000008,
		kMaskOriginal	= 0x00000004,
		kMaskEmphasis	= 0x00000003,
		kMaskAll		= 0xFFFFFFFF
	};

	const uint32 mHeader;

	MPEGAudioHeader(uint32 hdr) : mHeader(hdr) {}

// MPEG audio frame header (ISO/IEC 11172-3):

// 11111111 111..... ........ ........  syncword (all bits set)
// ........ ...xx... ........ ........  ID (11=1, 10=2, 00=2.5)
// ........ .....xx. ........ ........  layer (01=III, 10=II, 11=I)
// ........ .......x ........ ........  protection_bit (0=yes, 1=no)
// ........ ........ xxxx.... ........  bitrate_index (1111=forbidden)
// ........ ........ ....xx.. ........  sampling_frequency (00=44.1, 01=48, 10=32)
// ........ ........ ......x. ........  padding_bit (0=no, 1=yes)
// ........ ........ .......x ........  private_bit (unused)
// ........ ........ ........ xx......  mode (00=stereo,01=joint,10=dual,11=mono)
// ........ ........ ........ ..xx....  mode_extension
// ........ ........ ........ ....x...  copyright (0=no, 1=yes)
// ........ ........ ........ .....x..  original/copy (0=original, 1=copy)
// ........ ........ ........ ......xx  emphasis

	bool		IsSyncValid() const				{ return (mHeader & kMaskSync) == kMaskSync; }
	bool		IsMPEG2() const					{ return !(mHeader & kMaskVersion); }
	bool		IsMPEG25() const				{ return IsMPEG2() && !(mHeader & kMaskMPEG25); }
	unsigned	GetLayer() const				{ return 4 - ((mHeader >> 17) & 3); }
	bool		IsCRCProtected() const			{ return !(mHeader & kMaskCRC); }
	unsigned	GetBitrateIndex() const			{ return (mHeader >> 12) & 15; }
	unsigned	GetSamplingRateIndex() const	{ return (mHeader >> 10) & 3; }
	bool		IsPadded() const				{ return 0 != (mHeader & kMaskPadding); }
	bool		IsStereo() const				{ return (mHeader & kMaskMode) != kMaskMode; }

	unsigned	GetBitrateKbps() const;			//{ return bitrate[IsMPEG2()][GetLayer()-1][GetBitrateIndex()]; }
	unsigned	GetSamplingRateHz() const;		//{ return samp_freq[IsMPEG25()][IsMPEG2()][GetSamplingRateIndex()]; }

// Added these:
	unsigned	GetMode() const					{ return (mHeader >> 6) & 3; }
	unsigned	GetModeExt() const				{ return (mHeader >> 4) & 3; }
	unsigned	GetEmphasis() const				{ return mHeader & kMaskEmphasis; }

	bool IsValid() const;
	bool IsConsistent(uint32 hdr) const;
	unsigned GetFrameSize() const;
	unsigned GetPayloadSizeL3() const;
};

unsigned short mpeg_compact_header(const unsigned long hdr);
unsigned short ac3_compact_header(const unsigned long hdr);
int ac3_compact_info(const unsigned short hdr, int *srate, int *brate, int *chans);
int ac3_sync_info(const unsigned long hdr, long *srate, int *brate, int *chans);

IVDXAudioSource *CreateAudioSourceMPEG2(InputFileMPEG2 *const pp, const unsigned int stream_id);
IVDXAudioSource *CreateAudioSourceMPEG(InputFileMPEG2 *const pp, const unsigned int stream_id);
IVDXAudioSource *CreateAudioSourceLPCM(InputFileMPEG2 *const pp, const unsigned int stream_id);
IVDXAudioSource *CreateAudioSourceAC3(InputFileMPEG2 *const pp, const unsigned int stream_id);

#endif	// f_AUDIO_H