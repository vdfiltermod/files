;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; MMX and ISSE IDCTs, adapted from Xvid source code
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	SHIFT_INV_ROW   EQU 11
	SHIFT_INV_COL   EQU 6

%macro DCT_8_INV_ROW_MMX 4

%define %%src %1
%define %%dst %2
%define %%table %3
%define %%rounder %4

	movq		mm0, [%%src+0]

	movq		mm1, [%%src+8]
	movq		mm2, mm0

	movq		mm3, [%%table+0]
	punpcklwd	mm0, mm1

	movq		mm5, mm0
	punpckldq	mm0, mm0

	movq		mm4, [%%table+8]
	punpckhwd	mm2, mm1

	pmaddwd		mm3, mm0
	movq		mm6, mm2

	movq		mm1, [%%table+32]
	punpckldq	mm2, mm2

	pmaddwd		mm4, mm2
	punpckhdq	mm5, mm5

	pmaddwd		mm0, [%%table+16]
	punpckhdq	mm6, mm6

	movq		mm7, [%%table+40]
	pmaddwd		mm1, mm5

	paddd		mm3, [%%rounder]
	pmaddwd		mm7, mm6

	pmaddwd		mm2, [%%table+24]
	paddd		mm3, mm4

	pmaddwd		mm5, [%%table+48]
	movq		mm4, mm3

	pmaddwd		mm6, [%%table+56]
	paddd		mm1, mm7

	paddd		mm0, [%%rounder]
	psubd		mm3, mm1

; fccHandler: swapped the next two instructions
	paddd		mm1, mm4
	psrad		mm3, SHIFT_INV_ROW

	paddd		mm0, mm2
	psrad		mm1, SHIFT_INV_ROW

	paddd		mm5, mm6
	movq		mm4, mm0

	paddd		mm0, mm5
	psubd		mm4, mm5

	psrad		mm0, SHIFT_INV_ROW
	psrad		mm4, SHIFT_INV_ROW

	packssdw	mm1, mm0
	packssdw	mm4, mm3

	movq		mm7, mm4
	psrld		mm4, 16

	pslld		mm7, 16
	movq		[%%dst], mm1
                        
	por			mm7, mm4
	movq		[%%dst+8], mm7

%endmacro


%macro DCT_8_INV_ROW_XMM 4

%define %%src %1
%define %%dst %2
%define %%table %3
%define %%rounder %4

	movq		mm0, [%%src+0]

	movq		mm1, [%%src+8]
	movq		mm2, mm0

	movq		mm3, [%%table+0]
	pshufw		mm0, mm0, 10001000b

	movq		mm4, [%%table+8]
	movq		mm5, mm1

	pmaddwd		mm3, mm0
	pshufw		mm1, mm1, 10001000b

	movq		mm6, [%%table+32]
	pmaddwd		mm4, mm1

	movq		mm7, [%%table+40]
	pshufw		mm2, mm2, 11011101b

	pmaddwd		mm6, mm2
	pshufw		mm5, mm5, 11011101b

	paddd		mm3, [%%rounder]
	pmaddwd		mm7, mm5

	pmaddwd		mm0, [%%table+16]
	paddd		mm3, mm4

	pmaddwd		mm1, [%%table+24]
	movq		mm4, mm3

	pmaddwd		mm2, [%%table+48]
	paddd		mm6, mm7

	pmaddwd		mm5, [%%table+56]
	paddd		mm3, mm6

	paddd		mm0, [%%rounder]
	psrad		mm3, SHIFT_INV_ROW

	paddd		mm0, mm1
	psubd		mm4, mm6

	movq		mm7, mm0
	paddd		mm2, mm5

	paddd		mm0, mm2
	psrad		mm4, SHIFT_INV_ROW

	psubd		mm7, mm2
	psrad		mm0, SHIFT_INV_ROW

	packssdw	mm3, mm0
	psrad		mm7, SHIFT_INV_ROW

	packssdw	mm7, mm4

	movq		[%%dst+0], mm3
	pshufw		mm7, mm7, 10110001b

	movq		[%%dst+8], mm7

%endmacro


%macro DCT_8_INV_COL 2

%define %%src %1
%define %%dst %2

	movq		mm0, [tg_3_16]

	movq		mm3, [%%src+16*3]
	movq		mm1, mm0

	movq		mm5, [%%src+16*5]
	pmulhw		mm0, mm3

	movq		mm4, [tg_1_16]
	pmulhw		mm1, mm5

	movq		mm7, [%%src+16*7]
	movq		mm2, mm4

	movq		mm6, [%%src+16*1]
	pmulhw		mm4, mm7

	paddsw		mm0, mm3
	pmulhw		mm2, mm6

	paddsw		mm1, mm3
	psubsw		mm0, mm5

	movq		mm3, [ocos_4_16]
	paddsw		mm1, mm5

	paddsw		mm4, mm6
	psubsw		mm2, mm7

	movq		mm5, mm4
	movq		mm6, mm2

	paddsw		mm5, mm1
	psubsw		mm6, mm0

	psubsw		mm4, mm1
	paddsw		mm2, mm0

	movq		mm7, [tg_2_16]
	movq		mm1, mm4

	movq		[%%dst+3*16], mm5
	paddsw		mm1, mm2

	movq		[%%dst+5*16], mm6
	psubsw		mm4, mm2

	movq		mm5, [%%src+2*16]
	movq		mm0, mm7

	movq		mm6, [%%src+6*16]
	pmulhw		mm0, mm5

	movq		mm2, [%%src+0*16]
	pmulhw		mm7, mm6

	psubsw		mm0, mm6
	pmulhw		mm1, mm3

	movq		mm6, [%%src+4*16]
	pmulhw		mm4, mm3

	movq		mm3, mm2
	paddsw		mm7, mm5

	paddsw		mm2, mm6
	psubsw		mm3, mm6

	movq		mm5, mm2
	movq		mm6, mm3

	psubsw		mm2, mm7
	paddsw		mm3, mm0

	paddsw		mm1, mm1
	paddsw		mm4, mm4

	paddsw		mm5, mm7
	psubsw		mm6, mm0

	movq		mm7, mm3
	movq		mm0, mm6

	paddsw		mm3, mm1
	paddsw		mm6, mm4

	psraw		mm3, SHIFT_INV_COL
	psubsw		mm7, mm1

	psraw		mm6, SHIFT_INV_COL
	psubsw		mm0, mm4

	movq		mm1, [%%dst+3*16]
	psraw		mm7, SHIFT_INV_COL

	movq		mm4, mm5
	psraw		mm0, SHIFT_INV_COL

	movq		[%%dst+1*16], mm3
	paddsw		mm5, mm1

	movq		[%%dst+2*16], mm6
	psubsw		mm4, mm1

	movq		mm3, [%%dst+5*16]
	psraw		mm5, SHIFT_INV_COL

	movq		mm6, mm2
	psraw		mm4, SHIFT_INV_COL

	movq		[%%dst+5*16], mm0
	paddsw		mm2, mm3

	movq		[%%dst+6*16], mm7
	psubsw		mm6, mm3

	movq		[%%dst+0*16], mm5
	psraw		mm2, SHIFT_INV_COL

	movq		[%%dst+7*16], mm4
	psraw		mm6, SHIFT_INV_COL

	movq		[%%dst+3*16], mm2

	movq		[%%dst+4*16], mm6

%endmacro

