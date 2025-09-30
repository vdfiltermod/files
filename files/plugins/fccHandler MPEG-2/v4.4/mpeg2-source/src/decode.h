#ifndef _VIDEODECODERMPEG2_H_
#define _VIDEODECODERMPEG2_H_

class VideoDecoderMPEG2 : public vdxunknown<IVDXVideoDecoder> {

private:
	InputFileMPEG2* const parentPtr;
	IMPEG2Decoder*  mpDecoder;

	sint64          mPrevFrame;
	unsigned char*  pMemBlock;

protected:
	VDXPixmap       mPixmap;
	unsigned char*  mFrameBuffer;

public:
	VideoDecoderMPEG2(InputFileMPEG2 *pp);
	~VideoDecoderMPEG2();

	// IVDXVideoDecoder
	const void*         VDXAPIENTRY DecodeFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, sint64 sampleNumber, sint64 targetFrame);
	uint32              VDXAPIENTRY GetDecodePadding();
	void                VDXAPIENTRY Reset();
	bool                VDXAPIENTRY IsFrameBufferValid();
	const VDXPixmap&    VDXAPIENTRY GetFrameBuffer();
	bool                VDXAPIENTRY SetTargetFormat(int format, bool useDIBAlignment);
	bool                VDXAPIENTRY SetDecompressedFormat(const VDXBITMAPINFOHEADER *pbih);
	bool                VDXAPIENTRY IsDecodable(sint64 sample_num);
	const void*         VDXAPIENTRY GetFrameBufferBase();

	// VideoDecoderMPEG2
	bool    Init();
};


class VideoDecoderModelMPEG2 : public vdxunknown<IVDXVideoDecoderModel> {

private:
	InputFileMPEG2 *const parentPtr;

	int frame_forw, frame_back, frame_bidir, frame_aux;

	sint64 mSampleFirst;
	sint64 mSampleLast;

	sint64 stream_current_frame;
	sint64 stream_desired_frame;

	sint64 prev_field_IP(sint64 f64) const;
	sint64 next_field_IP(sint64 f64) const;

public:
	VideoDecoderModelMPEG2(InputFileMPEG2 *pp);
	~VideoDecoderModelMPEG2();

	// IVDXVideoDecoderModel
	void    VDXAPIENTRY Reset();
	void    VDXAPIENTRY SetDesiredFrame(sint64 frame_num);
	sint64  VDXAPIENTRY GetNextRequiredSample(bool& is_preroll);
	int     VDXAPIENTRY GetRequiredCount();
};


#endif	// _VIDEODECODERMPEG2_H_
