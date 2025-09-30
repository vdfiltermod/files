#ifndef _PARSER_H_
#define _PARSER_H_

class MPEGFileParser {

private:
	InputFileMPEG2 *const parentPtr;

	// Fix for M2M, search at least 3 MB
	enum {
		SEARCH_AUDIO_LIMIT          = 256,
		NON_INTERLEAVED_PACK_SIZE   = 16384,
		SEARCH_SYSTEM_LIMIT         = 1048576 * 3
	};

	DataVector video_stream_blocks;

	unsigned char packet_buffer[65536];

	inFile& mFile;
	int     vidstream;
	bool    fAbort;
	HWND    hWndStatus;
	HWND    hWndParent;

	int     NextStartCode();
	bool    Skip(int bytes);

	int     Read();
	int     Read(void *, int);

	bool    Verify();

	__int64 PacketPTS(int pkt, int stream_id);
	void    DoFixUps();

	bool    ParsePack(int stream_id);

	static  BOOL CALLBACK FindContextWindow(HWND hwnd, LPARAM lParam);
	bool    guiDlgMessageLoop(HWND hDlg);
	static  INT_PTR CALLBACK ParseDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	class   MPEGVideoParser *videoParser;
	class   MPEGAudioParser *audioParser[48];

	bool    fIsMPEG2;
	bool    fInterleaved;
	bool    fIsScrambled;
	long    lFirstCode;

	int     audio_streams;

public:
	MPEGFileParser(InputFileMPEG2 *const pp);
	~MPEGFileParser();

	void Parse(HMODULE hModule);
};

#endif	// _PARSER_H_
