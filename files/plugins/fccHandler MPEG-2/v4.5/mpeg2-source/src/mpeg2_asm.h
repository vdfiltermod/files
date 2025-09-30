#ifndef f_MPEG_ASM_H
#define f_MPEG_ASM_H

// ASM functions for MPEG-2 Decoder, by fccHandler, January 2012

extern "C" {

	void mmx_IDCT(void *blk);

	void isse_IDCT(void *blk);

	void mmx_Add_Block(
		const void* blk,
		void*       dst,
		int         pitch,
		int         addflag
	);

	void isse_form_component_prediction(
		const void* src,
		void*       dst,
		int         width,
		int         height,
		int         pitch,
		int         deltax,
		int         deltay,
		int         avg
	);

	void mmx_form_component_prediction(
		const void* src,
		void*       dst,
		int         width,
		int         height,
		int         pitch,
		int         deltax,
		int         deltay,
		int         avg
	);

	void mmx_conv420to422(
		const void* src,
		void*       dst,
		int         cpw,
		int         cph,
		bool progressive
	);

	void mmx_conv422to444(
		const void* src,
		void*       dst,
		int         cpw,
		int         cph,
		bool mpeg2
	);

	void mmx_conv422toYUY2(
		const void* scrY,
		const void* srcU,
		const void* srcV,
		void*       dst,
		int         width,
		int         height
	);

	void mmx_conv422toUYVY(
		const void* scrY,
		const void* srcU,
		const void* srcV,
		void*       dst,
		int         width,
		int         height
	);
}

#endif	// f_MPEG_ASM_H
