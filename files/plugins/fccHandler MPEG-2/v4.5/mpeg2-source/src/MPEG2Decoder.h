#ifndef f_MPEG2DECODER_H
#define f_MPEG2DECODER_H

class IMPEG2Decoder {

public:
	enum {
		kError						= 1,	// it's just... error.
		kErrorNotInitialized		= 2,
		kErrorBadValue				= 4,
		kErrorBadFrameType			= 8,
		kErrorBadMotionVector		= 16,
		kErrorTooManyCoefficients	= 32,
		kErrorNoMoreMacroblocks     = 64,
		kErrorSkippedMBInIFrame     = 128
	};
	
	enum kAcceleration {
		eAccel_None	= 0,
		eAccel_MMX	= 1,
		eAccel_ISSE	= 2
	};
	
	virtual bool Init(int width, int height, int seq_ext) = 0;
	virtual void Destruct() = 0;

	// decoding
	
	virtual int DecodeFrame(const void *src, int len, int frame, int dst, int fwd, int rev) = 0;
	virtual int GetErrorState() const = 0;
	virtual int GetFrameBuffer(int frame) const = 0;
	virtual int GetFrameNumber(int buffer) const = 0;
	virtual void CopyFrameBuffer(int dst, int src, int newframe) = 0;
	virtual void SwapFrameBuffers(int dst, int src) = 0;
	virtual void ClearFrameBuffers() = 0;

	virtual void CopyField(int dst, int dstfield, int src, int srcfield) = 0;
	virtual void EnableMatrixCoefficients(bool bEnable) = 0;
	virtual void EnableAcceleration(int level) = 0;

	// information

	virtual bool IsMPEG2() const = 0;
	virtual int  GetChromaFormat() const = 0;

	// framebuffer access

	virtual const void *GetYBuffer(int buffer, int& pitch) = 0;
	virtual const void *GetCrBuffer(int buffer, int& pitch) = 0;
	virtual const void *GetCbBuffer(int buffer, int& pitch) = 0;

	// framebuffer conversion
/*
	virtual bool DecodeUYVY(void *dst, int pitch, int buffer) = 0;
	virtual bool DecodeYUYV(void *dst, int pitch, int buffer) = 0;
	virtual bool DecodeYVYU(void *dst, int pitch, int buffer) = 0;
	virtual bool DecodeY41P(void *dst, int pitch, int buffer) = 0;
	virtual bool DecodeRGB15(void *dst, int pitch, int buffer) = 0;
	virtual bool DecodeRGB16(void *dst, int pitch, int buffer) = 0;
	virtual bool DecodeRGB24(void *dst, int pitch, int buffer) = 0;
	virtual bool DecodeRGB32(void *dst, int pitch, int buffer) = 0;

	virtual bool Decode420Planar(void *dstY, int pitchY, void *dstU,
		int pitchU, void *dstV, int pitchV, int buffer) = 0;

	virtual bool Decode422Planar(void *dstY, int pitchY, void *dstU,
		int pitchU, void *dstV, int pitchV, int buffer) = 0;
*/
	virtual bool ConvertFrame(const VDXPixmap& Pixmap, int buffer) = 0;
};

IMPEG2Decoder *CreateMPEG2Decoder();

#endif	// f_MPEG2DECODER_H
