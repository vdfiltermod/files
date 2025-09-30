#ifndef _INPUTFILEMPEG2_H_
#define _INPUTFILEMPEG2_H_

#define MAX_AUDIO_STREAMS 8

// Do we expose multiple audio streams to the host?
#define MULTIAUDIO


// Audio types we support
#define AUDIOTYPE_AC3	0x80
#define AUDIOTYPE_LPCM	0xA0
#define AUDIOTYPE_MPEG	0xC0
#define AUDIOTYPE_MASK	0xE0

// Frame types
#define MPEG_FRAME_TYPE_I	1
#define MPEG_FRAME_TYPE_P	2
#define MPEG_FRAME_TYPE_B	3
#define MPEG_FRAME_TYPE_D	4

// Flags for MPEGSampleInfo.bitflags
#define PICTUREFLAG_RFF		0x04	// repeat_first_field
#define PICTUREFLAG_TFF		0x08	// top_field_first
#define PICTUREFLAG_PF		0x10	// progressive_frame

// Picture structures
#define TOP_FIELD		1
#define BOTTOM_FIELD	2
#define FRAME_PICTURE	3


//////////////////////////////////////////////////////////////////////////
//
//
//					MPEGPacketInfo/MPEGSampleInfo
//
//
//////////////////////////////////////////////////////////////////////////

// For MPEGPacketInfo, the top 48 bits is the unsigned file offset
// of the packet, and the bottom 16 bits is the unsigned size of
// the packet.  Thus the MPEG cannot be larger than 256 TiB - 1.
// In actual practice, we'll probably exceed the maximum number
// of packets (2 GiB - 1) well before we hit the file size limit.

typedef struct {
	unsigned __int64	file_pos;
} MPEGPacketInfo;


// MPEGSampleInfo (12 bytes)

#include <pshpack1.h>

typedef struct {
	unsigned int    size;               // size of the sample in bytes
	unsigned int    pktindex;           // packet where the sample begins
	unsigned short  pktoffset;          // offset of sample in the packet
	union {
		struct {
			unsigned char   frame_type; // I,P,B,D
			unsigned char   bitflags;   // rff/tff/pf/struc
		};
		struct {
			unsigned short  header;     // compacted audio header
		};
	};
} MPEGSampleInfo;

#include <poppack.h>


class MPEGFileParser;

class InputFileMPEG2 : public vdxunknown<IVDXInputFile> {

friend MPEGFileParser;

private:
	bool            fUserAborted;
	uint32          uiFlags;

	// Video
	MPEGPacketInfo* video_packet_list;
	unsigned int    vpackets;
	__int64         vfirstPTS;
	bool            progressive_sequence;
	unsigned int    lFirstCode;

	// Audio
	unsigned char   audio_stream_list[96];  // 0x80 to 0xDF
	MPEGPacketInfo* audio_packet_list[48];
	unsigned int    apackets[48];
	__int64         afirstPTS[48];

#ifndef MULTIAUDIO
	static INT_PTR CALLBACK ChooseAudioDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	int iChosenAudio;
#endif

	inFile  mFile;
	int     mNumFiles;

	void    Cleanup();
	bool    PostInit();

	// File Information dialog
	static  void     UpdateDlgAudio(HWND hDlg, unsigned int id);
	static  void     UpdateDlgVideo(HWND hDlg, unsigned int id);
	static  INT_PTR  CALLBACK InfoDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	// Indexing
	LPWSTR  GetIndexName(LPCWSTR szFile);
	int     GetPathLen(LPCWSTR szFile);
	bool    ReadIndex(LPCWSTR szFile, AppendNames *an);
	void    WriteIndex(LPCWSTR szFile, AppendNames *an);
	void    WriteSampleInfo(inFile& idxFile, const MPEGSampleInfo *msi, int samples);
	void    WritePacketInfo(inFile& idxFile, const MPEGPacketInfo *mpi, unsigned long packets);

	// Version 1.9 index
	void    ReadSampleInfo19(inFile& idxFile, MPEGSampleInfo *msi, int samples);
	void    ReadPacketInfo19(inFile& idxFile, MPEGPacketInfo *mpi, unsigned long packets);
	// Version 2.0 index
	bool    ReadIndex20(inFile& idxFile, AppendNames *an);
	// Version 2.1 index
	bool    ReadIndex21(inFile& idxFile, AppendNames *an);

public:
	InputFileMPEG2(const VDXInputDriverContext& context, uint32 flags);
	~InputFileMPEG2();

	// IVDXInputFile
	bool    VDXAPIENTRY PromptForOptions(VDXHWND, IVDXInputOptions **ppOptions);
	bool    VDXAPIENTRY CreateOptions(const void *buf, uint32 len, IVDXInputOptions **ppOptions);
	void    VDXAPIENTRY Init(const wchar_t *path, IVDXInputOptions *options);
	bool    VDXAPIENTRY Append(const wchar_t *path);
	void    VDXAPIENTRY DisplayInfo(VDXHWND hwndParent);
	bool    VDXAPIENTRY GetVideoSource(int index, IVDXVideoSource **ppVS);
	bool    VDXAPIENTRY GetAudioSource(int index, IVDXAudioSource **ppAS);

	const   VDXInputDriverContext& mContext;

	// Audio samples and audio stream info
	MPEGSampleInfo* audio_sample_list[48];
	unsigned int    aframes[48];
	bool            fAllowDSC;

	// Video samples and video stream info
	MPEGSampleInfo* video_sample_list;
	unsigned int    vframes;
	unsigned int    largest_framesize;
	unsigned int    width;
	unsigned int    height;
	unsigned int    display_width;
	unsigned int    display_height;
	VDXFraction     mFrameRate;
	int             aspect_ratio;   // aspect_ratio_information
	unsigned int    seq_ext;
	unsigned int*   video_field_map;
	unsigned int    fields;
	bool            fAllowMatrix;
	int             matrix_coefficients;

	int             system;			// 0=none, 1=MPEG-1, 2=MPEG-2

	__int64 GetSamplePosition(unsigned int sample, int stream_id);
	void    ReadSample(void *buffer, unsigned int sample, int len, int stream_id);
};

#endif	// _INPUTFILEMPEG2_H_
