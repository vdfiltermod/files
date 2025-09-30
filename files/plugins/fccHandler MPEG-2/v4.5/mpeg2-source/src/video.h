#ifndef _VIDEOSOURCEMPEG2_H_
#define _VIDEOSOURCEMPEG2_H_

class VideoSourceMPEG2 : public vdxunknown<IVDXStreamSource>, public IVDXVideoSource {

private:
	InputFileMPEG2* const parentPtr;

	unsigned char*  read_buffer1;
	long            buffer1_frame;
	unsigned char*  read_buffer2;
	long            buffer2_frame;

	sint64          mSampleFirst;
	sint64          mSampleLast;

#ifdef SUPPORT_DSC
	unsigned char*  DirectFormat;
	int             DirectFormatLen;
	bool            DirectTested;
#endif

public:
	VideoSourceMPEG2(InputFileMPEG2* const pp);
	~VideoSourceMPEG2();

	// IVDXUnknown
	int         VDXAPIENTRY AddRef();
	int         VDXAPIENTRY Release();
	void*       VDXAPIENTRY AsInterface(uint32 iid);

	// IVDXStreamSource
	void        VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo&);
	bool        VDXAPIENTRY Read(sint64 lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	const void* VDXAPIENTRY GetDirectFormat();
	int         VDXAPIENTRY GetDirectFormatLen();
	ErrorMode   VDXAPIENTRY GetDecodeErrorMode();
	void        VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode);
	bool        VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode);
	bool        VDXAPIENTRY IsVBR();
	sint64      VDXAPIENTRY TimeToPositionVBR(sint64 us);
	sint64      VDXAPIENTRY PositionToTimeVBR(sint64 samples);

	// IVDXVideoSource
	void        VDXAPIENTRY GetVideoSourceInfo(VDXVideoSourceInfo& info);
	bool        VDXAPIENTRY CreateVideoDecoderModel(IVDXVideoDecoderModel **ppModel);
	bool        VDXAPIENTRY CreateVideoDecoder(IVDXVideoDecoder **ppDecoder);
	void        VDXAPIENTRY GetSampleInfo(sint64 sample_num, VDXVideoFrameInfo& frameInfo);
	bool        VDXAPIENTRY IsKey(sint64 sample_num);
	sint64      VDXAPIENTRY GetFrameNumberForSample(sint64 sample_num);
	sint64      VDXAPIENTRY GetSampleNumberForFrame(sint64 frame_num);
	sint64      VDXAPIENTRY GetRealFrame(sint64 frame_num);
	sint64      VDXAPIENTRY GetSampleBytePosition(sint64 sample_num);
};

#endif	// _VIDEOSOURCEMPEG2_H_
