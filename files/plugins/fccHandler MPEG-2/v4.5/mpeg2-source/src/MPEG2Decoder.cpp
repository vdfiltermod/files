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

// MPEG-2 Decoder

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <math.h>

#include "vd2\plugin\vdplugin.h"


#if defined(_AMD64_) || defined(_M_AMD64)
	#include <stdlib.h>
	extern "C" void amd64_emms();
	#define my_emms amd64_emms();
#else
	#define my_emms __asm emms
#endif

#pragma intrinsic(memcpy)

#ifdef _DEBUG
	#include <crtdbg.h>
#endif

#include "MPEG2Decoder.h"
#include "mpeg2_tables.h"
#include "mpeg2_asm.h"


// Start codes
#define SEQUENCE_START_CODE		0x1B3
#define PICTURE_START_CODE		0x100
#define SLICE_START_CODE_MIN	0x101
#define SLICE_START_CODE_MAX	0x1AF
#define EXTENSION_START_CODE	0x1B5
#define USER_DATA_START_CODE	0x1B2

// Extension start code IDs
#define SEQUENCE_EXTENSION_ID			1
#define SEQUENCE_DISPLAY_EXTENSION_ID	2
#define QUANT_MATRIX_EXTENSION_ID		3
#define COPYRIGHT_EXTENSION_ID			4
#define PICTURE_DISPLAY_EXTENSION_ID	7
#define PICTURE_CODING_EXTENSION_ID		8

#define MPEG_FRAME_TYPE_I			(1)
#define MPEG_FRAME_TYPE_P			(2)
#define MPEG_FRAME_TYPE_B			(3)
#define MPEG_FRAME_TYPE_D			(4)

// Chroma formats
#define CHROMA420		1
#define CHROMA422		2
#define CHROMA444		3
// Picture structures
#define TOP_FIELD		1
#define BOTTOM_FIELD	2
#define FRAME_PICTURE	3
// Motion_types
#define MC_FIELD	1
#define MC_FRAME	2
#define MC_16X8		2
#define MC_DMV		3
// Motion vector types
#define MV_FIELD	0
#define MV_FRAME	1
// flags for MPEGBuffer.bitflags
#define PICTUREFLAG_RFF	1	// repeat_first_field
#define PICTUREFLAG_TFF	2	// top_field_first
#define PICTUREFLAG_PF	4	// progressive_frame

// Number of buffers we need
#define MPEG_BUFFERS	(4UL)

#define CPUFLAG_MMX		1
#define CPUFLAG_ISSE	2

typedef unsigned char YCCSample;

typedef struct {
	YCCSample *Y;
	YCCSample *U;
	YCCSample *V;
	int frame_num;
	int bitflags;
} MPEGBuffer;


class MPEG2Decoder : public IMPEG2Decoder {

private:
	unsigned char *memblock;
	const unsigned char *bitarray;
	unsigned int totbits;
	unsigned int bitpos;

	unsigned char intramatrix[64];
	unsigned char nonintramatrix[64];
	unsigned char chromaintramatrix[64];
	unsigned char chromanonintramatrix[64];

	int forw_vector_full_pel;
	int back_vector_full_pel;

	int MPEG2_Flag;
	int quantizer_scale;
	int matrix_coefficients;
	int chroma_format;
	int block_count;
	int progressive_sequence;
	int f_code[2][2];
	int intra_dc_precision;
	int picture_structure;
	int frame_pred_frame_dct;
	int concealment_vectors;
	int q_scale_type;
	int intra_vlc_format;
	int alternate_scan;
	int PMV[2][2][2];
	int dmvector[2];
	int mvfield_select[2][2];
//	int motion_vector_count;
	int motion_type;
	int mv_format;
	int mvscale;
	int dct_type;

	bool fAllowMatrixCoefficients;

	MPEGBuffer buffers[MPEG_BUFFERS];

	MPEGBuffer *current_frame;
	MPEGBuffer *forw_frame;
	MPEGBuffer *back_frame;
	MPEGBuffer *aux_frame;

	YCCSample *uv422, *u444, *v444;

	short (*dct_coeff)[64];

	int second_field;
	int mErrorState;
	int cpuflags;

	int frame_type;

	int horizontal_size;
	int vertical_size;

	int mb_width;
	int mb_height;

	int coded_picture_width;
	int coded_picture_height;

	int dct_dc_y_past, dct_dc_u_past, dct_dc_v_past;
	int macro_block_flags;

	bool mpeg_alloc();
	int show_bits(int nBits);
	int get_bits(int nBits);
	int next_start_code();
	inline void picture_coding_extension();
	inline void quant_matrix_extension();
	void extension_and_user_data();
	inline bool sequence_header();
	inline void picture_header();
	inline void idctcol(short *blk);
	inline void idctrow(short *blk);
	inline void Fast_IDCT(short *src);
	void Add_Block(int comp, int bx, int by, int addflag);
	int get_luma_diff();
	int get_chroma_diff();

	inline void Decode_MPEG1_Intra_Block(int comp);
	inline void Decode_MPEG1_Non_Intra_Block(int comp);
	inline void Decode_MPEG2_Intra_Block(int comp);
	inline void Decode_MPEG2_Non_Intra_Block(int comp);

	void form_component_prediction(const YCCSample* src, YCCSample* dst,
		int w, int h, int lx, int x, int y, int dx, int dy, int average_flag);

	void form_prediction(MPEGBuffer *srcBuf, int sfield, MPEGBuffer *dstBuf, int dfield,
		int lx, int w, int h, int x, int y, int dx, int dy, int average_flag);

	void dual_prime(int DMV[][2], int mvx, int mvy);
	void form_predictions(int bx, int by);
	int mpeg_get_motion_component();
	bool get_mb_address_inc(int& val);
	inline int get_I_mb_flags();
	inline int get_P_mb_flags();
	inline int get_B_mb_flags();
	inline int get_D_mb_flags();
	inline int get_mb_flags();
	inline int get_mb_cbp();
	void decode_motion_vector(int& pred, int r_size, int code, int residual, int full_pel);
	void motion_vector(int *pmv, int h_size, int v_size, int full_pel);
	void motion_vectors(int count, int s, int h_size, int v_size);
	inline void decode_mb(int MBA);
	inline void skipped_mb(int MBA);
	inline void slice(int pos);

	void conv420to422(const YCCSample *src, unsigned char *dst, bool progressive);
	void conv422to444(const YCCSample *src, unsigned char *dst, bool mpeg2);
	void conv444toRGB(const YCCSample *srcY, const YCCSample *srcU, const YCCSample *srcV, void *dst, int dst_width, int dst_height, int dst_pitch, int bpp);
	
	// VirtualDub interface
	void Blit (const void *src, int src_width, int src_height, int src_pitch, void *dst, int dst_width, int dst_height, int dst_pitch);
	void HalfY(const void *src, int src_width, int src_height, int src_pitch, void *dst, int dst_width, int dst_height, int dst_pitch, bool progressive);

	// VirtualDub converters
        bool ConvertToRGB (const VDXPixmap& pm, int buffer, int bpp);
inline  bool ConvertToY8  (const VDXPixmap& pm, int buffer);
        bool ConvertTo420 (const VDXPixmap& pm, int buffer, bool interlace);
inline  bool ConvertToYUY2(const VDXPixmap& pm, int buffer);
inline  bool ConvertToUYVY(const VDXPixmap& pm, int buffer);
inline  bool ConvertTo422P(const VDXPixmap& pm, int buffer);
inline  bool ConvertTo444P(const VDXPixmap& pm, int buffer);

public:
	MPEG2Decoder();
//	~MPEG2Decoder();
	void Destruct();

	// decoding
	
	bool Init(int width, int height, int seq_ext);
	void EnableMatrixCoefficients(bool bEnable);
	void EnableAcceleration(int level);

	bool IsMPEG2()         const { return (MPEG2_Flag != 0); }
	int  GetChromaFormat() const { return chroma_format; }
	int  GetErrorState()   const { return mErrorState; }
	
	int  DecodeFrame(const void *src, int len, int frame, int dst, int fwd, int rev);
	
	// framebuffer access
	
	int  GetFrameBuffer(int frame) const;
	int  GetFrameNumber(int buffer) const;
	void CopyFrameBuffer(int dst, int src, int frameno);
	void SwapFrameBuffers(int dst, int src);
	void ClearFrameBuffers();

	const void *GetYBuffer(int buffer, int& pitch);
	const void *GetCrBuffer(int buffer, int& pitch);
	const void *GetCbBuffer(int buffer, int& pitch);

	void CopyField(int dst, int dstfield, int src, int srcfield);
	bool ConvertFrame(const VDXPixmap& Pixmap, int buffer);
};

#define SetError(x) mErrorState |= x


MPEG2Decoder::MPEG2Decoder()
{
#ifdef _DEBUG
	_RPT0(_CRT_WARN, "MPEG2Decoder constructed\n");
#endif
	memblock = NULL;
	fAllowMatrixCoefficients = true;
	cpuflags = 0;
}


IMPEG2Decoder *CreateMPEG2Decoder()
{
	return new MPEG2Decoder;
}


inline bool MPEG2Decoder::mpeg_alloc()
{
	unsigned int i;
	unsigned int chroma_size;

	// Init() has already confirmed that horizontal_size and
	// vertical_size are > 0 and that chroma_format is valid.

	// Macroblock width and full macroblock height:
	const unsigned int mbw = (horizontal_size + 15) >> 4;
	const unsigned int mbh = 2 * ((vertical_size + 31) >> 5);

	// Calculate: i = the total amount of memory we will need.
	// Allocate it as one huge block, then assign pointers.

	// size of Y buffer (4 of these):
	i = mbw * 16 * mbh * 16 * MPEG_BUFFERS;

	// size of Cb/Cr buffer:
	switch (chroma_format) {
	case CHROMA420:
		chroma_size = mbw * 8  * mbh * 8;
		break;
	case CHROMA422:
		chroma_size = mbw * 8  * mbh * 16;
		break;
	case CHROMA444:
		chroma_size = mbw * 16 * mbh * 16;
		break;
	}
	i += chroma_size * MPEG_BUFFERS;	// 4 Cb buffers
	i += chroma_size * MPEG_BUFFERS;	// 4 Cr buffers

	// Intermediate 4:2:2 buffer (1 of these)
	i += mbw * 8 * mbh * 16;
	// Intermediate 4:4:4 buffers (2 of these)
	i += mbw * 16 * mbh * 16 * 2;

	// dct coefficient block
	i += 12 * 64 * sizeof(short);

	i += 16;		// For 16-byte alignment

// Just for laughs, here are the buffer requirements
// for SD 720 x 480 MPEG at different chroma formats:
//		CHROMA420:  1386352 bytes
//		CHROMA422:  1904752 bytes
//		CHROMA444:  2941552 bytes
// And for HDTV 1920 x 1080:
//		CHROMA420:  8362816 bytes
//		CHROMA422: 11496256 bytes
//		CHROMA444: 17763136 bytes

//	memblock = (unsigned char *)
//		VirtualAlloc(NULL, i, MEM_COMMIT, PAGE_READWRITE);
	memblock = new unsigned char[i];

	if (memblock != NULL)
	{
		// Assign memory pointers
		
		i = (unsigned int)((DWORD_PTR)memblock & 15);
		buffers[0].Y = memblock;
		if (i != 0) buffers[0].Y += (16 - i);

		for (i = 1; i < MPEG_BUFFERS; ++i)
		{
			buffers[i].Y = buffers[i - 1].Y + mbw * 16 * mbh * 16;
		}

		buffers[0].U = buffers[i - 1].Y + mbw * 16 * mbh * 16;
		for (i = 1; i < MPEG_BUFFERS; ++i)
		{
			buffers[i].U = buffers[i - 1].U + chroma_size;
		}

		buffers[0].V = buffers[i - 1].U + chroma_size;
		for (i = 1; i < MPEG_BUFFERS; ++i)
		{
			buffers[i].V = buffers[i - 1].V + chroma_size;
		}

		uv422 = buffers[i - 1].V + chroma_size;

		u444 = uv422 + mbw * 8  * mbh * 16;
		v444 = u444  + mbw * 16 * mbh * 16;

		dct_coeff = (short (*)[64])(v444 + mbw * 16 * mbh * 16);

		ClearFrameBuffers();
		return true;
	}

	return false;
}


bool MPEG2Decoder::Init(int width, int height, int seq_ext)
{
// Initialize MPEG decoder fixed properties
// ----------------------------------------
// The following are considered fixed:
//		horizontal_size, vertical_size
//		coded_picture_width
//		coded_picture_height
//		progressive_sequence
//		chroma_format
//		block_count
//		MPEG2_Flag
//		memblock
// These will never change during decoding.
//
// The final call is to mpeg_alloc(), and if successful
// then we only need to check (memblock != NULL) to know
// that all of the above are correctly initialized.

	horizontal_size = width;
	vertical_size   = height;

	if (horizontal_size <= 0) goto Abort;
	if (vertical_size   <= 0) goto Abort;

	coded_picture_width  = (horizontal_size + 15) & -16;
	coded_picture_height = (vertical_size   + 15) & -16;

	current_frame = forw_frame = back_frame = (&(buffers[0]));

	if (seq_ext)
	{
		// Initializing MPEG-2
		// "seq_ext" is a bswapped portion of
		// the beginning of sequence_extension:
		// ........ ........ ........ 0001....	sequence_extension ID
		// ........ ........ ........ ....x...	escape bit
		// ........ ........ ........ .....xxx	profile
		// ........ ........ xxxx.... ........	level
		// ........ ........ ....x... ........	progressive_sequence
		// ........ ........ .....xx. ........	chroma_format
		// ........ x....... .......x ........	horizontal_size_ext
		// ........ .xx..... ........ ........	vertical_size_ext

		// test.mpg has 0x01005418

		chroma_format = (seq_ext >> 9) & 3;
		progressive_sequence = (seq_ext >> 11) & 1;

		MPEG2_Flag = 1;
	}
	else
	{
		// Initializing MPEG-1
		chroma_format = CHROMA420;
		progressive_sequence = 1;
		MPEG2_Flag = 0;
	}

	switch (chroma_format) {
		case CHROMA420: block_count = 6;    break;
		case CHROMA422: block_count = 8;    break;
		case CHROMA444: block_count = 12;   break;
		default: goto Abort;    // bad chroma_format
	}

	return mpeg_alloc();

Abort:
	return false;
}


void MPEG2Decoder::Destruct() {
#ifdef _DEBUG
	_RPT0(_CRT_WARN, "MPEG2Decoder destructed\n");
#endif
//	if (memblock != NULL) VirtualFree(memblock, 0, MEM_RELEASE);
	delete[] memblock;
	delete this;
}


///////////////////////////////////////////////////////////////////////////

// Critical bits-getting functions
// Correct results are guaranteed only if nBits >= 0 and nBits <= 24

#if defined(_AMD64_) || defined(_M_AMD64)

#pragma intrinsic(_byteswap_ulong)

int MPEG2Decoder::show_bits(int nBits)
{
	const unsigned char *src = bitarray + (bitpos >> 3);
	register unsigned int ret = 0;

	// Note: ((remains + 7) >> 3) is how many BYTES remain!

	if (nBits > 0 && nBits <= 24)
	{
		int remains = totbits - bitpos;
		if (remains > 24)
		{
			// At least four bytes remain in the buffer
			ret = _byteswap_ulong(*((const unsigned long *)src));
		}
		else if (remains > 0)
		{
			ret = src[0] << 24;
			if (remains > 8)
			{
				ret += src[1] << 16;
				if (remains > 16)
				{
					ret += src[2] << 8;
				}
			}
		}
		ret <<= (bitpos & 7);
		ret >>= (32 - nBits);
	}
	return (int)ret;
}


int MPEG2Decoder::get_bits(int nBits)
{
	const unsigned char *src = bitarray + (bitpos >> 3);
	register unsigned int ret = 0;

	if (nBits > 0 && nBits <= 24)
	{
		int remains = totbits - bitpos;
		if (remains > 24)
		{
			// At least four bytes remain in the buffer
			ret = _byteswap_ulong(*((const unsigned long *)src));
		}
		else if (remains > 0)
		{
			ret = src[0] << 24;
			if (remains > 8)
			{
				ret += src[1] << 16;
				if (remains > 16)
				{
					ret += src[2] << 8;
				}
			}
		}
		ret <<= (bitpos & 7);
		ret >>= (32 - nBits);
		bitpos += nBits;
	}
	return (int)ret;
}

#else	// _AMD64_ || _M_AMD64

// Well, I don't like the way VC6 assembles the above.
// The following hand-tweaked is about 3 times faster:

#pragma warning(push)
#pragma warning(disable:4035)	// no return value

int __declspec(naked) MPEG2Decoder::show_bits(int nBits)
{
	// Correct results are guaranteed only if nBits >= 0 and nBits <= 24
	// Optimize for best-case

	__asm {
		push	esi
		xor		eax, eax				; eax = return value
		push	ecx						; we'll need cl
		mov		esi, [ecx].bitpos
		mov		edx, [ecx].totbits
		cmp		dword ptr [esp+12], 0	; nBits <= 0?
		jle		done
		cmp		dword ptr [esp+12], 24	; nBits > 24?
		jg		done
		sub		edx, esi				; edx = totbits - bitpos
		jle		done					; remains <= 0 bits?
		shr		esi, 3					; byte position = bitpos / 8
		add		esi, [ecx].bitarray		; esi = bits pointer
		cmp		edx, 24
		jle		lte24					; remains <= 24 bits?
		// At least 32 bits remain in buffer (best-case)
		mov		eax, [esi]				; b3 b2 b1 b0
finale:
		mov		ecx, [ecx].bitpos
		mov		edx, 32
		bswap	eax
		sub		edx, [esp+12]			; edx = 32 - nBits
		and		ecx, 7					; ecx = bitpos & 7
		shl		eax, cl
		mov		cl, dl
		shr		eax, cl
done:
		pop		ecx
		pop		esi
		ret		4

lte24:
		cmp		edx, 16
		jle		lte16					; remains <= 16 bits?
		// Exactly 24 bits remain in buffer
		mov		al, [esi+2]
		shl		eax, 16
		mov		ax, word ptr [esi]		; 00 b2 b1 b0
		jmp		finale
lte16:
		cmp		edx, 8
		jle		lte08					; remains <= 8 bits?
		// Exactly 16 bits remain in buffer
		mov		ax, word ptr [esi]
		jmp		finale					; 00 00 b1 b0
lte08:
		test	edx, edx
		jle		finale
		// Only 8 bits remain in buffer
		mov		al, [esi]				; 00 00 00 b0
		jmp		finale
	}
}

// get_bits(x) is simply show_bits(x) + flush_buffer(x), but
// we'll avoid the overhead of a function call by copying the
// show_bits() code directly into get_bits().

int __declspec(naked) MPEG2Decoder::get_bits(int nBits)
{
	// Correct results are guaranteed only if nBits >= 0 and nBits <= 24
	// Optimize for best-case

	__asm {
		push	esi
		xor		eax, eax				; eax = return value
		push	ecx						; we'll need cl
		mov		esi, [ecx].bitpos
		mov		edx, [ecx].totbits
		cmp		dword ptr [esp+12], 0	; nBits <= 0?
		jle		done
		cmp		dword ptr [esp+12], 24	; nBits > 24?
		jg		done
		sub		edx, esi				; edx = totbits - bitpos
		jle		done					; remains <= 0 bits?
		shr		esi, 3					; byte position = bitpos / 8
		add		esi, [ecx].bitarray		; esi = bits pointer
		cmp		edx, 24
		jle		lte24					; remains <= 24 bits?
		// At least 32 bits remain in buffer (best-case)
		mov		eax, [esi]				; b3 b2 b1 b0
finale:
		mov		esi, [esp+12]			; esi = nBits
		mov		edx, [ecx].bitpos
		add		[ecx].bitpos, esi		; bitpos += nBits
		mov		ecx, edx
		mov		edx, 32
		bswap	eax
		sub		edx, esi				; edx = 32 - nBits
		and		ecx, 7					; ecx = bitpos & 7
		shl		eax, cl
		mov		cl, dl
		shr		eax, cl
done:
		pop		ecx
		pop		esi
		ret		4

lte24:
		cmp		edx, 16
		jle		lte16					; remains <= 16 bits?
		// Exactly 24 bits remain in buffer
		mov		al, [esi+2]
		shl		eax, 16
		mov		ax, word ptr [esi]		; 00 b2 b1 b0
		jmp		finale
lte16:
		cmp		edx, 8
		jle		lte08					; remains <= 8 bits?
		// Exactly 16 bits remain in buffer
		mov		ax, word ptr [esi]
		jmp		finale					; 00 00 b1 b0
lte08:
		test	edx, edx
		jle		finale
		// Only 8 bits remain in buffer
		mov		al, [esi]				; 00 00 00 b0
		jmp		finale
	}
}

#pragma warning(pop)		// 4035: No return value

#endif	// _AMD64_ || _M_AMD64


#define flush_buffer(nBits) bitpos += nBits


int MPEG2Decoder::next_start_code()
{
	int i;

	//align to next byte
	bitpos += (-(int)bitpos) & 7;

	//set position at next start code and return it
	while (bitpos < totbits)
	{
		i = get_bits(8);
		if (i == 0)
		{
			i = show_bits(24);
			if ((i & 0xFFFFFF00) == 0x100)
			{
				bitpos -= 8;
				return i;
			}
		}
	}
	return 0;
}


////////////////////////////////////////////////////////////////////////////////

inline void MPEG2Decoder::picture_coding_extension()
{
	current_frame->bitflags = 0;

	// zero is forbidden
	if (f_code[0][0] = get_bits(4)) --f_code[0][0];
	if (f_code[0][1] = get_bits(4)) --f_code[0][1];
	if (f_code[1][0] = get_bits(4)) --f_code[1][0];
	if (f_code[1][1] = get_bits(4)) --f_code[1][1];

	intra_dc_precision = get_bits(2);
	picture_structure = get_bits(2);
	if (get_bits(1)) current_frame->bitflags |= PICTUREFLAG_TFF;
	frame_pred_frame_dct = get_bits(1);
	concealment_vectors = get_bits(1);
	q_scale_type = get_bits(1);
	intra_vlc_format = get_bits(1);
	alternate_scan = get_bits(1);
	if (get_bits(1)) current_frame->bitflags |= PICTUREFLAG_RFF;
	flush_buffer(1);	// chroma_420_type (obsolete)
	if (get_bits(1)) current_frame->bitflags |= PICTUREFLAG_PF;
}


inline void MPEG2Decoder::quant_matrix_extension()
{
	if (get_bits(1))
	{
		for (int i = 0; i < 64; ++i)
		{
			intramatrix[zigzag[0][i]] = (unsigned char)get_bits(8);
		}
	}

	if (get_bits(1))
	{
		for (int i = 0; i < 64; ++i)
		{
			nonintramatrix[zigzag[0][i]] = (unsigned char)get_bits(8);
		}
	}

	if (get_bits(1))
	{
		for (int i = 0; i < 64; ++i)
		{
			chromaintramatrix[zigzag[0][i]] = (unsigned char)get_bits(8);
		}
	}
	else
	{
		memcpy(chromaintramatrix, intramatrix, 64);
	}

	if (get_bits(1))
	{
		for (int i = 0; i < 64; ++i)
		{
			chromanonintramatrix[zigzag[0][i]] = (unsigned char)get_bits(8);
		}
	}
	else
	{
		memcpy(chromanonintramatrix, nonintramatrix, 64);
	}
}


void MPEG2Decoder::extension_and_user_data()
{
	int code;

	while (code = next_start_code())
	{
		if (code == EXTENSION_START_CODE)
		{
			flush_buffer(32);

			switch (get_bits(4)) {

			case SEQUENCE_EXTENSION_ID:
				flush_buffer(8+7+12+1+8+1+2+5);
				if (fAllowMatrixCoefficients)
				{
					matrix_coefficients = 1;
				}
				break;

			case SEQUENCE_DISPLAY_EXTENSION_ID:
				if (get_bits(4) & 1)
				{
					flush_buffer(16);	// color_primaries, etc.
					if (fAllowMatrixCoefficients)
					{
						matrix_coefficients = get_bits(8) & 7;
					}
					else
					{
						flush_buffer(8);
					}
				}
				break;

			case PICTURE_CODING_EXTENSION_ID:
				picture_coding_extension();
				break;

			case QUANT_MATRIX_EXTENSION_ID:
				quant_matrix_extension();
				break;
			}
		}
		else
		{
			if (code != USER_DATA_START_CODE) break;
			flush_buffer(32);
		}
	}
}


inline bool MPEG2Decoder::sequence_header()
{
	if (get_bits(12) != (horizontal_size & 0xFFF)) return false;
	if (get_bits(12) != (vertical_size   & 0xFFF)) return false;

	flush_buffer(4);    // aspect_ratio_code
	flush_buffer(4);    // frame_rate_code
	flush_buffer(18);   // bit_rate
	flush_buffer(1);    // marker_bit
	flush_buffer(10);   // vbv_buffer_size
	flush_buffer(1);    // constrained_parameters_flag

	// load new matrices if applicable
	if (get_bits(1))
	{
		// load_intra_quantizer_matrix
		for (int i = 0; i < 64; ++i)
		{
			intramatrix[zigzag[0][i]] = (unsigned char)get_bits(8);
		}
	}
	else
	{
		memcpy(intramatrix, intramatrix_default, 64);
	}

	if (get_bits(1))
	{
		// load_non_intra_quantizer_matrix
		for (int i = 0; i < 64; ++i)
		{
			nonintramatrix[zigzag[0][i]] = (unsigned char)get_bits(8);
		}
	}
	else
	{
		memset(nonintramatrix, 16, 64);
	}

	memcpy(chromaintramatrix, intramatrix, 64);
	memcpy(chromanonintramatrix, nonintramatrix, 64);

	matrix_coefficients = 5;
	extension_and_user_data();

	return true;
}


inline void MPEG2Decoder::picture_header()
{
	flush_buffer(10);   // temporal_reference
	frame_type = get_bits(3);

	flush_buffer(16);   // VBV_delay

	if (frame_type == MPEG_FRAME_TYPE_P || frame_type == MPEG_FRAME_TYPE_B)
	{
		forw_vector_full_pel = get_bits(1);
		f_code[0][0] = f_code[0][1] = get_bits(3)-1;

		if (frame_type == MPEG_FRAME_TYPE_B)
		{
			back_vector_full_pel = get_bits(1);
			f_code[1][0] = f_code[1][1] = get_bits(3)-1;
		}
	}

	// The following remain at MPEG-1 defaults, unless
	// overwritten by MPEG-2 picture_coding_extension()...

	picture_structure       = FRAME_PICTURE;
	frame_pred_frame_dct    = 1;
	concealment_vectors     = 0;
	current_frame->bitflags = PICTUREFLAG_PF;

	extension_and_user_data();
}


// frame is the current frame number
// dst is destination MPEGBuffer
// fwd is forward reference MPEGBuffer
// rev is backward reference MPEGBuffer
int MPEG2Decoder::DecodeFrame(const void *src, int len, int frame, int dst, int fwd, int rev)
{
	int code;
	int ret = -1;	// return failure

	mErrorState = 0;

// sanity checks
	if (memblock == NULL)
	{
		SetError(kErrorNotInitialized);
		goto Abort;
	}
	if (src == NULL || len < 4) goto Abort;
	if ((unsigned int)dst >= MPEG_BUFFERS) goto Abort;
	if ((unsigned int)fwd >= MPEG_BUFFERS) goto Abort;
	if ((unsigned int)rev >= MPEG_BUFFERS) goto Abort;

// Bind bits-getting functions to "input_data"
	bitarray = (unsigned char *)src;
	totbits  = len << 3;
	bitpos   = 0;

	second_field = 0;
	frame_type   = 0;

	// Note:  The confusion here stems from the fact that the most
	// recently decoded I/P frame is the _forward_ prediction for
	// a P frame, but it's the _backward_ prediction for a B frame!

	// Rule:  The most recently decoded I/P frame is
	// always found in the _backward_ prediction buffer

	forw_frame    = &(buffers[fwd]);
	back_frame    = &(buffers[rev]);
	current_frame = &(buffers[dst]);

Again:

// Decode headers up to the first slice
	while (code = next_start_code())
	{
		if ( code >= SLICE_START_CODE_MIN
		  && code <= SLICE_START_CODE_MAX) break;

		flush_buffer(32);

		if (code == SEQUENCE_START_CODE)
		{
			if (!sequence_header())
			{
				code = 0;
				break;
			}
		}
		else if (code == PICTURE_START_CODE)
		{
			picture_header();
		}
	}

	if (!code) goto Abort;  // error; no slices

	/****************************************************/
	/*  Macroblock width and height                     */
	/*  cf. ISO/IEC 13818-2 section 6.3.3               */
	/****************************************************/

	mb_width = (horizontal_size + 15) >> 4;

	if (progressive_sequence)
	{
		mb_height = (vertical_size + 15) >> 4;
	}
	else
	{
		mb_height = (vertical_size + 31) >> 5;  // height of one field
		if (picture_structure == FRAME_PICTURE) mb_height <<= 1;
	}

	// Decode all slices
	do {
		flush_buffer(32);
		slice(code);
		code = next_start_code();
	} while (code >= SLICE_START_CODE_MIN
	      && code <= SLICE_START_CODE_MAX);

	if (!second_field)
	{
		current_frame->frame_num = frame;

		if (picture_structure != FRAME_PICTURE)
		{
			second_field = !second_field;
			goto Again;
		}
	}

	ret = dst;

Abort:
	if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE)) my_emms

	return ret;
}


int MPEG2Decoder::GetFrameBuffer(int frame) const
{
	int i;
	for (i = 0; i < MPEG_BUFFERS; ++i)
	{
		if (buffers[i].frame_num == frame) return i;
	}
	return -1;
}


int MPEG2Decoder::GetFrameNumber(int buffer) const
{
	if ((unsigned int)buffer < MPEG_BUFFERS)
	{
		return buffers[buffer].frame_num;
	}
	return -1;
}


void MPEG2Decoder::CopyFrameBuffer(int dst, int src, int frameno)
{
	// Copy one buffer to another
	if (memblock != NULL && (unsigned int)dst < MPEG_BUFFERS && (unsigned int)src < MPEG_BUFFERS)
	{
		memcpy(buffers[dst].Y, buffers[src].Y, mb_width * 16 * mb_height * 16);

		switch (chroma_format) {
		case CHROMA420:
			memcpy(buffers[dst].U, buffers[src].U, mb_width * 8 * mb_height * 8);
			memcpy(buffers[dst].V, buffers[src].V, mb_width * 8 * mb_height * 8);
		case CHROMA422:
			memcpy(buffers[dst].U, buffers[src].U, mb_width * 16 * mb_height * 8);
			memcpy(buffers[dst].V, buffers[src].V, mb_width * 16 * mb_height * 8);
		case CHROMA444:
			memcpy(buffers[dst].U, buffers[src].U, mb_width * 16 * mb_height * 16);
			memcpy(buffers[dst].V, buffers[src].V, mb_width * 16 * mb_height * 16);
		}

		buffers[dst].frame_num = frameno;
		buffers[dst].bitflags  = buffers[src].bitflags;
	}
}


void MPEG2Decoder::SwapFrameBuffers(int dst, int src)
{
	// Swap two buffers
	if (   (unsigned int)dst < MPEG_BUFFERS
	    && (unsigned int)src < MPEG_BUFFERS
	    && dst != src)
	{
		MPEGBuffer b = buffers[dst];
		buffers[dst] = buffers[src];
		buffers[src] = b;
	}
}


void MPEG2Decoder::ClearFrameBuffers()
{
	// Clear all buffers
	int i;
	for (i = 0; i < MPEG_BUFFERS; ++i)
	{
		buffers[i].frame_num = -1;
	}
}


void MPEG2Decoder::CopyField(int dst, int dstfield, int src, int srcfield)
{
	// Copy a single field from src to dst
	if (memblock != NULL
	    && (unsigned int)dst < MPEG_BUFFERS
	    && (unsigned int)src < MPEG_BUFFERS)
	{
		YCCSample *srcf;
		YCCSample *dstf;
		int y;

		int cpw = coded_picture_width;
		int cph = coded_picture_height >> 1;

		if (srcfield) srcfield = cpw;
		if (dstfield) dstfield = cpw;

		// First, the Y
		srcf = buffers[src].Y + srcfield;	// source field
		dstf = buffers[dst].Y + dstfield;	// destination field

		// No need to copy anything if they are the same
		if (dstf == srcf) return;

		for (y = 0; y < cph; ++y)
		{
			memcpy(dstf, srcf, cpw);
			dstf += cpw * 2;
			srcf += cpw * 2;
		}

		// Next, Cb and Cr
		if (chroma_format != CHROMA444)
		{
			srcfield >>= 1;
			dstfield >>= 1;
			cpw >>= 1;
			if (chroma_format == CHROMA420)
			{
				cph >>= 1;
			}
		}

		// Cb component
		srcf = buffers[src].U + srcfield;
		dstf = buffers[dst].U + dstfield;

		for (y = 0; y < cph; ++y)
		{
			memcpy(dstf, srcf, cpw);
			dstf += cpw * 2;
			srcf += cpw * 2;
		}

		// Cr component
		srcf = buffers[src].V + srcfield;
		dstf = buffers[dst].V + dstfield;

		for (y = 0; y < cph; ++y)
		{
			memcpy(dstf, srcf, cpw);
			dstf += cpw * 2;
			srcf += cpw * 2;
		}

		// Invalidate this frame number,
		// and flag the picture as interlaced
		buffers[dst].frame_num = -1;
		buffers[dst].bitflags &= (~PICTUREFLAG_PF);
	}
}


/*********************************************************
	Integer IDCT, based on Chen-Wang algorithm
**********************************************************/

#define CW_W1	2841	// 2048*sqrt(2)*cos(1*pi/16)
#define CW_W2	2676	// 2048*sqrt(2)*cos(2*pi/16)
#define CW_W3	2408	// 2048*sqrt(2)*cos(3*pi/16)
#define CW_W5	1609	// 2048*sqrt(2)*cos(5*pi/16)
#define CW_W6	1108	// 2048*sqrt(2)*cos(6*pi/16)
#define CW_W7	565		// 2048*sqrt(2)*cos(7*pi/16)

// Note: substituting 1567 for (CW_W2 - CW_W6) is more accurate
#define CW_W2minusCW_W6 1567

inline void MPEG2Decoder::idctrow(short *blk)
{
	int x4, x3, x7, x1, x6, x2, x5, x0, x8;

	// pos is (col * 8 + #)
	x4 = blk[1];
	x3 = blk[2];
	x7 = blk[3];
	x1 = (int)blk[4] << 11;
	x6 = blk[5];
	x2 = blk[6];
	x5 = blk[7];

	if ((x4 | x3 | x7 | x1 | x6 | x2 | x5) == 0)
	{
		// Shortcut...
		x0 = blk[0] << 3;

		blk[0] = blk[1] = blk[2] = blk[3] = blk[4]
		       = blk[5] = blk[6] = blk[7] = (short)x0;
		return;
	}

	x0 = ((int)blk[0] << 11) + 128;	// for proper rounding

	// first stage
	x8 = CW_W7 * (x4 + x5);             // w7    =  565
	x4 = x8 + (CW_W1 - CW_W7) * x4;     // w1-w7 = 2276
	x5 = x8 - (CW_W1 + CW_W7) * x5;     // w1+w7 = 3406
	x8 = CW_W3 * (x6 + x7);             // w3    = 2408
	x6 = x8 - (CW_W3 - CW_W5) * x6;     // w3-w5 =  799
	x7 = x8 - (CW_W3 + CW_W5) * x7;     // w3+w5 = 4017
	x8 = CW_W6 * (x3 + x2);             // w6    = 1108
	x2 = x8 - (CW_W2 + CW_W6) * x2;     // w2+w6 = 3784
	x3 = x8 + CW_W2minusCW_W6 * x3;     // w2-w6 = 1567

	// second stage
	x8 = x0 + x1; x0 -= x1;
	x1 = x4 + x6; x4 -= x6;
	x6 = x5 + x7; x5 -= x7;
	x7 = x8 + x3; x8 -= x3;
	x3 = x0 + x2; x0 -= x2;
	// Note: 181 = 128 * sqrt(2)
	x2 = (181 * (x4 + x5) + 128) >> 8;
	x4 = (181 * (x4 - x5) + 128) >> 8;

	// final stage
	blk[0] = (short)((x7 + x1) >> 8);
	blk[1] = (short)((x3 + x2) >> 8);
	blk[2] = (short)((x0 + x4) >> 8);
	blk[3] = (short)((x8 + x6) >> 8);
	blk[4] = (short)((x8 - x6) >> 8);
	blk[5] = (short)((x0 - x4) >> 8);
	blk[6] = (short)((x3 - x2) >> 8);
	blk[7] = (short)((x7 - x1) >> 8);
}

inline void MPEG2Decoder::idctcol(short *blk)
{
	int x4, x3, x7, x1, x6, x2, x5, x0, x8;

	x4 = blk[8];
	x3 = blk[16];
	x7 = blk[24];
	x1 = (int)blk[32] << 8;
	x6 = blk[40];
	x2 = blk[48];
	x5 = blk[56];

	if ((x4 | x3 | x7 | x1 | x6 | x2 | x5) == 0)
	{
		// Shortcut...
		x0 = (blk[0] + 32) >> 6;
		if (x0 < -256) x0 = -256; else if (x0 > 255) x0 = 255;

		blk[0] = blk[8] = blk[16] = blk[24] = blk[32]
		       = blk[40] = blk[48] = blk[56] = (short)x0;
		return;
	}

	x0 = ((int)blk[0] << 8) + 8192;

	// first stage
	x8 = (CW_W7 * (x4 + x5)) + 4;
	x4 = (x8 + (CW_W1 - CW_W7) * x4) >> 3;
	x5 = (x8 - (CW_W1 + CW_W7) * x5) >> 3;
	x8 = (CW_W3 * (x6 + x7)) + 4;
	x6 = (x8 - (CW_W3 - CW_W5) * x6) >> 3;
	x7 = (x8 - (CW_W3 + CW_W5) * x7) >> 3;
	x8 = (CW_W6 * (x3 + x2)) + 4;
	x2 = (x8 - (CW_W2 + CW_W6) * x2) >> 3;
	x3 = (x8 + CW_W2minusCW_W6 * x3) >> 3;

	// second stage
	x8 = x0 + x1; x0 -= x1;
	x1 = x4 + x6; x4 -= x6;
	x6 = x5 + x7; x5 -= x7;
	x7 = x8 + x3; x8 -= x3;
	x3 = x0 + x2; x0 -= x2;
	x2 = (181 * (x4 + x5) + 128) >> 8;
	x4 = (181 * (x4 - x5) + 128) >> 8;

	// final stage
	x5 = (x7 + x1) >> 14;
	if (x5 < -256) x5 = -256; else if (x5 > 255) x5 = 255;
	blk[0] = (short)x5;

	x5 = (x3 + x2) >> 14;
	if (x5 < -256) x5 = -256; else if (x5 > 255) x5 = 255;
	blk[8] = (short)x5;

	x5 = (x0 + x4) >> 14;
	if (x5 < -256) x5 = -256; else if (x5 > 255) x5 = 255;
	blk[16] = (short)x5;

	x5 = (x8 + x6) >> 14;
	if (x5 < -256) x5 = -256; else if (x5 > 255) x5 = 255;
	blk[24] = (short)x5;

	x5 = (x8 - x6) >> 14;
	if (x5 < -256) x5 = -256; else if (x5 > 255) x5 = 255;
	blk[32] = (short)x5;

	x5 = (x0 - x4) >> 14;
	if (x5 < -256) x5 = -256; else if (x5 > 255) x5 = 255;
	blk[40] = (short)x5;

	x5 = (x3 - x2) >> 14;
	if (x5 < -256) x5 = -256; else if (x5 > 255) x5 = 255;
	blk[48] = (short)x5;

	x5 = (x7 - x1) >> 14;
	if (x5 < -256) x5 = -256; else if (x5 > 255) x5 = 255;
	blk[56] = (short)x5;
}

inline void MPEG2Decoder::Fast_IDCT(short *src)
{
	// two dimensional inverse discrete cosine transform
	int i;
	short *blk;
	for (i = 0, blk = src; i < 8; ++i, blk += 8) idctrow(blk);
	for (i = 0, blk = src; i < 8; ++i, ++blk) idctcol(blk);
}


void MPEG2Decoder::Add_Block(int comp, int bx, int by, int addflag)
{
	YCCSample *rfp;
	int rfp_pitch;

	if (comp < 4)
	{
		// luminance
		rfp = current_frame->Y;

		if (picture_structure == FRAME_PICTURE)
		{
			// frame picture
			if (dct_type) {
				// field DCT coding
				rfp += coded_picture_width * (by + ((comp & 2) >> 1)) + bx + ((comp & 1) << 3);
				rfp_pitch = coded_picture_width << 1;
			} else {
				// frame DCT coding
				rfp += coded_picture_width * (by + ((comp & 2) << 2)) + bx + ((comp & 1) << 3);
				rfp_pitch = coded_picture_width;
			}
		}
		else
		{
			// field picture
			if (picture_structure == BOTTOM_FIELD) rfp += coded_picture_width;
			rfp += (coded_picture_width << 1) * (by + ((comp & 2) << 2)) + bx + ((comp & 1) << 3);
			rfp_pitch = coded_picture_width << 1;
		}
	}
	else
	{
		// chrominance
		int uv_pitch;

		if (comp & 1) {
			rfp = current_frame->V;
		} else {
			rfp = current_frame->U;
		}

		// scale coordinates
		if (chroma_format != CHROMA444) {
			bx >>= 1;
			if (chroma_format == CHROMA420) by >>= 1;
			uv_pitch = coded_picture_width >> 1;
		} else {
			uv_pitch = coded_picture_width;
		}

		if (picture_structure == FRAME_PICTURE)
		{
			if (dct_type && chroma_format != CHROMA420)
			{
				// field DCT coding
				rfp += uv_pitch * (by + ((comp & 2) >> 1)) + bx + (comp & 8);
				rfp_pitch = uv_pitch << 1;
			} else {
				// frame DCT coding
				rfp += uv_pitch * (by + ((comp & 2) << 2)) + bx + (comp & 8);
				rfp_pitch = uv_pitch;
			}
		}
		else
		{
			// field picture
			if (picture_structure == BOTTOM_FIELD) rfp += uv_pitch;
			rfp += (uv_pitch << 1) * (by + ((comp & 2) << 2)) + bx + (comp & 8);
			rfp_pitch = uv_pitch << 1;
		}
	}

	if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE))
	{
		mmx_Add_Block(dct_coeff[comp], rfp, rfp_pitch, addflag);
	}
	else
	{
		const short *blk = dct_coeff[comp];

		if (addflag)
		{
			for (int y = 0; y < 8; ++y)
			{
				for (int x = 0; x < 8; ++x)
				{
					int p = *blk++ + rfp[x];
					if ((unsigned int)p > 255) p = (~p >> 31) & 255;
					rfp[x] = (YCCSample)p;
				}
				rfp += rfp_pitch;
			}
		}
		else
		{
			for (int y = 0; y < 8; ++y)
			{
				for (int x = 0; x < 8; ++x)
				{
					int p = *blk++ + 128;
					if ((unsigned int)p > 255) p = (~p >> 31) & 255;
					rfp[x] = (YCCSample)p;
				}
				rfp += rfp_pitch;
			}
		}
	}
}


int MPEG2Decoder::get_luma_diff()
{
	int size;
	// decode length
	int code = show_bits(9);	// 1 1111 ....

	if (code < 0x1F0) {
		code >>= 4;
		size = DClumtab0[code].val;
		flush_buffer(DClumtab0[code].len);
	} else {
		code &= 0xF;
		size = DClumtab1[code].val;
		flush_buffer(DClumtab1[code].len);
	}

	if (size > 0)
	{
		code = get_bits(size);  // maximum is 11 bits
		size = 1 << (size - 1);
		if (code < size) code = (code + 1) - (size + size);
		return code;
	}
	return 0;
}


int MPEG2Decoder::get_chroma_diff()
{
	int size;
	// decode length
	int code = show_bits(10);

	if (code < 0x3E0) {
		code >>= 5;
		size = DCchromtab0[code].val;
		flush_buffer(DCchromtab0[code].len);
	} else {
		code &= 0x1F;
		size = DCchromtab1[code].val;
		flush_buffer(DCchromtab1[code].len);
	}

	if (size > 0)
	{
		code = get_bits(size);	// maximum is 11 bits
		size = 1 << (size - 1);
		if (code < size) code = (code + 1) - (size + size);
		return code;
	}
	return 0;
}


// decode one intra coded MPEG-1 block
void MPEG2Decoder::Decode_MPEG1_Intra_Block(int comp)
{
	int code, val, i, j, sign;
	const DCTtab *tab;

	short *blk = dct_coeff[comp];

	// decode DC coefficients
	if (comp < 4)       // Y
		val = (dct_dc_y_past += get_luma_diff());
	else if (comp & 1)  // V
		val = (dct_dc_v_past += get_chroma_diff());
	else                // U
		val = (dct_dc_u_past += get_chroma_diff());

	blk[0] = (short)(val << 3);

	if (frame_type == MPEG_FRAME_TYPE_D) return;

	// decode AC coefficients
	for (i = 1; ; ++i)
	{
		code = show_bits(16);
		if (code >= 16384)
			tab = &DCTtabnext[(code >> 12) - 4];
		else if (code >= 1024)
			tab = &DCTtab0[(code >> 8) -  4];
		else if (code >= 512)
			tab = &DCTtab1[(code >> 6) -  8];
		else if (code >= 256)
			tab = &DCTtab2[(code >> 4) - 16];
		else if (code >= 128)
			tab = &DCTtab3[(code >> 3) - 16];
		else if (code >= 64)
			tab = &DCTtab4[(code >> 2) - 16];
		else if (code >= 32)
			tab = &DCTtab5[(code >> 1) - 16];
		else if (code >= 16)
			tab = &DCTtab6[code - 16];
		else {
			SetError(kErrorBadValue);
			break;
		}

		flush_buffer(tab->len);
		val = tab->run;

		if (val == 65)
		{
			// escape
			i += get_bits(6);

			val = get_bits(8);
			if (val == 0)
				val = get_bits(8);
			else if (val == 128)
				val = get_bits(8) - 256;
			else if (val > 128)
				val -= 256;

			sign = (val < 0)? 1: 0;
			if (sign) val = -val;
		}
		else
		{
			if (val == 64) break;
			i += val;
			val = tab->level;
			sign = get_bits(1);
		}

		if (i >= 64)
		{
			SetError(kErrorTooManyCoefficients);
			break;
		}

		j = zigzag[0][i];
		val = (val * quantizer_scale * intramatrix[j]) >> 3;

		if (val) val = (val - 1) | 1;       // mismatch
		if (val >= 2048) val = 2047 + sign; // saturation
		if (sign) val = -val;

		blk[j] = (short)val;
    }
}


// decode one non-intra coded MPEG-1 block
inline void MPEG2Decoder::Decode_MPEG1_Non_Intra_Block(int comp)
{
	int code, val, i, j, sign;
	const DCTtab *tab;

	short *blk = dct_coeff[comp];

	// decode AC coefficients
	for (i = 0; ; ++i)
	{
		code = show_bits(16);
		if (code >= 16384)
			if (i)  tab = &DCTtabnext [(code >> 12) - 4];
			else    tab = &DCTtabfirst[(code >> 12) - 4];
		else if (code >= 1024)
			tab = &DCTtab0[(code >> 8) -  4];
		else if (code >= 512)
			tab = &DCTtab1[(code >> 6) -  8];
		else if (code >= 256)
			tab = &DCTtab2[(code >> 4) - 16];
		else if (code >= 128)
			tab = &DCTtab3[(code >> 3) - 16];
		else if (code >= 64)
			tab = &DCTtab4[(code >> 2) - 16];
		else if (code >= 32)
			tab = &DCTtab5[(code >> 1) - 16];
		else if (code >= 16)
			tab = &DCTtab6[code - 16];
		else {
			SetError(kErrorBadValue);
			break;
		}

		flush_buffer(tab->len);
		val = tab->run;

		if (val == 65)
		{
			// escape
			i+= get_bits(6);

			val = get_bits(8);
			if (val == 0)
				val = get_bits(8);
			else if (val == 128)
				val = get_bits(8) - 256;
			else if (val > 128)
				val -= 256;

			sign = (val < 0)? 1: 0;
			if (sign) val = -val;
		}
		else
		{
			if (val == 64) break;
			i += val;
			val = tab->level;
			sign = get_bits(1);
		}

		if (i >= 64)
		{
			SetError(kErrorTooManyCoefficients);
			break;
		}

		j = zigzag[0][i];
		val = ((val * 2 + 1) * quantizer_scale * nonintramatrix[j]) >> 4;

		if (val) val = (val - 1) | 1;       // mismatch
		if (val >= 2048) val = 2047 + sign; // saturation
		if (sign) val = -val;

		blk[j] = (short)val;
	}
}


// decode one intra coded MPEG-2 block
inline void MPEG2Decoder::Decode_MPEG2_Intra_Block(int comp)
{
	int code, val, i, j, sign, sum;
	const DCTtab *tab;

	short *blk = dct_coeff[comp];

	const unsigned char *qmat =
		(comp < 4 || chroma_format == CHROMA420)?
		intramatrix: chromaintramatrix;

	// DC coefficient
	if (comp < 4)		// Y
		val = (dct_dc_y_past += get_luma_diff());
	else if (comp & 1)	// V
		val = (dct_dc_v_past += get_chroma_diff());
	else				// U
		val = (dct_dc_u_past += get_chroma_diff());

	sum = val << (3 - intra_dc_precision);
	blk[0] = (short)sum;

	// AC coefficients
	for (i = 1; ; ++i)
	{
		code = show_bits(16);

		if (code >= 16384)
			if (intra_vlc_format)   tab = &DCTtab0a  [(code >> 8 ) - 4];
			else                    tab = &DCTtabnext[(code >> 12) - 4];
		else if (code >= 1024)
			if (intra_vlc_format)   tab = &DCTtab0a[(code >> 8) - 4];
			else                    tab = &DCTtab0 [(code >> 8) - 4];
		else if (code >= 512)
			if (intra_vlc_format)   tab = &DCTtab1a[(code >> 6) - 8];
			else                    tab = &DCTtab1 [(code >> 6) - 8];
		else if (code >= 256)
			tab = &DCTtab2[(code >> 4) - 16];
		else if (code >= 128)
			tab = &DCTtab3[(code >> 3) - 16];
		else if (code >= 64)
			tab = &DCTtab4[(code >> 2) - 16];
		else if (code >= 32)
			tab = &DCTtab5[(code >> 1) - 16];
		else if (code >= 16)
			tab = &DCTtab6[code - 16];
		else {
			SetError(kErrorBadValue);
			break;
		}

		flush_buffer(tab->len);
		val = tab->run;

		if (val == 65)
		{
			// escape
			i += get_bits(6);
			val = get_bits(12);
			if (!(val & 2047))
			{
				SetError(kErrorBadValue);
				break;
			}
			sign = val >> 11;
			if (sign) val = 4096 - val;
		}
		else
		{
			if (val == 64) break;
			i += val;
			val = tab->level;
			sign = get_bits(1);
		}

		if (i >= 64)
		{
			SetError(kErrorTooManyCoefficients);
			break;
		}

		j = zigzag[alternate_scan][i];
		val = (val * quantizer_scale * qmat[j]) >> 4;

		if (val >= 2048) val = 2047 + sign;	// saturation
		if (sign) val = -val;

		blk[j] = (short)val;
		sum ^= val;		// mismatch
	}

	if (!mErrorState)
	{
		if (!(sum & 1)) blk[63] ^= 1;	// mismatch control
	}
}


// decode one non-intra coded MPEG-2 block
inline void MPEG2Decoder::Decode_MPEG2_Non_Intra_Block(int comp)
{
	int code, val, i, j, sign, sum;
	const DCTtab *tab;

	short *blk = dct_coeff[comp];

	const unsigned char *qmat =
		(comp < 4 || chroma_format == CHROMA420)?
		nonintramatrix: chromanonintramatrix;

	// AC coefficients
	sum = 0;
	for (i = 0; ; ++i)
	{
		code = show_bits(16);

		if (code >= 16384)
			if (i)  tab = &DCTtabnext [(code >> 12) - 4];
			else    tab = &DCTtabfirst[(code >> 12) - 4];
		else if (code >= 1024)
			tab = &DCTtab0[(code >> 8) -  4];
		else if (code >= 512)
			tab = &DCTtab1[(code >> 6) -  8];
		else if (code >= 256)
			tab = &DCTtab2[(code >> 4) - 16];
		else if (code >= 128)
			tab = &DCTtab3[(code >> 3) - 16];
		else if (code >= 64)
			tab = &DCTtab4[(code >> 2) - 16];
		else if (code >= 32)
			tab = &DCTtab5[(code >> 1) - 16];
		else if (code >= 16)
			tab = &DCTtab6[code - 16];
		else {
			SetError(kErrorBadValue);
			break;
		}

		flush_buffer(tab->len);
		val = tab->run;

		if (val == 65)
		{
			// escape
			i += get_bits(6);
			val = get_bits(12);
			if (!(val & 2047))
			{
				SetError(kErrorBadValue);
				break;
			}
			sign = val >> 11;
			if (sign) val = 4096 - val;
		}
		else
		{
			if (val == 64) break;
			i += val;
			val = tab->level;
			sign = get_bits(1);
		}

		if (i >= 64)
		{
			SetError(kErrorTooManyCoefficients);
			break;
		}

		j = zigzag[alternate_scan][i];
		val = ((val * 2 + 1) * quantizer_scale * qmat[j]) >> 5;

		if (val >= 2048) val = 2047 + sign;	// saturation
		if (sign) val = -val;

		blk[j] = (short)val;
		sum ^= val;		// mismatch
	}

	if (!mErrorState)
	{
		if (!(sum & 1)) blk[63] ^= 1;	// mismatch control
	}
}


///////////////////////////
// Prediction (replaces a_predict* code)

void MPEG2Decoder::form_component_prediction(
	const YCCSample* src, YCCSample* dst, int w, int h, int lx,
	int x, int y, int dx, int dy, int average_flag)
{

// src is a YCbCr field from where the prediction will be read.
// dst is a YCbCr field where the prediction will be written.

// average_flag is used for bidirectional prediction.  If average_flag
// is nonzero, then we blend (50/50) the new prediction with a previous
// prediction which is already in the destination macroblock.

// lx is the horizontal pitch (in pels) of both src and dst.
// (x, y) is the position of the current macroblock (in pels).
// (dx, dy) is a vector to the src prediction (in half pels).

// Calculate the macroblock positions in src and dst:

	src += lx * (y + (dy >> 1)) + x + (dx >> 1);
	dst += lx * y + x;

// Now bounds check the prediction.  Only the source macroblock
// needs to be checked; the destination is always in range.

// Our 3 YCbCr buffers span the range from "memblock" to "uv422",
// so anything outside that range is an out of bounds prediction.

// "src" always increments, so as long as it starts out greater
// than or equal to "memblock" we're safe.  It may increment beyond
// "uv422", but we're still safe because there is PLENTY of allocated
// memory up there, and it will never creep farther than 32 * Y_pitch
// into that area.  Note: data at "src" is only read, never written.

// (The only way the above can happen is if we're passed an out of
// bounds motion vector.  So if that's the case, we don't care about
// "perfect" decoding anymore, we just care about NOT crashing!)

	if (((DWORD_PTR)src >= (DWORD_PTR)memblock) && ((DWORD_PTR)src < (DWORD_PTR)uv422))
	{
		if (cpuflags & CPUFLAG_ISSE)
		{
			isse_form_component_prediction(src, dst,
				w, h, lx, dx, dy, average_flag);
		}
		else if (cpuflags & CPUFLAG_MMX)
		{
			mmx_form_component_prediction(src, dst,
				w, h, lx, dx, dy, average_flag);
		}
		else
		{
			int i, j;

/*	The following are equivalent:

a)	result = (s1 + s2 + 1) >> 1;
b)	result = (s1 >> 1) + (s2 >> 1) + ((s1 | s2) & 1);

a)	result = (d + ((s1 + s2 + 1) >> 1) + 1) >> 1;
b)	result = (s1 + s2 + d + d + 3) >> 2;

a)	result = (d + ((s1 + s2 + s3 + s4 + 2) >> 2) + 1) >> 1;
b)	result = (s1 + s2 + s3 + s4 + d + d + d + d + 6) >> 3;
*/

			if (dx & 1) {
				if (dy & 1) {
					// horizontal and vertical half-pel
					if (average_flag) {
						for (j = 0; j < h; ++j) {
							for (i = 0; i < w; ++i) {
								int v = src[i] + src[i + 1]
									+ src[i + lx] + src[i + lx + 1];

								v = dst[i] + ((v + 2) >> 2);
								dst[i] = (YCCSample)((v + 1) >> 1);
							}
							src += lx;
							dst += lx;
						}
					} else {
						for (j = 0; j < h; ++j) {
							for (i = 0; i < w; ++i) {
								int v = src[i] + src[i + 1]
									+ src[i + lx] + src[i + lx + 1];

								dst[i] = (YCCSample)((v + 2) >> 2);
							}
							src += lx;
							dst += lx;
						}
					}
				} else {
					// horizontal but no vertical half-pel
					if (average_flag) {
						for (j = 0; j < h; ++j) {
							for (i = 0; i < w; ++i) {
								int v = src[i] + src[i + 1];
								v = dst[i] + ((v + 1) >> 1);
								dst[i] = (YCCSample)((v + 1) >> 1);
							}
							src += lx;
							dst += lx;
						}
					} else {
						for (j = 0; j < h; ++j) {
							for (i = 0; i < w; ++i) {
								int v = src[i] + src[i + 1];
								dst[i] = (YCCSample)((v + 1) >> 1);
							}
							src += lx;
							dst += lx;
						}
					}
				}
			} else if (dy & 1) {
				// no horizontal but vertical half-pel
				if (average_flag) {
					for (j = 0; j < h; ++j) {
						for (i = 0; i < w; ++i) {
							int v = src[i] + src[i + lx];
							v = dst[i] + ((v + 1) >> 1);
							dst[i] = (YCCSample)((v + 1) >> 1);
						}
						src += lx;
						dst += lx;
					}
				} else {
					for (j = 0; j < h; ++j) {
						for (i = 0; i < w; ++i) {
							int v = src[i] + src[i + lx];
							dst[i] = (YCCSample)((v + 1) >> 1);
						}
						src += lx;
						dst += lx;
					}
				}
			} else {
				// no horizontal nor vertical half-pel
				if (average_flag) {
					for (j = 0; j < h; ++j) {
						for (i = 0; i < w; ++i) {
							int v = dst[i] + src[i];
							dst[i] = (YCCSample)((v + 1) >> 1);
						}
						src += lx;
						dst += lx;
					}
				} else {
					for (j = 0; j < h; ++j) {
						for (i = 0; i < w; ++i) {
							dst[i] = src[i];
						}
						src += lx;
						dst += lx;
					}
				}
			}
		}
	}
	else
	{
		SetError(kErrorBadMotionVector);
	}
}


void MPEG2Decoder::form_prediction(
	MPEGBuffer *srcBuf, int sfield, MPEGBuffer *dstBuf, int dfield,
	int lx, int w, int h, int x, int y, int dx, int dy, int average_flag)
{
	// Note: lx is always coded_picture_width or coded_picture_width * 2
	// Note: w is always 16
	// Note: h is always 8 or 16

	// Distance to the next field (if applicable):
	if (sfield) sfield = lx >> 1;
	if (dfield) dfield = lx >> 1;

	// Get the Y component prediction
	form_component_prediction(
		srcBuf->Y + sfield, dstBuf->Y + dfield,
		w, h, lx, x, y, dx, dy, average_flag
	);

	// Adjust the parameters for lower resolution chroma
	if (chroma_format != CHROMA444)
	{
		// 4:2:2 or 4:2:0
		sfield >>= 1;	// distance to next src field is halved
		dfield >>= 1;	// distance to next dst field is halved
		lx >>= 1;		// horizontal pitch is halved
		w >>= 1;		// width of prediction is halved
		x >>= 1;		// horizontal macroblock position is halved
		dx /= 2;		// horizontal motion vector is halved

		if (chroma_format == CHROMA420)
		{
			// 4:2:0
			h >>= 1;	// height of prediction is halved
			y >>= 1;	// vertical macroblock position is halved
			dy /= 2;	// vertical motion vector is halved
		}
	}

	// Get the Cb component prediction
	form_component_prediction(
		srcBuf->U + sfield, dstBuf->U + dfield,
		w, h, lx, x, y, dx, dy, average_flag
	);

	// Get the Cr component prediction
	form_component_prediction(
		srcBuf->V + sfield, dstBuf->V + dfield,
		w, h, lx, x, y, dx, dy, average_flag
	);
}


void MPEG2Decoder::dual_prime(int DMV[][2], int mvx, int mvy)
{
	if (picture_structure == FRAME_PICTURE) {
		if (current_frame->bitflags & PICTUREFLAG_TFF) {
			DMV[0][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
			DMV[0][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] - 1;
			DMV[1][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
			DMV[1][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] + 1;
		} else {
			DMV[0][0] = ((3*mvx+(mvx>0))>>1) + dmvector[0];
			DMV[0][1] = ((3*mvy+(mvy>0))>>1) + dmvector[1] - 1;
			DMV[1][0] = ((mvx  +(mvx>0))>>1) + dmvector[0];
			DMV[1][1] = ((mvy  +(mvy>0))>>1) + dmvector[1] + 1;
		}
	} else {
		DMV[0][0] = ((mvx+(mvx>0))>>1) + dmvector[0];
		DMV[0][1] = ((mvy+(mvy>0))>>1) + dmvector[1];
		if (picture_structure == TOP_FIELD) {
			--DMV[0][1];
		} else {
			++DMV[0][1];
		}
	}
}


//#define forw_frame (&(buffers[MPEG_BUFFER_FORWARD]))
//#define back_frame (&(buffers[MPEG_BUFFER_BACKWARD]))

void MPEG2Decoder::form_predictions(int bx, int by)
{
	int addflag = 0;

	if ((macro_block_flags & MBF_FORWARD) || frame_type == MPEG_FRAME_TYPE_P)
	{
		if (picture_structure == FRAME_PICTURE)
		{
			if (motion_type == MC_FRAME || !(macro_block_flags & MBF_FORWARD))
			{
				// frame-based prediction
				form_prediction(
					forw_frame, 0,
					current_frame, 0,
					coded_picture_width,
					16, 16, bx, by,
					PMV[0][0][0], PMV[0][0][1], 0
				);
			}
			else if (motion_type == MC_FIELD)
			{
				// field-based prediction

				// top field prediction
				form_prediction(
					forw_frame, mvfield_select[0][0],
					current_frame, 0,
					coded_picture_width << 1,
					16, 8, bx, by >>1,
					PMV[0][0][0], PMV[0][0][1] >> 1, 0
				);

				// bottom field prediction
				form_prediction(
					forw_frame, mvfield_select[1][0],
					current_frame, 1,
					coded_picture_width << 1,
					16, 8, bx, by >> 1,
					PMV[1][0][0], PMV[1][0][1] >> 1, 0
				);
			}
			else if (motion_type==MC_DMV)
			{
				// dual prime prediction

				// calculate derived motion vectors
				int DMV[2][2];
				dual_prime(DMV, PMV[0][0][0], PMV[0][0][1]>>1);

				// predict top field from top field
				form_prediction(
					forw_frame, 0,
					current_frame, 0,
					coded_picture_width << 1,
					16, 8, bx, by >> 1,
					PMV[0][0][0], PMV[0][0][1] >> 1, 0
				);

				// predict and add to top field from bottom field
				form_prediction(
					forw_frame, 1, current_frame, 0,
					coded_picture_width << 1,
					16, 8, bx, by >> 1,
					DMV[0][0], DMV[0][1], 1
				);

				// predict bottom field from bottom field
				form_prediction(
					forw_frame, 1, current_frame, 1,
					coded_picture_width << 1,
					16, 8, bx, by >> 1,
					PMV[0][0][0], PMV[0][0][1] >> 1, 0
				);

				// predict and add to bottom field from top field */
				form_prediction(
					forw_frame, 0,
					current_frame, 1,
					coded_picture_width << 1,
					16, 8, bx, by >> 1,
					DMV[1][0], DMV[1][1], 1
				);
			}
		}
		else
		{
			// TOP_FIELD or BOTTOM_FIELD

// While trying to rid myself of the "second_field" variable
// and have all buffer selection controlled externally, I've
// come to a better understanding of this next bit of code.
//
// If this is a B-field, it always predicts from the FORWARD
// buffer, even if the B-field is the second of two fields,
// because we never form predictions from B-pictures (that
// makes sense). "Second_field" is unimportant in this case.
//
// If this is a P-field, and it is the first of two P-fields,
// it always predicts from the FORWARD buffer because that's
// where the previous reference frame (or fields) reside.
//
// But if this is a P-field and it is the second of two fields
// (either I or P was the first), then there is this complication:
// If it is predicting from the field of the same parity, that
// field resides in the FORWARD buffer. If it is predicting from
// a field of opposite parity, that resides in the BACKWARD buffer.
//
// So ultimately this REQUIRES us to know whether we are decoding
// the second field of a coded frame or the first field, thus the
// "second_field" variable can't be eliminated entirely.

			MPEGBuffer *predframe;

			// field picture
			const int currentfield = (picture_structure == BOTTOM_FIELD);

			// determine which frame to use for prediction
			if (frame_type == MPEG_FRAME_TYPE_P && second_field
				&& currentfield != mvfield_select[0][0])
			{
				predframe = back_frame; // same frame
			} else {
				predframe = forw_frame; // previous frame
			}

			if (motion_type == MC_FIELD || !(macro_block_flags & MBF_FORWARD))
			{
				// field-based prediction
				form_prediction(
					predframe, mvfield_select[0][0],
					current_frame, currentfield,
					coded_picture_width << 1,
					16, 16, bx, by,
					PMV[0][0][0], PMV[0][0][1], 0
				);
			}
			else if (motion_type == MC_16X8)
			{
				// predict upper half
				form_prediction(
					predframe, mvfield_select[0][0],
					current_frame, currentfield,
					coded_picture_width << 1,
					16, 8, bx, by,
					PMV[0][0][0], PMV[0][0][1], 0
				);

				// determine which frame to use for lower half prediction
				if (frame_type == MPEG_FRAME_TYPE_P && second_field
					&& currentfield != mvfield_select[1][0])
				{
					predframe = back_frame; // same frame
				} else {
					predframe = forw_frame; // previous frame
				}

				// predict lower half
				form_prediction(
					predframe, mvfield_select[1][0],
					current_frame, currentfield,
					coded_picture_width << 1,
					16, 8, bx, by + 8,
					PMV[1][0][0], PMV[1][0][1], 0
				);

			}
			else if (motion_type == MC_DMV)
			{
				// dual prime prediction

// Note what the following is REALLY saying... If this is a field
// picture and it is the second of two field pictures, then it
// predicts from the previous field picture, which just happens
// to have been decoded into the same buffer we are now decoding
// into!  If it is the first of two field pictures, it predicts
// from the previous reference buffer, which contains the previous
// field (and/or frame picture) that came before.

// In short, we ALWAYS predict from the previous field/frame;
// "predframe" simply indicates in which buffer it resides.

				// calculate derived motion vectors
				int DMV[2][2];
				dual_prime(DMV, PMV[0][0][0], PMV[0][0][1]);

				if (second_field) {
					predframe = back_frame; // same frame
				} else {
					predframe = forw_frame; // previous frame
				}

				// predict currentfield from field of same parity
				form_prediction(
					forw_frame, currentfield,
					current_frame, currentfield,
					coded_picture_width << 1,
					16, 16, bx, by,
					PMV[0][0][0], PMV[0][0][1], 0
				);

				// predict other field from field of opposite parity
				form_prediction(
					predframe, !currentfield,
					current_frame, currentfield,
					coded_picture_width << 1,
					16, 16, bx, by,
					DMV[0][0], DMV[0][1], 1
				);
			}
		}

		// We have fetched one prediction.  So if another
		// prediction is made then it must be bidirectional:
		addflag = 1;
	}

	if (macro_block_flags & MBF_BACKWARD)
	{
		if (picture_structure == FRAME_PICTURE)
		{
			if (motion_type == MC_FRAME)
			{
				// frame-based prediction
				form_prediction(
					back_frame, 0,
					current_frame, 0,
					coded_picture_width,
					16, 16, bx, by,
					PMV[0][1][0], PMV[0][1][1], addflag
				);
			}
			else
			{
				// field-based prediction

				// top field prediction
				form_prediction(
					back_frame, mvfield_select[0][1],
					current_frame, 0,
					coded_picture_width << 1,
					16, 8, bx, by >> 1,
					PMV[0][1][0], PMV[0][1][1] >> 1, addflag
				);

				// bottom field prediction
				form_prediction(
					back_frame, mvfield_select[1][1],
					current_frame, 1,
					coded_picture_width << 1,
					16, 8, bx, by >> 1,
					PMV[1][1][0], PMV[1][1][1] >> 1, addflag
				);
			}
		}
		else
		{
			// TOP_FIELD or BOTTOM_FIELD

			// field picture
			const int currentfield = (picture_structure==BOTTOM_FIELD);

			if (motion_type == MC_FIELD)
			{
				// field-based prediction
				form_prediction(
					back_frame, mvfield_select[0][1],
					current_frame, currentfield,
					coded_picture_width << 1,
					16, 16, bx, by,
					PMV[0][1][0], PMV[0][1][1], addflag
				);
			}
			else if (motion_type == MC_16X8)
			{
				form_prediction(
					back_frame, mvfield_select[0][1],
					current_frame, currentfield,
					coded_picture_width << 1,
					16, 8, bx, by,
					PMV[0][1][0], PMV[0][1][1], addflag
				);

				form_prediction(
					back_frame, mvfield_select[1][1],
					current_frame, currentfield,
					coded_picture_width << 1,
					16, 8, bx, by + 8,
					PMV[1][1][0], PMV[1][1][1], addflag
				);
			}
		}
	}
}


///////////////////////////

int MPEG2Decoder::mpeg_get_motion_component()
{
	if (get_bits(1) == 0)
	{
		int code = show_bits(9);
		if (code >= 64) {
			code >>= 6;
			flush_buffer(MVtab0[code].len);
			code = MVtab0[code].val;
		} else if (code >= 24) {
			code >>= 3;
			flush_buffer(MVtab1[code].len);
			code = MVtab1[code].val;
		} else if (code >= 12) {
			code -= 12;
			flush_buffer(MVtab2[code].len);
			code = MVtab2[code].val;
		} else {
			SetError(kErrorBadMotionVector);
			return 0;
		}
		if (get_bits(1)) code = -code;
		return code;
	}
	return 0;
}


bool MPEG2Decoder::get_mb_address_inc(int& val)
{
	// The macroblock_address_increment is a required element,
	// therefore we must return false if it cannot be recovered.
	// This triggers slice() to resynchronize at the next slice.

	// Note that we use the same mechanism to detect when there
	// are no more macroblocks, which will likewise trigger slice()
	// to resync at the next slice.  This will happen if we hit a
	// string of 11 zero bits (cf. ISO/IEC 13818-2 section 6.2.4).

	for (val = 0; ;)
	{
		int code = show_bits(11);

		if (code >= 0x400)
		{
			flush_buffer(1);
			++val;
			return true;
		}

		if (code >= 0x80)
		{
			code >>= 6;
			flush_buffer(MBAtab0[code].len);
			val += MBAtab0[code].val;
			return true;
		}

		if (code >= 0x18)
		{
			code -= 24;
			flush_buffer(MBAtab1[code].len);
			val += MBAtab1[code].val;
			return true;
		}

		if (code != 15)             // not macroblock_stuffing
		{
			if (code != 8) break;   // not macroblock_escape
			val += 33;
		}

		flush_buffer(11);
	}

	return false;
}


inline int MPEG2Decoder::get_I_mb_flags()
{
// the only valid codes are 1, or 01
	int code = show_bits(2);
	if (code & 2) {
		flush_buffer(1);
		return MBF_INTRA;
	}
	if (code & 1) {
		flush_buffer(2);
	} else {
		SetError(kErrorBadValue);
	}
	return (MBF_INTRA|MBF_QUANT);
}


inline int MPEG2Decoder::get_P_mb_flags()
{
	int code = show_bits(6);
	if (code >= 8) {
		code >>= 3;
		flush_buffer(PMBtab0[code].len);
		code = PMBtab0[code].val;
	} else if (code) {
		flush_buffer(PMBtab1[code].len);
		code = PMBtab1[code].val;
	} else {
		SetError(kErrorBadValue);
	}
	return code;
}


inline int MPEG2Decoder::get_B_mb_flags()
{
	int code = show_bits(6);
	if (code >= 8) {
		code >>= 2;
		flush_buffer(BMBtab0[code].len);
		code = BMBtab0[code].val;
	} else if (code) {
		flush_buffer(BMBtab1[code].len);
		code = BMBtab1[code].val;
	} else {
		SetError(kErrorBadValue);
	}
	return code;
}


inline int MPEG2Decoder::get_D_mb_flags()
{
	if (get_bits(1)) {
		flush_buffer(1);
	} else {
		SetError(kErrorBadValue);
	}
	return MBF_INTRA;
}


inline int MPEG2Decoder::get_mb_flags()
{
	if (frame_type == MPEG_FRAME_TYPE_I) return get_I_mb_flags();
	if (frame_type == MPEG_FRAME_TYPE_P) return get_P_mb_flags();
	if (frame_type == MPEG_FRAME_TYPE_B) return get_B_mb_flags();
	if (frame_type == MPEG_FRAME_TYPE_D) return get_D_mb_flags();

	SetError(kErrorBadFrameType);
	return 0;
}


inline int MPEG2Decoder::get_mb_cbp()
{
	int code = show_bits(9);
	if (code >= 128) {
		code >>= 4;
		flush_buffer(CBPtab0[code].len);
		code = CBPtab0[code].val;
	} else if (code >= 8) {
		code >>= 1;
		flush_buffer(CBPtab1[code].len);
		code = CBPtab1[code].val;
	} else if (code) {
		flush_buffer(CBPtab2[code].len);
		code = CBPtab2[code].val;
	} else {
		SetError(kErrorBadValue);
	}
	return code;
}


// calculate motion vector component
void MPEG2Decoder::decode_motion_vector(int& pred,
	int r_size, int code, int residual, int full_pel)
{
	const int lim = 16 << r_size;
	int vec = full_pel? pred >> 1: pred;

	if (code > 0)
	{
		vec += (( code - 1) << r_size) + residual + 1;
		if (vec >= lim) vec -= (lim + lim);
	}
	else if (code < 0)
	{
		vec -= ((-code - 1) << r_size) + residual + 1;
		if (vec < -lim) vec += (lim + lim);
	}

	pred = full_pel? vec << 1: vec;
}


void MPEG2Decoder::motion_vector(int *pmv,
	int h_size, int v_size, int full_pel)
{
	int code, residual;

	// horizontal component (maximum h_size is 14 bits)
	code = mpeg_get_motion_component();
	residual = (h_size > 0 && code != 0)? get_bits(h_size): 0;

	decode_motion_vector(pmv[0], h_size, code, residual, full_pel);

	if (motion_type == MC_DMV)
	{
		// cf. ISO/IEC 13818-2, Annex B, Table B-11
		if (dmvector[0] = get_bits(1))
		{
			if (get_bits(1)) dmvector[0] = -1;
		}
	}

	// vertical component (maximum v_size is 14 bits)
	code = mpeg_get_motion_component();
	residual = (v_size > 0 && code != 0)? get_bits(v_size): 0;

	if (mvscale) pmv[1] >>= 1;
	decode_motion_vector(pmv[1], v_size, code, residual, full_pel);
	if (mvscale) pmv[1] <<= 1;

	if (motion_type == MC_DMV)
	{
		// cf. ISO/IEC 13818-2, Annex B, Table B-11
		if (dmvector[1] = get_bits(1))
		{
			if (get_bits(1)) dmvector[1] = -1;
		}
	}
}


void MPEG2Decoder::motion_vectors(int count, int s, int h_size, int v_size)
{
	if (count == 1)
	{
		if (mv_format == MV_FIELD && motion_type != MC_DMV)
		{
			mvfield_select[1][s] = mvfield_select[0][s] = get_bits(1);
		}
		motion_vector(PMV[0][s], h_size, v_size, 0);

		PMV[1][s][0] = PMV[0][s][0];
		PMV[1][s][1] = PMV[0][s][1];
	}
	else
	{
		// motion_vector_count == 2
		mvfield_select[0][s] = get_bits(1);
		motion_vector(PMV[0][s], h_size, v_size, 0);

		mvfield_select[1][s] = get_bits(1);
		motion_vector(PMV[1][s], h_size, v_size, 0);
	}
}


inline void MPEG2Decoder::decode_mb(int MBA)
{
	int motion_vector_count;
	int pos_x, pos_y;
	int comp, cbp;

	macro_block_flags = get_mb_flags();
	if (mErrorState) return;

	// get frame/field motion type
	if (macro_block_flags & (MBF_FORWARD | MBF_BACKWARD))
	{
		if (picture_structure == FRAME_PICTURE && frame_pred_frame_dct)
		{
			motion_type = MC_FRAME;
		}
		else
		{
			motion_type = get_bits(2);
		}
	}
	else if ((macro_block_flags & MBF_INTRA) && concealment_vectors)
	{
		motion_type = (picture_structure == FRAME_PICTURE)? MC_FRAME: MC_FIELD;
	}
	else
	{
		motion_type = 0;    // implied
	}

	// derive motion_vector_count, mv_format
	if (picture_structure == FRAME_PICTURE)
	{
		motion_vector_count = (motion_type == MC_FIELD)? 2: 1;
		mv_format = (motion_type == MC_FRAME)? MV_FRAME: MV_FIELD;
	}
	else
	{
		motion_vector_count = (motion_type == MC_16X8)? 2: 1;
		mv_format = MV_FIELD;
	}

	mvscale = (mv_format == MV_FIELD && picture_structure == FRAME_PICTURE);

	// get dct_type (frame DCT / field DCT)
	if (picture_structure == FRAME_PICTURE
	    && !frame_pred_frame_dct
	    && (macro_block_flags & (MBF_PATTERN | MBF_INTRA)))
	{
		dct_type = get_bits(1);
	} else {
		dct_type = 0;
	}

	if (macro_block_flags & MBF_QUANT)
	{
		quantizer_scale = get_bits(5);
		if (MPEG2_Flag)
		{
			if (q_scale_type) {
				quantizer_scale = non_linear[quantizer_scale];
			} else {
				quantizer_scale <<= 1;
			}
		}
	}

	// decode forward motion vectors
	if ((macro_block_flags & MBF_FORWARD) ||
		((macro_block_flags & MBF_INTRA) && concealment_vectors))
	{
		if (MPEG2_Flag) {
			motion_vectors(motion_vector_count, 0, f_code[0][0], f_code[0][1]);
		} else {
			motion_vector(PMV[0][0], f_code[0][0], f_code[0][0], forw_vector_full_pel);
		}
		if (mErrorState) return;
	}

	// decode backward motion vectors
	if (macro_block_flags & MBF_BACKWARD)
	{
		if (MPEG2_Flag) {
			motion_vectors(motion_vector_count, 1, f_code[1][0], f_code[1][1]);
		} else {
			motion_vector(PMV[0][1], f_code[1][0], f_code[1][0], back_vector_full_pel);
		}
		if (mErrorState) return;
	}

	if ((macro_block_flags & MBF_INTRA) && concealment_vectors)
	{
		flush_buffer(1);	// remove marker_bit
	}

	if (macro_block_flags & MBF_PATTERN)
	{
		cbp = get_mb_cbp();
		if (mErrorState) return;

		if (chroma_format == CHROMA422) {
			cbp = (cbp << 2) + get_bits(2);
		} else if (chroma_format == CHROMA444) {
			cbp = (cbp << 6) + get_bits(6);
		}
	}
	else
	{
		cbp = (macro_block_flags & MBF_INTRA)?
			(1 << block_count) - 1: 0;
	}

	// decode blocks
	memset(dct_coeff, 0, sizeof(short) * 64 * block_count);

	cbp <<= (32 - block_count);
	for (comp = 0; comp < block_count; ++comp)
	{
		if (cbp & 0x80000000)
		{
			if (macro_block_flags & MBF_INTRA)
			{
				if (MPEG2_Flag)
					Decode_MPEG2_Intra_Block(comp);
				else
					Decode_MPEG1_Intra_Block(comp);
			}
			else
			{
				if (MPEG2_Flag)
					Decode_MPEG2_Non_Intra_Block(comp);
				else
					Decode_MPEG1_Non_Intra_Block(comp);
			}
			if (mErrorState) return;

			// IDCT
			if (cpuflags & CPUFLAG_ISSE)
				isse_IDCT(dct_coeff[comp]);
			else if (cpuflags & CPUFLAG_MMX)
				mmx_IDCT (dct_coeff[comp]);
			else
				Fast_IDCT(dct_coeff[comp]);
		}

		cbp <<= 1;
	}

	if (frame_type == MPEG_FRAME_TYPE_D) flush_buffer(1);

	// reset intra_dc predictors
	if (!(macro_block_flags & MBF_INTRA)) {
		dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = 0;
	} else if (!concealment_vectors) {
		PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
		PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;
	}

	// special "No_MC" macroblock_type case
	if (frame_type == MPEG_FRAME_TYPE_P
		&& !(macro_block_flags & (MBF_FORWARD | MBF_INTRA)))
	{
		PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;

		if (picture_structure==FRAME_PICTURE) {
			motion_type = MC_FRAME;
		} else {
			motion_type = MC_FIELD;
			mvfield_select[0][0] = (picture_structure == BOTTOM_FIELD);
		}
	}

	// derive current position
	pos_x = 16 * (MBA % mb_width);
	pos_y = 16 * (MBA / mb_width);

	cbp = (macro_block_flags & MBF_INTRA)? 0: 1;

	// motion compensation
	if (cbp) form_predictions(pos_x, pos_y);

	// copy or add block data into picture
	for (comp = 0; comp < block_count; ++comp)
	{
		Add_Block(comp, pos_x, pos_y, cbp);
	}
}


inline void MPEG2Decoder::skipped_mb(int MBA)
{
	int pos_x, pos_y;
	int comp;

	memset(dct_coeff, 0, sizeof(short)*64*block_count);
	// reset intra_dc predictors
	dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = 0;
	// reset motion vector predictors
	if (frame_type==MPEG_FRAME_TYPE_P)
		PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;

	// derive motion_type
	if (picture_structure==FRAME_PICTURE)
		motion_type = MC_FRAME;
	else {
		motion_type = MC_FIELD;
		// predict from field of same parity
		mvfield_select[0][0]=mvfield_select[0][1] =
			(picture_structure==BOTTOM_FIELD);
	}

	// skipped macroblock in I-frame is not supported
	if (frame_type == MPEG_FRAME_TYPE_I)
	{
		SetError(kErrorSkippedMBInIFrame);
		return;
	}

	// derive current position
	pos_x = 16 * (MBA % mb_width);
	pos_y = 16 * (MBA / mb_width);

	// motion compensation
	form_predictions(pos_x,pos_y);

	// copy block data into picture
	for (comp = 0; comp < block_count; ++comp)
	{
		Add_Block(comp, pos_x, pos_y, 1);
	}
}


inline void MPEG2Decoder::slice(int pos)
{
	int MBA, MBAInc, MBAMax;
	int vert_ext;

	vert_ext = (MPEG2_Flag && (vertical_size > 2800))? get_bits(3): 0;

	quantizer_scale = get_bits(5);
	if (MPEG2_Flag) {
		if (q_scale_type)
			quantizer_scale = non_linear[quantizer_scale];
		else quantizer_scale <<= 1;
	}

	while(get_bits(1)) flush_buffer(8);

	if (!get_mb_address_inc(MBAInc)) return;

	MBA = ((vert_ext << 7) + (pos & 0xFF) - 1) * mb_width + MBAInc - 1;
	MBAInc = 1;	// first macroblock, not skipped
	MBAMax = mb_width * mb_height;

	dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = 0;
	PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
	PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;

	while (MBA < MBAMax)
	{
		if (!MBAInc)
		{
			// It is expected that the slice will end when
			// nextbits() is '000 0000 0000 0000 0000 0000'.
			// (See ISO/IEC 13818-2 section 6.2.4.)

			if (!get_mb_address_inc(MBAInc)) break;
		}

		if (MBAInc == 1)
			decode_mb(MBA);
		else
			skipped_mb(MBA);

		if (mErrorState) break;
		--MBAInc;
		++MBA;
	}
}


/*************************************************************

	MPEG-1 and MPEG-2 YCbCr to RGB/YUY2/UYVY conversions

*************************************************************/

// Color conversion table (see Table 6-9 of ISO/IEC 13818-2)
// Inverse coefficients are represented as 16:16 fixed point

// Confirmed output range for 8-bit R,G,B is -289 to 547
// Confirmed output range for 6-bit Green is -43 to 108
// Confirmed output range for 5-bit R,G,B is -37 to 68

#define Y_MULT	76309UL		// (255/219)*65536

static const int Inverse_Table_6_9[8][7] = {
//	crv,     cbu,     cgu,    cgv,    radd,     gadd,     badd
	{117504, 138453, -13954, -34903, -16228689, 5065481, -18910197},	// no sequence_display_extension
	{117504, 138453, -13954, -34903, -16228689, 5065481, -18910197},	// ITU-R Rec. 709 (1990)
	{104597, 132201, -25675, -53279, -14576620, 8917830, -18109946},	// unspecified
	{104597, 132201, -25675, -53279, -14576620, 8917830, -18109946},	// reserved
	{104448, 132798, -24759, -53109, -14557520, 8778945, -18186343},	// FCC
	{104597, 132201, -25675, -53279, -14576620, 8917830, -18109946},	// ITU-R Rec. 624-4 System B, G
	{104597, 132201, -25675, -53279, -14576620, 8917830, -18109946},	// SMPTE 170M
	{117579, 136230, -16907, -35559, -16238238, 5527474, -18625621}		// SMPTE 240M (1987)
};

// Confirmed range is -289 to 547, myclipper uses offset -290
static const unsigned char myclipper[840] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,
	14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
	30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,
	46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,
	62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,
	78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,
	94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,
	110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,
	126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,
	142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,
	158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,
	174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,
	190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,
	206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,
	222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,
	238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,
	254,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255
};

// Another little clip table for 15-bit conversion
// Confirmed range is -37 to 68, myclipper2 uses offset -38
static const unsigned char myclipper2[108] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,
	10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
	26,27,28,29,30,31,31,31,31,31,31,31,31,31,31,31,
	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,
	31,31,31,31,31,31,31,31,31,31,31,31
};

// Another little clip table for 6-bit green
// Confirmed range is -43 to 108, myclipper3 uses offset -44
static const unsigned char myclipper3[156] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,
	4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
	20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
	36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
	52,53,54,55,56,57,58,59,60,61,62,63,63,63,63,63,
	63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,
	63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,
	63,63,63,63,63,63,63,63,63,63,63,63
};


void MPEG2Decoder::EnableMatrixCoefficients(bool bEnable)
{
	fAllowMatrixCoefficients = bEnable;
}


void MPEG2Decoder::EnableAcceleration(int level)
{
	cpuflags = 0;
	if (level & eAccel_MMX ) cpuflags |= CPUFLAG_MMX;
	if (level & eAccel_ISSE) cpuflags |= CPUFLAG_ISSE;
}


void MPEG2Decoder::conv420to422(
	const YCCSample*    src,            // source chroma plane
	unsigned char*      dst,            // destination buffer
	bool                progressive)    // progressive flag
{
	// Upsample one 4:2:0 chroma plane to 4:2:2.
	// (See ISO/IEC 13818-2 section 6.1.2.1)

	// The caller is responsible for ensuring that:
	// 1) src and dst are valid pointers
	// 2) the src buffer is at least ((cpw / 2) * (cph / 2)) bytes
	// 3) the dst buffer is at least ((cpw / 2) * cph) bytes
	// 4) emms is executed after this function returns

	// If MMX or ISSE is supported, use blazing fast MMX converter.

	if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE))
	{
		mmx_conv420to422(src, dst, coded_picture_width, coded_picture_height, progressive);
		return;
	}

	// Otherwise, use slow scalar versions.
	// (For the sake of clarity I have not optimized these.)

	if (progressive)
	{
		// MPEG-1 or MPEG-2 progressive frame.

		//  +-------+-------+        +-------+-------+
		//  | x   x | x   x |        | x o x | x o x |
		//  |   o   |   o   |        +-------+-------+
		//  | x   x | x   x |        | x o x | x o x |
		//  +-------+-------+  -->   +-------+-------+
		//  | x   x | x   x |        | x o x | x o x |
		//  |   o   |   o   |        +-------+-------+
		//  | x   x | x   x |        | x o x | x o x |
		//  +-------+-------+        +-------+-------+

		// Chroma plane height minus one
		const int cph = (coded_picture_height >> 1) - 1;

		// Chroma plane width
		const int cpw = coded_picture_width >> 1;

		for (int y = 0; y <= cph; ++y)
		{
			// Pointers to the previous and next lines
			const YCCSample *sm1 = (y >   0)? src - cpw: src;
			const YCCSample *sp1 = (y < cph)? src + cpw: src;

			// Pointer to the next destination line
			unsigned char *dp1 = dst + cpw;

			for (int x = cpw; x > 0; --x)
			{
				const unsigned int pm1 = *sm1++;
				const unsigned int pp0 = *src++;
				const unsigned int pp1 = *sp1++;

				*dst++ = (unsigned char)((1 * pm1 + 3 * pp0 + 2) >> 2);
				*dp1++ = (unsigned char)((3 * pp0 + 1 * pp1 + 2) >> 2);
			}
			
			dst += cpw;
		}
	}
	else
	{

// MPEG-2 interlaced frame.  I've separated the fields
// and centered the chroma horizontally for better clarity.
//
//      top field      bottom field               top field      bottom field
//
//  +-------+-------+                         +-------+-------+
//  | x   x | x   x	|                         | x o x | x o x |
//  |   o   |   o   |       |       |         |       |       |       |       |
//                  | x   x | x   x |                         | x o x | x o x |
//  +-------+-------+-------+-------+         +-------+-------+-------+-------+
//  | x   x | x   x	|                         | x o x | x o x |
//  |       |       |   o   |   o   |         |       |       |
//                  | x   x | x   x |                         | x o x | x o x |
//  +-------+-------+-------+-------+   -->   +-------+-------+-------+-------+
//  | x   x | x   x	|                         | x o x | x o x |
//  |   o   |   o   |       |       |         |       |       |       |       |
//                  | x   x | x   x |                         | x o x | x o x |
//  +-------+-------+-------+-------+         +-------+-------+-------+-------+
//  | x   x | x   x	|                         | x o x | x o x |
//  |       |       |   o   |   o   |         |       |       |       |       |
//                  | x   x | x   x |                         | x o x | x o x |
//                  +-------+-------+                         +-------+-------+

		// Half chroma plane height minus one
		const int cph = (coded_picture_height >> 2) - 1;

		// Field pitch is twice chroma plane width
		const int cpw = coded_picture_width;

		int x;

		for (int y = 0; y <= cph; ++y)
		{
			// Pointers to the previous and next fields
			const YCCSample *sm1 = (y >   0)? src - cpw: src;
			const YCCSample *sp1 = (y < cph)? src + cpw: src;

			// Pointer to the next destination line
			unsigned char *dp1 = dst + cpw;

			// Top field
			for (x = cpw >> 1; x > 0; --x)
			{
				const unsigned int pm1 = *sm1++;
				const unsigned int pp0 = *src++;
				const unsigned int pp1 = *sp1++;

				*dst++ = (unsigned char)((1 * pm1 + 7 * pp0 + 4) >> 3);
				*dp1++ = (unsigned char)((5 * pp0 + 3 * pp1 + 4) >> 3);
			}

			// Bottom field
			for (x = cpw >> 1; x > 0; --x)
			{
				const unsigned int pm1 = *sm1++;
				const unsigned int pp0 = *src++;
				const unsigned int pp1 = *sp1++;

				*dst++ = (unsigned char)((3 * pm1 + 5 * pp0 + 4) >> 3);
				*dp1++ = (unsigned char)((7 * pp0 + 1 * pp1 + 4) >> 3);
			}

			dst += cpw;
		}
	}
}


void MPEG2Decoder::conv422to444(
	const YCCSample*    src,        // source chroma plane
	unsigned char*      dst,        // destination buffer
	bool                mpeg2)      // MPEG-2 flag
{
	// Upsample one 4:2:2 chroma plane to 4:4:4.
	// (See ISO/IEC 13818-2 section 6.1.2.2)

	// The caller is responsible for ensuring that:
	// 1) src and dst are valid pointers
	// 2) the src buffer is at least ((cpw / 2) * cph) bytes
	// 3) the dst buffer is at least (cpw * cph) bytes
	// 4) emms is executed after this function returns

	// If MMX or ISSE is supported, use blazing fast MMX converter.

	if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE))
	{
		mmx_conv422to444(src, dst, coded_picture_width, coded_picture_height, MPEG2_Flag != 0);
		return;
	}

	// Otherwise, use slow scalar versions.
	// (For the sake of clarity I have not optimized these.)

	if (!mpeg2)
	{
		// MPEG-1 chroma positioning (Y is not shown):
		//
		//  +-------+-------+        +---+---+---+---+
		//  |   o   |   o   |        | o | o | o | o |
		//  +-------+-------+  -->   +---+---+---+---+
		//  |   o   |   o   |        | o | o | o | o |
		//  +-------+-------+        +---+---+---+---+

		for (int y = 0; y < coded_picture_height; ++y)
		{
			unsigned int pm1;
			unsigned int pp0 = *src++;
			unsigned int pp1 = pp0;
			
			// Do all samples on this row except the last one
			for (int x = (coded_picture_width >> 1) - 1; x > 0; --x)
			{
				pm1 = pp0;
				pp0 = pp1;
				pp1 = *src++;

				*dst++ = (unsigned char)((1 * pm1 + 3 * pp0 + 2) >> 2);
				*dst++ = (unsigned char)((3 * pp0 + 1 * pp1 + 2) >> 2);
			}

			// Now do the last sample on this row
			*dst++ = (unsigned char)((1 * pp0 + 3 * pp1 + 2) >> 2);
			*dst++ = (unsigned char)pp1;
		}
	}
	else
	{
		// MPEG-2 chroma positioning (Y is not shown):
		//
		//  +-------+-------+        +---+---+---+---+
		//  | o     | o     |        | o | o | o | o |
		//  +-------+-------+  -->   +---+---+---+---+
		//  | o     | o     |        | o | o | o | o |
		//  +-------+-------+        +---+---+---+---+

		for (int y = 0; y < coded_picture_height; ++y)
		{
			unsigned int pp0;
			unsigned int pp1 = *src++;

			// Do all samples on this row except the last one
			for (int x = (coded_picture_width >> 1) - 1; x > 0; --x)
			{
				pp0 = pp1;
				pp1 = *src++;

				*dst++ = (unsigned char)pp0;
				*dst++ = (unsigned char)((pp0 + pp1 + 1) >> 1);
			}

			// Now do the last sample on this row
			*dst++ = (unsigned char)pp1;
			*dst++ = (unsigned char)pp1;
		}
	}
}


// Convert YCbCr 4:4:4 to RGB 16,24,32
// (See ISO/IEC 13818-2 section 6.3.6)

inline void MPEG2Decoder::conv444toRGB(
	const YCCSample *srcY,      // source Y plane
	const YCCSample *srcU,		// source Cb plane
	const YCCSample *srcV,		// source Cr plane
	void* dst,                  // destination buffer
	int   dst_width,            // width of destination
	int   dst_height,           // height of destination
	int   dst_pitch,            // pitch of destination
	int   bpp)                  // RGB bits per pixel
{
	// dst_pitch is the pitch, and it can be negative!
	// If so, _dst points at the END of the bitmap

    const int crv  = Inverse_Table_6_9[matrix_coefficients & 7][0];
    const int cbu  = Inverse_Table_6_9[matrix_coefficients & 7][1];
    const int cgu  = Inverse_Table_6_9[matrix_coefficients & 7][2];
    const int cgv  = Inverse_Table_6_9[matrix_coefficients & 7][3];
    const int radd = Inverse_Table_6_9[matrix_coefficients & 7][4];
    const int gadd = Inverse_Table_6_9[matrix_coefficients & 7][5];
    const int badd = Inverse_Table_6_9[matrix_coefficients & 7][6];

    const unsigned char *clipper  = myclipper  + 290;
	const unsigned char *clipper2 = myclipper2 + 38;
	const unsigned char *clipper3 = myclipper3 + 44;

	int ht, x;

	const int wmod = coded_picture_width - dst_width;

	switch (bpp) {

	case 15:
		for (ht = 0; ht < dst_height; ++ht)
		{
			unsigned short *dp0 = (unsigned short *)
				((unsigned char *)dst + ht * dst_pitch);

			for (x = 0; x < dst_width; ++x)
			{
				const int y = *srcY++ * Y_MULT;
				const int u = *srcU++;
				const int v = *srcV++;

				unsigned short i = clipper2[(y + v * crv + radd) >> 19];
				i = (i << 5) + clipper2[(y + u * cgu + v * cgv + gadd) >> 19];
				*dp0++ = (i << 5) + clipper2[(y + u * cbu + badd) >> 19];
			}

			srcY += wmod;
			srcU += wmod;
			srcV += wmod;
		}
		break;

	case 16:
		for (ht = 0; ht < dst_height; ++ht)
		{
			unsigned short *dp0 = (unsigned short *)
				((unsigned char *)dst + ht * dst_pitch);

			for (x = 0; x < dst_width; ++x)
			{
				const int y = *srcY++ * Y_MULT;
				const int u = *srcU++;
				const int v = *srcV++;

				unsigned short i = clipper2[(y + v * crv + radd) >> 19];
				i = (i << 6) + clipper3[(y + u * cgu + v * cgv + gadd) >> 18];
				*dp0++ = (i << 5) + clipper2[(y + u * cbu + badd) >> 19];
			}

			srcY += wmod;
			srcU += wmod;
			srcV += wmod;
		}
		break;

	case 24:
		for (ht = 0; ht < dst_height; ++ht)
		{
			unsigned char *dp0 = (unsigned char *)dst + ht * dst_pitch;

			for (x = 0; x < dst_width; ++x)
			{
				const int y = *srcY++ * Y_MULT;
				const int u = *srcU++;
				const int v = *srcV++;

				*dp0++ = clipper[(y + u * cbu + badd) >> 16];
				*dp0++ = clipper[(y + u * cgu + v * cgv + gadd) >> 16];
				*dp0++ = clipper[(y + v * crv + radd) >> 16];
			}

			srcY += wmod;
			srcU += wmod;
			srcV += wmod;
		}
		break;

	case 32:
		for (ht = 0; ht < dst_height; ++ht)
		{
			unsigned int *dp0 = (unsigned int *)
				((unsigned char *)dst + ht * dst_pitch);

			for (x = 0; x < dst_width; ++x)
			{
				const int y = *srcY++ * Y_MULT;
				const int u = *srcU++;
				const int v = *srcV++;

				unsigned int i = clipper[(y + v * crv + radd) >> 16];
				i = (i << 8) + clipper[(y + u * cgu + v * cgv + gadd) >> 16];
				*dp0++ = (i << 8) + clipper[(y + u * cbu + badd) >> 16];
			}

			srcY += wmod;
			srcU += wmod;
			srcV += wmod;
		}
		break;
	}
}


/***************************************************************************
 *
 *  VirtualDub interface
 *
 ***************************************************************************/


bool MPEG2Decoder::ConvertToRGB(const VDXPixmap& pm, int buffer, int bpp)
{
	// Convert YCbCr to RGB 15,16,24,32

	switch (chroma_format) {

	case CHROMA420:
	{
		// Progressive or Interlaced?
		const bool pf = (buffers[buffer].bitflags & PICTUREFLAG_PF)? true: false;

		conv420to422(buffers[buffer].U, uv422, pf);
		conv422to444(uv422, u444, MPEG2_Flag != 0);
		conv420to422(buffers[buffer].V, uv422, pf);
		conv422to444(uv422, v444, MPEG2_Flag != 0);

		if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE)) { my_emms }

		conv444toRGB(buffers[buffer].Y, u444, v444, pm.data, pm.w, pm.h, (int)pm.pitch, bpp);
		return true;
	}

	case CHROMA422:
		conv422to444(buffers[buffer].U, u444, MPEG2_Flag != 0);
		conv422to444(buffers[buffer].V, v444, MPEG2_Flag != 0);
		if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE)) { my_emms }

		conv444toRGB(buffers[buffer].Y, u444, v444, pm.data, pm.w, pm.h, (int)pm.pitch, bpp);
		return true;

	case CHROMA444:
		conv444toRGB(buffers[buffer].Y, buffers[buffer].U, buffers[buffer].V, pm.data, pm.w, pm.h, (int)pm.pitch, bpp);
		return true;
	}

	return false;
}


void MPEG2Decoder::Blit(
	const void* src,            // source buffer
	int         src_width,      // width of source
	int         src_height,     // height of source
	int         src_pitch,      // pitch of source
	void*       dst,            // destination buffer
	int         dst_width,      // width of destination
	int         dst_height,     // height of destination
	int         dst_pitch)      // pitch of destination
{
	// Caller's responsibilities:
	// 1) src must not be NULL
	// 2) dst must not be NULL

	// Clamp
	if (dst_width  > src_width ) dst_width  = src_width;
	if (dst_height > src_height) dst_height = src_height;

	if (dst_width > 0)
	{
		while (dst_height > 0)
		{
			memcpy(dst, src, dst_width);
			src = (char *)src + src_pitch;
			dst = (char *)dst + dst_pitch;
			--dst_height;
		}
	}
}


void MPEG2Decoder::HalfY(
	const void* src,            // source buffer
	int         src_width,      // width of source
	int         src_height,     // height of source
	int         src_pitch,      // pitch of source
	void*       dst,            // destination buffer
	int         dst_width,      // width of destination
	int         dst_height,     // height of destination
	int         dst_pitch,      // pitch of destination
	bool        progressive)    // progressive frame flag
{
	// Halve the height of a Cb/Cr component

	// Caller's responsibilities:
	// 1) src must not be NULL
	// 2) dst must not be NULL

	// Clamp
	if (dst_width > src_width) dst_width = src_width;
	if ((dst_height * 2) > src_height) dst_height = src_height >> 1;

	if (progressive)
	{
		// Progressive
		while (dst_height > 0)
		{
			const YCCSample *sp0 = (YCCSample *)src;
			const YCCSample *sp1 = sp0 + src_pitch;

			unsigned char *dp0 = (unsigned char *)dst;

			for (int x = dst_width; x > 0; --x)
			{
				const unsigned int pp0 = *sp0++;
				const unsigned int pp1 = *sp1++;
				*dp0++ = (unsigned char)((pp0 + pp1 + 1) >> 1);
			}

			src = (char *)src + src_pitch * 2;
			dst = (char *)dst + dst_pitch;
			--dst_height;
		}
	}
	else
	{
		// Interlaced
		int x;

		while (dst_height > 0)
		{
			// Top field
			const YCCSample *sp0 = (YCCSample *)src;
			const YCCSample *sp1 = sp0 + src_pitch * 2;
			unsigned char *dp0 = (unsigned char *)dst;

			for (x = dst_width; x > 0; --x)
			{
				const unsigned int pp0 = *sp0++;
				const unsigned int pp1 = *sp1++;
				*dp0++ = (unsigned char)((3 * pp0 + 1 * pp1 + 2) >> 2);
			}

			if (--dst_height <= 0) break;
			src = (char *)src + src_pitch;
			dst = (char *)dst + dst_pitch;

			// Bottom field
			sp0 = (YCCSample *)src;
			sp1 = sp0 + src_pitch * 2;
			dp0 = (unsigned char *)dst;

			for (x = dst_width; x > 0; --x)
			{
				const unsigned int pp0 = *sp0++;
				const unsigned int pp1 = *sp1++;
				*dp0++ = (unsigned char)((1 * pp0 + 3 * pp1 + 2) >> 2);
			}

			src = (char *)src + src_pitch * 3;
			dst = (char *)dst + dst_pitch;
			--dst_height;
		}
	}
}


inline bool MPEG2Decoder::ConvertToY8(const VDXPixmap &pm, int buffer)
{
	Blit(buffers[buffer].Y, coded_picture_width,
		coded_picture_height, coded_picture_width,
		pm.data, pm.w, pm.h, (int)pm.pitch
	);
	return true;
}


bool MPEG2Decoder::ConvertTo420(const VDXPixmap& pm, int buffer, bool interlace)
{
	// Convert to 4:2:0 planar, progressive or interlaced.
	// If tff is nonzero, also look at the field order.

	// Y plane
	Blit(buffers[buffer].Y, coded_picture_width,
		coded_picture_height, coded_picture_width,
		pm.data, pm.w, pm.h, (int)pm.pitch);

	if (chroma_format == CHROMA420)
	{
		// TODO: If interlace is false and progressive_frame
		// is false, we should de-interlace the chroma here.

		// TODO: If interlace is true and progressive frame
		// is false and tff is nonzero, then we also need
		// to match the field order.

		// Chroma coded picture width, height
		const int cpw = coded_picture_width  >> 1;
		const int cph = coded_picture_height >> 1;

		// Cb plane
		Blit(buffers[buffer].U, cpw, cph, cpw, pm.data2, pm.w >> 1, pm.h >> 1, (int)pm.pitch2);

		// Cr plane
		Blit(buffers[buffer].V, cpw, cph, cpw, pm.data3, pm.w >> 1, pm.h >> 1, (int)pm.pitch3);

		return true;
	}

	if (chroma_format == CHROMA422)
	{
		// Vertically downsample the chroma planes.
		// This code is pretty much untested.

		// If interlace is true AND this frame is interlaced,
		// then we preserve the interlacing when downsampling.
		// Otherwise we just do a 50/50 blend of the fields.

		bool progressive = (interlace)?
			((buffers[buffer].bitflags & PICTUREFLAG_PF) != 0): true;

		// Chroma coded picture width
		const int cpw = coded_picture_width  >> 1;

		// Cb plane
		HalfY(buffers[buffer].U, cpw, coded_picture_height, cpw, pm.data2, pm.w, pm.h, (int)pm.pitch2, progressive);

		// Cr plane
		HalfY(buffers[buffer].V, cpw, coded_picture_height, cpw, pm.data3, pm.w, pm.h, (int)pm.pitch3, progressive);

		return true;
	}

	// TODO: CHROMA444

	return false;
}


inline bool MPEG2Decoder::ConvertToYUY2(const VDXPixmap &pm, int buffer)
{
	const YCCSample *Yptr;
	const YCCSample *Uptr;
	const YCCSample *Vptr;

	// Progressive or Interlaced?
	const bool pf = !!(buffers[buffer].bitflags & PICTUREFLAG_PF);

	switch (chroma_format) {

	case CHROMA420:
		conv420to422(buffers[buffer].U, u444, pf);
		conv420to422(buffers[buffer].V, v444, pf);
		if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE))
		{
			if (pm.w == coded_picture_width)
			{
				// This is only safe if width is a multiple of 16!
				mmx_conv422toYUY2(
					buffers[buffer].Y,
					u444,
					v444,
					pm.data,
					pm.w >> 1,
					pm.h
				);
				return true;
			}
			my_emms
		}
		Uptr = u444;
		Vptr = v444;
		break;

	case CHROMA422:
		if ((cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE)) && pm.w == coded_picture_width)
		{
			// This is only safe if width is a multiple of 16!
			mmx_conv422toYUY2(
				buffers[buffer].Y,
				buffers[buffer].U,
				buffers[buffer].V,
				pm.data,
				pm.w >> 1,
				pm.h
			);
			return true;
		}
		Uptr = buffers[buffer].U;
		Vptr = buffers[buffer].V;
		break;

	case CHROMA444:
		// TODO
	default:
		return false;
	}

	Yptr = buffers[buffer].Y;

	for (int y = 0; y < pm.h; ++y)
	{
		const YCCSample *yp0 = Yptr;
		const YCCSample *up0 = Uptr;
		const YCCSample *vp0 = Vptr;

		unsigned char *dst = (unsigned char *)pm.data + y * pm.pitch;

		for (int x = pm.w >> 1; x > 0; --x)
		{
			*dst++ = *yp0++;
			*dst++ = *up0++;
			*dst++ = *yp0++;
			*dst++ = *vp0++;
		}

		Yptr += coded_picture_width;
		Uptr += (coded_picture_width >> 1);
		Vptr += (coded_picture_width >> 1);
	}

	return true;
}


inline bool MPEG2Decoder::ConvertToUYVY(const VDXPixmap& pm, int buffer)
{
	const YCCSample *Yptr;
	const YCCSample *Uptr;
	const YCCSample *Vptr;

	// Progressive or Interlaced?
	const bool pf = !!(buffers[buffer].bitflags & PICTUREFLAG_PF);

	switch (chroma_format) {

	case CHROMA420:
		conv420to422(buffers[buffer].U, u444, pf);
		conv420to422(buffers[buffer].V, v444, pf);
		if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE))
		{
			if (pm.w == coded_picture_width)
			{
				// This is only safe if width is a multiple of 16!
				mmx_conv422toUYVY(
					buffers[buffer].Y,
					u444,
					v444,
					pm.data,
					pm.w >> 1,
					pm.h
				);
				return true;
			}
			my_emms
		}
		Uptr = u444;
		Vptr = v444;
		break;

	case CHROMA422:
		if ((cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE)) && pm.w == coded_picture_width)
		{
			// This is only safe if width is a multiple of 16!
			mmx_conv422toUYVY(
				buffers[buffer].Y,
				buffers[buffer].U,
				buffers[buffer].V,
				pm.data,
				pm.w >> 1,
				pm.h
			);
			return true;
		}
		Uptr = buffers[buffer].U;
		Vptr = buffers[buffer].V;
		break;

	case CHROMA444:
		// TODO

	default:
		return false;
	}

	Yptr = buffers[buffer].Y;

	for (int y = 0; y < pm.h; ++y)
	{
		const YCCSample *yp0 = Yptr;
		const YCCSample *up0 = Uptr;
		const YCCSample *vp0 = Vptr;

		unsigned char *dst = (unsigned char *)pm.data + y * pm.pitch;

		for (int x = pm.w >> 1; x > 0; --x)
		{
			*dst++ = *up0++;
			*dst++ = *yp0++;
			*dst++ = *vp0++;
			*dst++ = *yp0++;
		}

		Yptr += coded_picture_width;
		Uptr += (coded_picture_width >> 1);
		Vptr += (coded_picture_width >> 1);
	}

	return true;
}


inline bool MPEG2Decoder::ConvertTo422P(const VDXPixmap& pm, int buffer)
{
	// Convert to 4:2:2 planar

	// Progressive or Interlaced?
	const bool pf = !!(buffers[buffer].bitflags & PICTUREFLAG_PF);

	// Y plane
	Blit(buffers[buffer].Y, coded_picture_width,
		coded_picture_height, coded_picture_width,
		pm.data, pm.w, pm.h, (int)pm.pitch
	);

	switch (chroma_format) {

	case CHROMA420:
		conv420to422(buffers[buffer].U, u444, pf);
		conv420to422(buffers[buffer].V, v444, pf);
		if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE)) { my_emms }
		Blit(u444, coded_picture_width >> 1,
			coded_picture_height, coded_picture_width >> 1,
			pm.data2, pm.w >> 1, pm.h, (int)pm.pitch2
		);
		Blit(v444, coded_picture_width >> 1,
			coded_picture_height, coded_picture_width >> 1,
			pm.data3, pm.w >> 1, pm.h, (int)pm.pitch3
		);
		break;

	case CHROMA422:
		Blit(buffers[buffer].U, coded_picture_width >> 1,
			coded_picture_height, coded_picture_width >> 1,
			pm.data2, pm.w >> 1, pm.h, (int)pm.pitch2
		);
		Blit(buffers[buffer].V, coded_picture_width >> 1,
			coded_picture_height, coded_picture_width >> 1,
			pm.data3, pm.w >> 1, pm.h, (int)pm.pitch3
		);
		break;

	case CHROMA444:
		// TODO

	default:
		return false;
	}

	return true;
}


inline bool MPEG2Decoder::ConvertTo444P(const VDXPixmap& pm, int buffer)
{
	// Convert to 4:4:4 planar

	// Progressive or Interlaced?
	const bool pf = !!(buffers[buffer].bitflags & PICTUREFLAG_PF);

	// Y plane
	Blit(buffers[buffer].Y, coded_picture_width,
		coded_picture_height, coded_picture_width,
		pm.data, pm.w, pm.h, (int)pm.pitch
	);

	switch (chroma_format) {

	case CHROMA420:
		conv420to422(buffers[buffer].U, uv422, pf);
		conv422to444(uv422, u444, MPEG2_Flag != 0);
		conv420to422(buffers[buffer].V, uv422, pf);
		conv422to444(uv422, v444, MPEG2_Flag != 0);
		if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE)) { my_emms }
		Blit(u444, coded_picture_width,
			coded_picture_height, coded_picture_width,
			pm.data2, pm.w, pm.h, (int)pm.pitch2
		);
		Blit(v444, coded_picture_width,
			coded_picture_height, coded_picture_width,
			pm.data3, pm.w, pm.h, (int)pm.pitch3
		);
		break;

	case CHROMA422:
		conv422to444(buffers[buffer].U, u444, MPEG2_Flag != 0);
		conv422to444(buffers[buffer].V, v444, MPEG2_Flag != 0);
		if (cpuflags & (CPUFLAG_MMX | CPUFLAG_ISSE)) { my_emms }
		Blit(u444, coded_picture_width,
			coded_picture_height, coded_picture_width,
			pm.data2, pm.w, pm.h, (int)pm.pitch2
		);
		Blit(v444, coded_picture_width,
			coded_picture_height, coded_picture_width,
			pm.data3, pm.w, pm.h, (int)pm.pitch3
		);
		break;

	case CHROMA444:
		Blit(buffers[buffer].U, coded_picture_width,
			coded_picture_height, coded_picture_width,
			pm.data2, pm.w, pm.h, (int)pm.pitch2
		);
		Blit(buffers[buffer].V, coded_picture_width,
			coded_picture_height, coded_picture_width,
			pm.data3, pm.w, pm.h, (int)pm.pitch3
		);
		break;

	default:
		return false;
	}

	return true;
}


/***************************************************************************
 *
 *      Public
 *
 ***************************************************************************/

const void *MPEG2Decoder::GetYBuffer(int buffer, int& pitch)
{
	if ((unsigned int)buffer < MPEG_BUFFERS)
	{
		pitch = coded_picture_width;
		return buffers[buffer].Y;
	}
	pitch = 0;
	return NULL;
}


const void *MPEG2Decoder::GetCbBuffer(int buffer, int& pitch)
{
	if ((unsigned int)buffer < MPEG_BUFFERS)
	{
		pitch = (chroma_format == CHROMA444)?
			coded_picture_width: coded_picture_width >> 1;
		return buffers[buffer].U;
	}
	pitch = 0;
	return NULL;
}


const void *MPEG2Decoder::GetCrBuffer(int buffer, int& pitch)
{
	if ((unsigned int)buffer < MPEG_BUFFERS)
	{
		pitch = (chroma_format == CHROMA444)?
			coded_picture_width: coded_picture_width >> 1;
		return buffers[buffer].V;
	}
	pitch = 0;
	return NULL;
}


bool MPEG2Decoder::ConvertFrame(const VDXPixmap& pm, int buffer)
{
	using namespace nsVDXPixmap;

	// Do ALL preliminary checks here, to avoid having
	// to do the same checks in the converters.

	// Note: VDXPixmap is always valid, since it is controlled by us.
	
	if (memblock == NULL) goto Abort;
	if ((unsigned int)buffer >= MPEG_BUFFERS) goto Abort;

	switch (pm.format) {

		case kPixFormat_XRGB1555:
			return ConvertToRGB(pm, buffer, 15);

		case kPixFormat_RGB565:
			return ConvertToRGB(pm, buffer, 16);

		case kPixFormat_RGB888:
			return ConvertToRGB(pm, buffer, 24);

		case kPixFormat_XRGB8888:
			return ConvertToRGB(pm, buffer, 32);

		case kPixFormat_Y8:
		case kPixFormat_Y8_FR:
			return ConvertToY8(pm, buffer);

		case kPixFormat_YUV422_YUYV:
		case kPixFormat_YUV422_YUYV_709:
		case kPixFormat_YUV422_YUYV_FR:
		case kPixFormat_YUV422_YUYV_709_FR:
			return ConvertToYUY2(pm, buffer);

		case kPixFormat_YUV422_UYVY:
		case kPixFormat_YUV422_UYVY_709:
		case kPixFormat_YUV422_UYVY_FR:
		case kPixFormat_YUV422_UYVY_709_FR:
			return ConvertToUYVY(pm, buffer);

		case kPixFormat_YUV420_Planar:
		case kPixFormat_YUV420_Planar_709:
		case kPixFormat_YUV420_Planar_FR:
		case kPixFormat_YUV420_Planar_709_FR:
			return ConvertTo420(pm, buffer, false);

		case kPixFormat_YUV422_Planar:
		case kPixFormat_YUV422_Planar_709:
		case kPixFormat_YUV422_Planar_FR:
		case kPixFormat_YUV422_Planar_709_FR:
			return ConvertTo422P(pm, buffer);

		case kPixFormat_YUV444_Planar:
		case kPixFormat_YUV444_Planar_709:
		case kPixFormat_YUV444_Planar_FR:
		case kPixFormat_YUV444_Planar_709_FR:
			return ConvertTo444P(pm, buffer);

		case kPixFormat_YUV410_Planar:
		case kPixFormat_YUV410_Planar_709:
		case kPixFormat_YUV410_Planar_FR:
		case kPixFormat_YUV410_Planar_709_FR:
		case kPixFormat_YUV411_Planar:
		case kPixFormat_YUV411_Planar_709:
		case kPixFormat_YUV411_Planar_FR:
		case kPixFormat_YUV411_Planar_709_FR:
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
			break;  // TODO, interlaced formats

		case kPixFormat_VDXA_RGB:
		case kPixFormat_VDXA_YUV:
		case kPixFormat_Null:
		default:
			break;
	}

Abort:
	return false;
}

