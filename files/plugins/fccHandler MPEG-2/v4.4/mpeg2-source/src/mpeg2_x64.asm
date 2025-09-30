;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; MPEG-2 Plugin for VirtualDub 1.8.1+
;; Copyright (C) 2007-2012 fccHandler
;; Copyright (C) 1998-2012 Avery Lee
;;
;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program; if not, write to the Free Software
;; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		;; 64-bit MMX and ISSE code for MPEG-2 Decoder ;;
		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Beware, most of these routines omit emms!  The caller must issue it.


;                Number of
; Operation   Functional Units   Latency   Throughput   Execution Pipes
; ---------   ----------------   -------   ----------   ---------------
; ALU                      2        1          1        U and V
; Multiplexer              1        3          1        U or V
; Shift/pack/unpack        1        1          1        U or V
; Memory access            1        1          1        U only
; Integer register access  1        1          1        U only

; * The Arithmetic Logic Unit (ALU) executes arithmetic and logic operations
;   (that is, add, subtract, xor, and).
;
; * The Multiplier unit performs all multiplication operations. Multiplication
;   requires three cycles but can be pipelined, resulting in one multiplication
;   operation every clock cycle. The processor has only one multiplier unit which
;   means that multiplication instructions cannot pair with other multiplication
;   instructions. However, the multiplication instructions can pair with other
;   types of instructions. They can execute in either the U- or V-pipes.
;
; * The Shift unit performs all shift, pack and unpack operations. Only one shifter
;   is available so shift, pack and unpack instructions cannot pair with other shift
;   unit instructions. However, the shift unit instructions can pair with other types
;   of instructions. They can execute in either the U- or V-pipes.
;
; * MMX Technology Instructions that access memory or integer registers can only
;   execute in the U-pipe and cannot be paired with any instructions that are not
;   MMX Technology instructions.
;
; * After updating an MMX Technology register, two clock cycles must pass before that
;   MMX Technology register can be moved to either memory or to an integer register.

%include "idct_tables.inc"


%macro PROC 1
	global %1
	align 16
	%1:
%endmacro


	default rel
	segment .rdata, align=16


	segment .text


; Silly replacement for the missing _mm_empty() intrinsic for x64

PROC amd64_emms
	emms
	ret


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; MMX and ISSE IDCTs, adapted from Xvid source code
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%include "idct_macros.inc"


PROC mmx_IDCT

	DCT_8_INV_ROW_MMX rcx+0*16, rcx+0*16, tab_i_04_mmx, rounder_0
	DCT_8_INV_ROW_MMX rcx+1*16, rcx+1*16, tab_i_17_mmx, rounder_1
	DCT_8_INV_ROW_MMX rcx+2*16, rcx+2*16, tab_i_26_mmx, rounder_2
	DCT_8_INV_ROW_MMX rcx+3*16, rcx+3*16, tab_i_35_mmx, rounder_3
	DCT_8_INV_ROW_MMX rcx+4*16, rcx+4*16, tab_i_04_mmx, rounder_4
	DCT_8_INV_ROW_MMX rcx+5*16, rcx+5*16, tab_i_35_mmx, rounder_5
	DCT_8_INV_ROW_MMX rcx+6*16, rcx+6*16, tab_i_26_mmx, rounder_6
	DCT_8_INV_ROW_MMX rcx+7*16, rcx+7*16, tab_i_17_mmx, rounder_7

	DCT_8_INV_COL rcx+0, rcx+0
	DCT_8_INV_COL rcx+8, rcx+8

	ret    


PROC isse_IDCT

	DCT_8_INV_ROW_XMM rcx+0*16, rcx+0*16, tab_i_04_xmm, rounder_0
	DCT_8_INV_ROW_XMM rcx+1*16, rcx+1*16, tab_i_17_xmm, rounder_1
	DCT_8_INV_ROW_XMM rcx+2*16, rcx+2*16, tab_i_26_xmm, rounder_2
	DCT_8_INV_ROW_XMM rcx+3*16, rcx+3*16, tab_i_35_xmm, rounder_3
	DCT_8_INV_ROW_XMM rcx+4*16, rcx+4*16, tab_i_04_xmm, rounder_4
	DCT_8_INV_ROW_XMM rcx+5*16, rcx+5*16, tab_i_35_xmm, rounder_5
	DCT_8_INV_ROW_XMM rcx+6*16, rcx+6*16, tab_i_26_xmm, rounder_6
	DCT_8_INV_ROW_XMM rcx+7*16, rcx+7*16, tab_i_17_xmm, rounder_7

	DCT_8_INV_COL rcx+0, rcx+0
	DCT_8_INV_COL rcx+8, rcx+8

	ret


    ; mmx_Add_Block(
    ;   rcx     const void*   blk,
    ;   rdx     void*         dst,
    ;   r8d     int           pitch,
	;	r9d     int           addflag
	; );

	; The caller is responsible for ensuring that:
	; 1) blk and dst are valid pointers
	; 2) pitch is greater than or equal to 8
	; 3) the blk buffer is at least 128 bytes
	; 4) the dst buffer is at least (pitch * 8) bytes
	; 5) the MMX state is emptied with emms

PROC mmx_Add_Block

	; "blk" is an array of 64 signed short values
	; "dst" is a series of 8 lines with 8 bytes per line
	; The lines of "dst" are separated by "pitch"

	; Parameters
	%define blk     rcx
	%define dst     rdx
	%define pitch   r8
	%define addflag r9

    and		pitch, 0FFFFFFFh		; destination pitch
	mov		al, 8					; al = loop counter

	test	addflag, 1				; adding blk and dst?
	jz		.noadd					; jump if not adding

	pxor	mm7, mm7				; clear mm7

.yloop1
	movq		mm0, [dst]			; mm0 = dst[76543210]

	movq		mm1, mm0			; mm1 = copy of dst
	punpcklbw	mm0, mm7			; mm0 = dst[+3][+2][+1][+0]

	paddw		mm0, [blk]			; mm0 = dstlo + blklo
	punpckhbw	mm1, mm7			; mm1 = dst[+7][+6][+5][+4]
	
	paddw		mm1, [blk+8]		; mm1 = dsthi + blkhi

	packuswb	mm0, mm1			; dst finished
	add			blk, 16				; advance blk pointer

	dec			al					; decrement loop counter

	movq		[dst], mm0			; store dst

	lea			dst, [dst+pitch]	; advance dst pointer
	jnz			.yloop1				; continue until eax == zero

	ret

.noadd
	movq		mm7, [mmx_0101010101010101]
	psllq		mm7, 7				; mm7 = 8080808080808080h

.yloop2
	movq		mm0, [blk]			; mm0 = blklo
	
	packsswb	mm0, [blk+8]		; mm0 = pack and saturate blklo and blkhi

	paddb		mm0, mm7			; add 128 (convert to unsigned byte)
	add			rcx, 16				; advance blk pointer

	dec			al					; decrement loop counter

	movq		[dst], mm0			; store dst

	lea			dst, [dst+pitch]	; advance dst pointer
	jnz			.yloop2				; continue until eax == zero

	ret
	

    ; mmx_form_component_prediction(
    ;   [rcx]       void *src,      source of prediction
    ;   [rdx]       void *dst,      destination for prediction
    ;   [r8d]       int  width,     width of block (8 or 16)
    ;   [r9d]       int  height,    height of block (4, 8 or 16)
    ;   [rsp+28h]   int  lx,        stride (both src and dst)
    ;   [rsp+30h]   int  dx,        bottom bit indicates half-pel
    ;   [rsp+38h]   int  dy,        bottom bit indicates half-pel
    ;   [rsp+40h]   int  avg        averaging flag (bottom 8 bits)
    ; );

	; If we seek to be re-entrant, we can't rely on
	; a data segment parameter.  Instead, we'll modify
	; the last parameter on the stack to signify ISSE.

PROC isse_form_component_prediction
	or		byte [rsp+40h], 2			; set isse flag
	jmp		mmx_form_component_prediction.asm_form_component_prediction

PROC mmx_form_component_prediction
	and		byte [rsp+40h], 0FDh		; clear isse flag

.asm_form_component_prediction

	; These registers are common to every method
	; Note: width is always either 8 or 16
		
		xor		r10, r10
		and		r8, 0FFFFFFFFh			; r8d = width
		mov		r10d, dword [rsp+28h]	; r10 = pitch
		mov		r11, r10
		sub		r11, r8					; r11 = pitch - width

	; Definitions
		%define src     rcx				; source pointer
		%define dst     rdx				; destination pointer
		%define width   r8d				; src and dst width
		%define height  r9d				; src and dst height
		%define pitch   r10				; src and dst pitch
		%define deltax  byte [rsp+30h]	; X delta (only the low bit is used)
		%define deltay  byte [rsp+38h]	; Y delta (only the low bit is used)
		%define avg     byte [rsp+40h]	; average flag (1) and isse flag (2)
		%define modulus r11				; modulus (pitch - width)

	; Determine which routine handles this block
	
		test	deltax, 1			; (deltax & 1)?
		jz		.nohx
		test	deltay, 1			; (deltay & 1)?
		jz		.nohy
		test	avg, 1				; (avg & 1)?
		jnz		.pred_111			; horz and vert halfpel with avg
		jmp		.pred_110			; horz and vert halfpel, no avg
.nohy
		test	avg, 1				; (avg & 1)?
		jnz		.pred_101			; horz halfpel no vert with avg
		jmp		.pred_100			; horz halfpel no vert, no avg
.nohx
		test	deltay, 1			; (deltay & 1)?
		jz		.nohxy
		test	avg, 1				; (avg & 1)?
		jnz		.pred_011			; vert halfpel no horz with avg
		jmp		.pred_010			; vert halfpel no horz, no avg
.nohxy
		test	avg, 1				; (avg & 1)?
		jnz		.pred_001			; no horz or vert halfpel with avg
		jmp		.pred_000			; no horz or vert halfpel, no avg


; The following are equivalent:
;
; a)  result = (s1 + s2 + 1) >> 1;
; b)  result = (s1 >> 1) + (s2 >> 1) + ((s1 | s2) & 1);
;
; a)  result = (d + ((s1 + s2 + 1) >> 1) + 1) >> 1;
; b)  result = (s1 + s2 + d + d + 3) >> 2;
;
; a)  result = (d + ((s1 + s2 + s3 + s4 + 2) >> 2) + 1) >> 1;
; b)  result = (s1 + s2 + s3 + s4 + d + d + d + d + 6) >> 3;

; Actually, I'm now using the same algorithm Avery used:
; c)  result = (s1 | s2) - ((s1 ^ s2) >> 1);
; Even though it doesn't present as many opportunities
; for pairing, it does shave a few cycles off the loops!


; From Intel's documentation:
;
; "While the pavg instructions operate on two values at a time, it
; is possible to use three pavg instructions to approximate 4-value
; averaging. The line below illustrates this in pseudo-code:
;
;	Y = pavg(pavg(A,B),pavg(C,D)-1)
;
; This value is close to (A + B + C + D + 2)/4 which is the
; typical calculation used to perform subsampling. However, for
; the approximation, 87.5% of values match exactly, and 12.5% of
; the values are off by one least significant bit (LSbit). The
; maximum error is one LSbit. This error is often acceptable for
; performing motion estimation."

; Well, I don't accept it, so there's no pavgb code for
; the 2 cases of horizontal plus vertical half-pel here.
; (Avery's recommendation won't work here either.)
; Ref: see section 7.6.4 of ISO/IEC 13818-2

align 16
.pred_111	; horizontal and vertical half-pel, with averaging

; *dst = (src[x] + src[x+1] + src[x+lx2]
;  + src[x+lx2+1] + (*dst)<<2 + 6) >> 3

		movq		mm6, [mmx_0006000600060006]
		pxor		mm7, mm7

.loop0y		mov		eax, width

.loop0x		movq		mm0, [src]

			movq		mm2, [src+1]
			movq		mm1, mm0

			movq		mm3, mm2
			punpcklbw	mm0, mm7	; mm0 = [rcx] low

			movq		mm4, [src+pitch]
			punpckhbw	mm1, mm7	; mm1 = [rcx] high

			movq		mm5, mm4
			punpcklbw	mm2, mm7	; mm2 = [rcx+1] low

			paddw		mm0, mm2	; mm0 = X low, mm2 free
			punpckhbw	mm3, mm7	; mm3 = [rcx+1] high

			movq		mm2, [src+pitch+1]
			punpcklbw	mm4, mm7	; mm4 = [r10] low

			paddw		mm1, mm3	; mm1 = X high, mm3 free
			punpckhbw	mm5, mm7	; mm5 = [r10] high

			movq		mm3, mm2
			punpcklbw	mm2, mm7	; mm2 = [r10+1] low

			paddw		mm0, mm6	; mm0 = X low adjusted
			punpckhbw	mm3, mm7	; mm3 = [r10+1] high

			paddw		mm2, mm4	; mm2 = Y low, mm4 free
			paddw		mm3, mm5	; mm3 = Y high, mm5 free

			movq		mm4, [dst]
			paddw		mm0, mm2	; mm0 = X+Y low, adjusted

			movq		mm5, mm4
			punpcklbw	mm4, mm7	; mm4 = dst low

			paddw		mm3, mm6	; mm3 = Y high, adjusted
			punpckhbw	mm5, mm7	; mm5 = dst high

			paddw		mm1, mm3	; mm1 = X+Y high, adjusted
			psllw		mm4, 2

			paddw		mm0, mm4
			psllw		mm5, 2

			paddw		mm1, mm5
			psrlw		mm0, 3

			psrlw		mm1, 3
			add			src, 8

			packuswb	mm0, mm1
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.loop0x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.loop0y

		ret

align 16
.pred_110	; horizontal and vertical half-pel, no averaging

; *dst = (src[x] + src[x+1] + src[x+lx2] + src[x+lx2+1] + 2) >> 2

		movq		mm6, [mmx_0002000200020002]
		pxor		mm7, mm7

.loop1y		mov		eax, width

.loop1x		movq		mm0, [src]

			movq		mm2, [src+1]
			movq		mm1, mm0

			movq		mm3, mm2
			punpcklbw	mm0, mm7		; [rcx] low

			paddw		mm0, mm6
			punpckhbw	mm1, mm7		; [rcx] high

			paddw		mm1, mm6
			punpcklbw	mm2, mm7		; [rcx+1] low

			paddw		mm0, mm2
			punpckhbw	mm3, mm7		; [rcx+1] high

			movq		mm2, [src+pitch]
			paddw		mm1, mm3

			movq		mm3, mm2
			punpcklbw	mm2, mm7		; [r10] low

			paddw		mm0, mm2
			punpckhbw	mm3, mm7

			movq		mm2, [src+pitch+1]
			paddw		mm1, mm3

			movq		mm3, mm2
			punpcklbw	mm2, mm7		; [r10+1] low

			paddw		mm0, mm2
			punpckhbw	mm3, mm7		; [r10+1] high

			paddw		mm1, mm3
			psrlw		mm0, 2

			psrlw		mm1, 2
			add			src, 8

			packuswb	mm0, mm1
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.loop1x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.loop1y

		ret

align 16
.pred_101	; horizontal but no vertical half-pel, with averaging

; *dst = ((src[x] + src[x+1] + (*dst) + (*dst) + 3) >> 2

		test	avg, 2			; ISSE allowed?
		jnz		.isseloop2y

		movq	mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop2y	mov			eax, width

.loop2x		movq		mm0, [src]

			movq		mm2, [src+1]
			movq		mm1, mm0

			pxor		mm0, mm2
			por			mm1, mm2

			movq		mm2, [dst]
			psrlq		mm0, 1

			pand		mm0, mm7	; 7F7F7F7F7F7F7F7F

			psubb		mm1, mm0	; mm1 done, mm0 free

			movq		mm0, mm1
			pxor		mm1, mm2

			por			mm0, mm2
			psrlq		mm1, 1

			pand		mm1, mm7	; 7F7F7F7F7F7F7F7F
			add			src, 8

			psubb		mm0, mm1
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.loop2x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.loop2y

		ret

align	16
.isseloop2y
		mov		eax, width

.isseloop2x	movq		mm0, [src]

			pavgb		mm0, [src+1]

			pavgb		mm0, [dst]

			add			src, 8
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.isseloop2x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.isseloop2y

		ret

align 16
.pred_100	; horizontal but no vertical half-pel, no averaging

		test	avg, 2			; ISSE allowed?
		jnz		.isseloop3y

; There are at least 3 ways to do this:
; *dst = ((src[x] + src[x+1] + 1) >> 1;
; *dst = (src[x] >> 1) + (src[x+1] >> 1) + ((src[x] | src[x+1]) & 1);
; *dst = (src[x] | src[x+1]) - ((src[x] ^ src[x+1]) >> 1);

		movq		mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop3y	mov			eax, width

; *dst = (src[x] | src[x+1]) - ((src[x] ^ src[x+1]) >> 1);

.loop3x		movq		mm0, [src]

			movq		mm1, [src+1]
			movq		mm2, mm0

			pxor		mm2, mm1
			por			mm0, mm1

			psrlq		mm2, 1

			pand		mm2, mm7	; 7F7F7F7F7F7F7F7F
			add			src, 8

			psubb		mm0, mm2
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.loop3x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.loop3y

		ret

align	16
.isseloop3y
		mov		eax, width

.isseloop3x	movq		mm0, [src]

			pavgb		mm0, [src+1]

			add			src, 8
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.isseloop3x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.isseloop3y

		ret

align 16
.pred_011	; no horizontal but vertical half-pel, with averaging

		test	avg, 2			; ISSE allowed?
		jnz		.isseloop4y

		movq		mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop4y	mov			eax, width

.loop4x		movq		mm0, [src]

			movq		mm2, [src+pitch]
			movq		mm1, mm0

			pxor		mm0, mm2
			por			mm1, mm2

			movq		mm2, [dst]
			psrlq		mm0, 1

			pand		mm0, mm7	; 7F7F7F7F7F7F7F7F

			psubb		mm1, mm0	; mm1 done, mm0 free

			movq		mm0, mm1
			pxor		mm1, mm2

			por			mm0, mm2
			psrlq		mm1, 1

			pand		mm1, mm7	; 7F7F7F7F7F7F7F7F
			add			src, 8

			psubb		mm0, mm1
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.loop4x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.loop4y

		ret

align	16
.isseloop4y
		mov		eax, width

.isseloop4x	movq		mm0, [src]

			pavgb		mm0, [src+pitch]

			pavgb		mm0, [dst]

			add			src, 8
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.isseloop4x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.isseloop4y

		ret

align 16
.pred_010	; no horizontal but vertical half-pel, no averaging

		test	avg, 2			; ISSE allowed?
		jnz		.isseloop5y

		movq		mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop5y	mov			eax, width

.loop5x		movq		mm0, [src]

			movq		mm1, [src+pitch]
			movq		mm2, mm0

			pxor		mm2, mm1
			por			mm0, mm1

			psrlq		mm2, 1

			pand		mm2, mm7	; 7F7F7F7F7F7F7F7F
			add			src, 8

			psubb		mm0, mm2
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.loop5x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.loop5y

		ret

align	16
.isseloop5y
		mov		eax, width

.isseloop5x	movq		mm0, [src]

			pavgb		mm0, [src+pitch]

			add			src, 8
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.isseloop5x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.isseloop5y

		ret

align 16
.pred_001	; no norizontal nor vertical half-pel, with averaging

		test	avg, 2			; ISSE allowed?
		jnz		.isseloop6y

		movq		mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop6y	mov			eax, width

.loop6x		movq		mm0, [src]

			movq		mm1, [dst]
			movq		mm2, mm0

			pxor		mm2, mm1
			por			mm0, mm1

			psrlq		mm2, 1

			pand		mm2, mm7	; 7F7F7F7F7F7F7F7F
			add			src, 8

			psubb		mm0, mm2
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.loop6x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.loop6y

		ret

align	16
.isseloop6y
		mov		eax, width

.isseloop6x	movq		mm0, [src]

			pavgb		mm0, [dst]

			add			src, 8
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.isseloop6x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.isseloop6y

		ret

align 16
.pred_000	; no horizontal nor vertical half-pel, no averaging

.loop7y	mov		eax, width

.loop7x		movq		mm0, [src]

			add			src, 8
			sub			eax, 8

			movq		[dst], mm0

			lea			dst, [dst+8]
			jg			.loop7x

		add		src, modulus
		add		dst, modulus
		dec		height
		jg		.loop7y

		ret


    ; mmx_conv420to422(
    ;   [rcx]       const void* src,        // source chroma buffer
    ;   [rdx]       void*       dst,        // destination chroma buffer
    ;   [r8]        int         width,      // coded_picture_width
    ;   [r9]        int         height,     // coded_picture_height
    ;   [rsp+28h]   bool        progressive // is progressive
    ; );

	; The caller is responsible for ensuring that:
	; 1) src and dst are valid pointers
	; 2) cpw is a nonzero multiple of 16
	; 3) cph is a nonzero multiple of 16
	; 4) the src buffer is at least ((cpw / 2) * (cph / 2)) bytes
	; 5) the dst buffer is at least ((cpw / 2) * cph) bytes
	; 6) the MMX state is emptied with emms

PROC mmx_conv420to422

	; Local
	%define cphm1       [rsp+20h]

	and		r8, 0FFFFFFFFh		; cpw
	and		r9, 0FFFFFFFFh		; cph

	test	byte [rsp+28h], 1	; progressive?
	jz		.interlaced

; Progressive MPEG-1 or MPEG-2:
    ; The chroma is subsampled in the middle:
    ; MPEG-1          MPEG-2
    ;   Y . Y . Y       Y . Y . Y
    ;   . C . C .       C . C . C
    ;   Y . Y . Y       Y . Y . Y

	movq    mm6, [mmx_0002000200020002]
	pxor	mm7, mm7

	; 4:2:0 chroma coded picture width/height is half luma
	shr		r9, 1
	dec		r9
	mov		cphm1, r9			; we want (cph - 1) in the loop

	shr		r8, 1				; r8 = chroma coded_picture_width
	xor		r9, r9				; r9 = y loop counter

.syloop
	cmp		r9, cphm1			; y > (cph - 1)?
	jg		.finished			; break if so

	mov		rax, r8				; rax = x loop counter
	xor		r10, r10			; r10 = sm1, offset to (src - 1) row
	xor		r11, r11			; r11 = sp1, offset to (src + 1) row

    ; The following branches fall through for all iterations but two
	cmp		r9, r10				; y > 0?
	jle		.sok1
	sub		r10, r8				; (sm1 points to the previous line)
.sok1
	cmp		r9, cphm1			; r9 < (cph - 1)?
	jge		.sok2
	add		r11, r8				; (sp1 points to the next line)
.sok2

	; MPEG-1 progressive upsample:
	; (dst+0) = ((src+0) * 3 + (src-1) + 2) >> 2
	; (dst+1) = ((src+0) * 3 + (src+1) + 2) >> 2

.sxloop
	movq		mm0, [rcx]		; mm0 = src

	movq		mm2, [rcx+r10]	; mm2 = sm1
	movq		mm1, mm0		; mm1 = copy of src

	punpcklbw	mm0, mm7		; mm0 = srclo
	movq		mm3, mm2		; mm3 = copy of sm1

	punpckhbw	mm1, mm7		; mm1 = srchi
	movq		mm4, mm0		; mm4 = copy of srclo

	punpcklbw	mm2, mm7		; mm2 = sm1lo
	movq		mm5, mm1		; mm5 = copy of srchi

	punpckhbw	mm3, mm7		; mm3 = sm1hi
	paddw		mm0, mm4		; mm0 = srclo * 2

	paddw		mm1, mm5		; mm1 = srchi * 2
	paddw		mm0, mm4		; mm0 = srclo * 3

	paddw		mm1, mm5		; mm1 = srchi * 3
	paddw		mm0, mm6		; mm0 = srclo * 3 + 2

	paddw		mm2, mm0		; mm2 = sm1lo + srclo * 3 + 2
	paddw		mm1, mm6		; mm1 = srchi * 3 + 2

	psrlw		mm2, 2			; mm2 = [# dstlo finished #]
	paddw		mm3, mm1		; mm3 = sm1hi + srchi * 3 + 2

	psrlw		mm3, 2			; mm3 = [# dsthi finished #]
	movq		mm4, [rcx+r11]	; mm4 = sp1

	packuswb	mm2, mm3		; mm2 = [# dst finished #]
	movq		mm5, mm4		; mm5 = copy of sp1

	punpcklbw	mm4, mm7		; mm4 = sp1lo
	add			rcx, 8			; increment src pointer

	punpckhbw	mm5, mm7		; mm5 = sp1hi
	paddw		mm0, mm4		; mm0 = srclo * 3 + sp1lo + 2

	psrlw		mm0, 2			; mm0 = [# dp1lo finished #]
	paddw		mm1, mm5		; mm1 = srchi * 3 + sp1hi + 2

	movq		[rdx], mm2		; store dst
	psrlw		mm1, 2			; mm1 = [# dp1hi finished #]

	packuswb	mm0, mm1		; mm1 = [# dp1 finished #]
	sub			rax, 8			; decrement x loop counter

	movq		[rdx+r8], mm0	; store dp1

	lea			rdx, [rdx+8]	; increment dst pointer
	jg			.sxloop

	add			rdx, r8			; advance to next row
	inc			r9				; increment y loop counter

	jmp			.syloop

.interlaced

	movq		mm6, [mmx_0002000200020002]

	pxor		mm7, mm7
	paddw		mm6, mm6

	shr			r9, 2			; cph = chroma height / 2
	dec			r9				; we want (cph - 1) in the loop
	mov			cphm1, r9
	xor			r9, r9			; r9 = y loop counter

.myloop
	cmp			r9, cphm1		; y > (cph - 1)?
	jg			.finished		; break if so

	xor			r10, r10		; sm1 (src minus 1)
	xor			r11, r11		; sp1 (src plus 1)

	cmp			r9, r10			; y > 0?
	jle			.mok1
	sub			r10, r8
.mok1
	cmp			r9, cphm1		; y < (cph - 1)?
	jge			.mok2
	add			r11, r8
.mok2
	mov			rax, r8			; rax = x loop counter

	; top field:
	; (dst+0) = ((src-1) * 1 + (src+0) * 7 + 4) >> 3
	; (dst+1) = ((src+0) * 5 + (src+1) * 3 + 4) >> 3

.mxloop1
	movq		mm0, [rcx]		; mm0 = src

    movq        mm2, [rcx+r10]  ; mm2 = sm1
	movq		mm1, mm0		; mm1 = copy of src

    movq        mm3, mm2		; mm3 = copy of sm1
	punpcklbw	mm0, mm7		; mm0 = srclo

	movq		mm4, mm0		; mm4 = copy of srclo
	punpckhbw	mm1, mm7		; mm1 = srchi

	movq		mm5, mm1		; mm5 = copy of srchi
    psllw       mm4, 3			; mm4 = srclo * 8

	psubw		mm4, mm0		; mm4 = srclo * 7
    psllw       mm5, 3			; mm5 = srchi * 8

	psubw		mm5, mm1		; mm5 = srchi * 7
	punpcklbw	mm2, mm7		; mm2 = sm1lo

    paddw       mm2, mm4        ; mm2 = sm1lo + srclo * 7
	punpckhbw	mm3, mm7		; mm3 = sm1hi
	
    paddw       mm3, mm5        ; mm3 = sm1hi + srchi * 7
    movq        mm4, mm0        ; mm4 = copy of srclo

	paddw		mm2, mm6		; mm2 = sm1lo + srclo * 7 + 4
    movq        mm5, mm1        ; mm5 = copy of srchi

	paddw		mm3, mm6		; mm3 = sm1hi + srchi * 7 + 4
    psllw       mm4, 2			; mm4 = srclo * 4

	paddw		mm4, mm0		; mm4 = srclo * 5
	psllw		mm5, 2			; mm5 = srchi * 4

	paddw		mm5, mm1		; mm5 = srchi * 5
    psrlw       mm2, 3          ; mm2 = [# dstlo finished #]

    movq        mm0, [rcx+r11]  ; mm0 = sp1
	psrlw		mm3, 3			; mm3 = [# dsthi finished #]

	movq		mm1, mm0		; mm1 = copy of sp1
	punpcklbw	mm0, mm7		; mm0 = sp1lo

	paddw		mm4, mm0		; mm4 = srclo * 5 + sp1lo
	punpckhbw	mm1, mm7		; mm1 = sp1hi

	packuswb	mm2, mm3		; mm2 = [# dst finished #]
	paddw		mm4, mm0		; mm4 = srclo * 5 + sp1lo * 2

	paddw		mm5, mm1		; mm5 = srchi * 5 + sp1hi
	paddw		mm4, mm0		; mm4 = srclo * 5 + sp1lo * 3

	paddw		mm5, mm1		; mm5 = srchi * 5 + sp1hi * 2
	paddw		mm4, mm6		; mm4 = srclo * 5 + sp1lo * 3 + 4

	movq		[rdx], mm2		; store dst
	paddw		mm5, mm1		; mm5 = srchi * 5 + sp1hi * 3

	psrlw		mm4, 3			; mm4 = [# dp1lo finished #]
	paddw		mm5, mm6		; mm5 = srchi * 5 + sp1hi * 3 + 4

	psrlw		mm5, 3			; mm5 = [# dp1hi finished #]
	add			rcx, 8

	packuswb	mm4, mm5		; mm4 = [# dp1 finished #]
	sub			rax, 16			; decrement x loop counter

	movq		[rdx+r8], mm4	; store dp1

	lea			rdx, [rdx+8]
    jg          .mxloop1

	; bottom field:
	; (dst+0) = ((src-1) * 3 + (src+0) * 5 + 4) >> 3
	; (dst+1) = ((src+0) * 7 + (src+1) * 1 + 4) >> 3

	mov			rax, r8			; rax = x loop counter

.mxloop2
	movq		mm0, [rcx]		; mm0 = src

    movq        mm2, [rcx+r10]  ; mm2 = sm1
	movq		mm1, mm0		; mm1 = copy of src

    movq        mm3, mm2		; mm3 = copy of sm1
	punpcklbw	mm0, mm7		; mm0 = srclo

	movq		mm4, mm0		; mm4 = copy of srclo
	punpckhbw	mm1, mm7		; mm1 = srchi

	movq		mm5, mm1		; mm5 = copy of srchi
    psllw       mm4, 2			; mm4 = srclo * 4

	paddw		mm4, mm0		; mm4 = srclo * 5
    psllw       mm5, 2			; mm5 = srchi * 4

	paddw		mm5, mm1		; mm5 = srchi * 5
	punpcklbw	mm2, mm7		; mm2 = sm1lo
	
	paddw		mm4, mm6		; mm4 = srclo * 5 + 4
	punpckhbw	mm3, mm7		; mm3 = sm1hi
	
	paddw		mm5, mm6		; mm5 = srchi * 5 + 4
	paddw		mm4, mm2		; mm4 = srclo * 5 + sm1lo + 4

	paddw		mm5, mm3		; mm5 = srchi * 5 + sm1hi + 4
	paddw		mm4, mm2		; mm4 = srclo * 5 + sm1lo * 2 + 4

	paddw		mm5, mm3		; mm5 = srchi * 5 + sm1hi * 2 + 4
	paddw		mm4, mm2		; mm4 = srclo * 5 + sm1lo * 3 + 4

	movq		mm2, mm0		; mm2 = copy of srclo
	psrlw		mm4, 3			; mm4 = [# dp0lo finished #]

	paddw		mm5, mm3		; mm5 = srchi * 5 + sm1hi * 3 + 4
	psllw		mm0, 3			; mm0 = srclo * 8

	psubw		mm0, mm2		; mm0 = srclo * 7
	psrlw		mm5, 3			; mm5 = [# dsthi finished #]

	movq		mm2, [rcx+r11]	; mm2 = sp1
	movq		mm3, mm1		; mm3 = copy of srchi

	psllw		mm1, 3			; mm1 = srchi * 8
	paddw		mm0, mm6		; mm0 = srclo * 7 + 4

	packuswb	mm4, mm5		; mm4 = [# dst finished #]
	psubw		mm1, mm3		; mm1 = srchi * 7

	movq		mm3, mm2		; mm3 = copy of sp1
	punpcklbw	mm2, mm7		; mm2 = sp1lo

	paddw		mm1, mm6		; mm1 = srchi * 7 + 4
	punpckhbw	mm3, mm7		; mm3 = sp1hi
	
	movq		[rdx], mm4		; store dst
	paddw		mm0, mm2		; mm0 = srclo * 7 + sp1lo + 4

	psrlw		mm0, 3			; mm0 = [# dp1lo finished #]
	paddw		mm1, mm3		; mm1 = srchi * 7 + sp1hi + 4
	
	psrlw		mm1, 3			; mm1 = [# dp1hi finished #]
	add			rcx, 8

	packuswb	mm0, mm1		; [# dp1 finished #]
	sub			rax, 16			; decrement x loop counter

	movq		[rdx+r8], mm0	; store dp1
	
	lea			rdx, [rdx+8]
	jg			.mxloop2

    add			rdx, r8
	inc			r9				; increment y loop counter

	jmp			.myloop

.finished
	ret


    ; mmx_conv422to444(
    ;   [rcx]       const void* src,
    ;   [rdx]       void*       dst,
    ;   [r8]        int         cpw,
    ;   [r9]        int         cph,
    ;   [rsp+28h]   bool        mpeg2
    ; );

	; The caller is responsible for ensuring that:
	; 1) src and dst are valid pointers
	; 2) cpw is a nonzero multiple of 16
	; 3) cph is a nonzero multiple of 16
	; 4) the src buffer is at least ((cpw / 2) * cph) bytes
	; 5) the dst buffer is at least (cpw * cph) bytes
	; 6) the MMX state is emptied with emms

PROC mmx_conv422to444

	and		r8, 0FFFFFFFFh			; r8 = coded_picture_width
	and		r9, 0FFFFFFFFh			; r9 = coded_picture_height

	test	byte [rsp+28h], 1		; MPEG-2?
	jnz		.Mpg2

	; MPEG-1 chroma upsampling.

    movq    mm6, [mmx_0002000200020002]
	pxor	mm7, mm7

.syloop
	dec		r9
	js		.finished

; For SIMD we will have to implement the following filter.
; The source pixels will need to be unpacked, processed as
; words, then interleaved for storing.  The MPEG-2 filter does
; something similar, but MPEG-1 blending requires three samples
; and is more complex:
;
;src:
; 25%         [+2]      [+1]      [+0]      [-1]
; 75%    [+3] [+3] [+2] [+2] [+1] [+1] [+0] [+0]
; 25%    [+4]      [+3]      [+2]      [+1]
; dst+0: [+7] [+6] [+5] [+4] [+3] [+2] [+1] [+0]
;
;src:
; 25%         [+6]      [+5]      [+4]      [+3]
; 75%    [+7] [+7] [+6] [+6] [+5] [+5] [+4] [+4]
; 25%    [+8]      [+7]      [+6]      [+5]
; dst+8: [+7] [+6] [+5] [+4] [+3] [+2] [+1] [+0]

	movq	mm0, [rcx]

; Initially, the last pixel of sm1 is the same as the first pixel of src
	movq	mm1, mm0
	psllq	mm0, 56			; mm1 = [0.......]

	mov		rax, r8			; rax = coded_picture_width

	shr		rax, 4			; rax = coded_picture_width / 16
	add		rcx, 8

.smxloop:
	dec		rax
	jle		.smxloopdone

	; At the start of each iteration:
	; mm0 contains the prev src sample (we take only its last pixel)
	; mm1 contains the prev sp1 sample [76543210]

	movq		mm2, mm0		; mm2 = prev [76543210]
	movq		mm0, mm1		; rotate prev sp1 into current src

	movq		mm5, mm1		; mm5 = curr [76543210]
	psrlq		mm2, 56			; mm2 = prev [.......7]

	movq		mm1, [rcx]		; mm1 = new sp1 sample
	punpcklbw	mm5, mm7		; mm5 = [+3][+2][+1][+0]

	punpcklbw	mm2, mm7		; mm2 = [  ][  ][  ][-1]
	movq		mm3, mm5		; mm3 = [+3][+2][+1][+0]

	psllq		mm5, 16			; mm5 = [+2][+1][+0][  ]
	movq		mm4, mm3		; mm4 = copy of src

	por			mm2, mm5		; mm2 = [+2][+1][+0][-1], mm5 free
	paddw		mm3, mm4		; mm3 = src * 2

	movq		mm5, mm0		; mm5 = curr [76543210]
	paddw		mm3, mm4		; mm3 = src * 3, mm4 free

	psrlq		mm5, 8			; mm5 = next [.7654321]
	paddw		mm3, mm6		; mm3 = src * 3 + 2

	punpcklbw	mm5, mm7		; mm5 = [+4][+3][+2][+1]
	paddw		mm2, mm3		; mm2 = src * 3 + 2 + sm1

	paddw		mm5, mm3		; mm5 = src * 3 + 2 + sp1, mm3 free
	psrlw		mm2, 2			; mm2 [# finished #]

	psrlw		mm5, 2			; mm5 [# finished #]
	movq		mm3, mm0		; mm3 = curr [76543210]

	psllq		mm5, 8			; shift for interleave
	punpckhbw	mm3, mm7		; mm4 = [+7][+6][+5][+4]

	por			mm2, mm5		; mm2 = dst, mm5 free
	movq		mm4, mm3		; mm4 = copy of src

	movq		mm5, mm1		; mm5 = next [76543210]
	paddw		mm3, mm4		; mm3 = src * 2

	psllq		mm5, 56			; mm5 = next [0.......]
	paddw		mm3, mm4		; mm3 = src * 3, mm4 free

	movq		[rdx], mm2		; store dst, mm2 free
	movq		mm4, mm0		; mm4 = curr [76543210]

	movq		mm2, mm0		; mm2 = curr [76543210]	
	psrlq		mm4, 8			; mm4 = curr [.7654321]

	por			mm4, mm5		; mm4 = [87654321], mm5 free
	psllq		mm2, 8			; mm2 = curr [6543210.]

	paddw		mm3, mm6		; mm3 = src * 3 + 2
	punpckhbw	mm2, mm7		; mm2 = [+6][+5][+4][+3]

	punpckhbw	mm4, mm7		; mm4 = [+8][+7][+6][+5]
	paddw		mm2, mm3		; mm2 = src * 3 + sm1 + 2

	paddw		mm4, mm3		; mm4 = src * 3 + sp1 + 2
	psrlw		mm2, 2			; mm2 [# finished #]

	psrlw		mm4, 2			; mm4 [# finished #]
	
	psllq		mm2, 8			; shift for interleave

	por			mm2, mm4		; mm2 = dp1
	add			rcx, 8

	movq		[rdx+8], mm2

	add			rdx, 16
	jmp			.smxloop

	; The last block needs special handling.

.smxloopdone
	movq		mm2, mm0		; mm2 = prev [76543210]
	movq		mm0, mm1		; rotate prev sp1 into current src

	movq		mm5, mm1		; mm5 = curr [76543210]
	psrlq		mm2, 56			; mm2 = prev [.......7]

	movq		mm1, mm0  		; mm1 = new sp1 sample (same as src)
	punpcklbw	mm5, mm7		; mm5 = [+3][+2][+1][+0]

	punpcklbw	mm2, mm7		; mm2 = [  ][  ][  ][-1]
	movq		mm3, mm5		; mm3 = [+3][+2][+1][+0]

	psllq		mm5, 16			; mm5 = [+2][+1][+0][  ]
	movq		mm4, mm3		; mm4 = copy of src

	por			mm2, mm5		; mm2 = [+2][+1][+0][-1], mm5 free
	paddw		mm3, mm4		; mm3 = src * 2

	movq		mm5, mm0		; mm5 = curr [76543210]
	paddw		mm3, mm4		; mm3 = src * 3, mm4 free

	psrlq		mm5, 8			; mm5 = next [.7654321]
	paddw		mm3, mm6		; mm3 = src * 3 + 2

	punpcklbw	mm5, mm7		; mm5 = [+4][+3][+2][+1]
	paddw		mm2, mm3		; mm2 = src * 3 + 2 + sm1

	paddw		mm5, mm3		; mm5 = src * 3 + 2 + sp1, mm3 free
	psrlw		mm2, 2			; mm2 [# finished #]

	psrlw		mm5, 2			; mm5 [# finished #]
	movq		mm3, mm0		; mm3 = curr [76543210]

	psllq		mm5, 8			; shift for interleave
	punpckhbw	mm3, mm7		; mm4 = [+7][+6][+5][+4]

	por			mm2, mm5		; mm2 = dst, mm5 free
	movq		mm4, mm3		; mm4 = copy of src

	psrlq		mm1, 56			; mm1 = curr [.......7]
	paddw		mm3, mm4		; mm3 = src * 2

	psllq		mm1, 56			; mm1 = curr [7.......]
	paddw		mm3, mm4		; mm3 = src * 3, mm4 free

	movq		[rdx], mm2		; store dst, mm2 free
	movq		mm4, mm0		; mm4 = curr [76543210]

	movq		mm2, mm0		; mm2 = curr [76543210]	
	psrlq		mm4, 8			; mm4 = curr [.7654321]

	por			mm4, mm1		; mm4 = [77654321]
	psllq		mm2, 8			; mm2 = curr [6543210.]

	paddw		mm3, mm6		; mm3 = src * 3 + 2
	punpckhbw	mm2, mm7		; mm2 = [+6][+5][+4][+3]

	punpckhbw	mm4, mm7		; mm4 = [+8][+7][+6][+5]
	paddw		mm2, mm3		; mm2 = src * 3 + sm1 + 2

	paddw		mm4, mm3		; mm4 = src * 3 + sp1 + 2
	psrlw		mm2, 2			; mm2 [# finished #]

	psrlw		mm4, 2			; mm4 [# finished #]
	
	psllq		mm2, 8			; shift for interleave

	por			mm2, mm4		; mm2 = dp1

	movq		[rdx+8], mm2

	add			rdx, 16
	jmp			.syloop

.Mpg2
	; MPEG-2 chroma upsampling.
    ;   Y . Y . Y
    ;   C . C . C
    ;   Y . Y . Y

    movq    mm6, [mmx_0101010101010101]
    movq    mm7, [mmx_7F7F7F7F7F7F7F7F]
	pcmpeqd	mm5, mm5	; FFFFFFFFFFFFFFFF
	psllq	mm5, 56		; FF00000000000000

.myloop
	dec			r9
	js			.finished

	movq		mm1, [rcx]

	mov			rax, r8		; rax = coded_picture_width
	add			rcx, 8

	sub			rax, 16
	jle			.mxloopdone

	; let mm0 = 76543210
	; let mm1 = FEDCBA98

.mxloop
	movq		mm0, mm1		; prev = current
	movq		mm2, mm1		; mm2 = current

	movq		mm3, [rcx]		; fetch next
	psrlq		mm2, 8			; .7654321

	movq		mm1, mm3		; current = next (for next pass)
	psllq		mm3, 56			; 8.......

	por			mm3, mm2		; mm3 = 87654321
	movq		mm2, mm0		; mm2 = 76543210

	; Now, produce the average of mm2 and mm3
	movq		mm4, mm2
	psrlq		mm2, 1

	por			mm4, mm3
	psrlq		mm3, 1

	pand		mm4, mm6		; 0101010101010101
	pand		mm2, mm7		; 7F7F7F7F7F7F7F7F

	paddb		mm2, mm4
	pand		mm3, mm7		; 7F7F7F7F7F7F7F7F

	paddb		mm2, mm3		; done (mm2 = average)
	movq		mm3, mm0		; 76543210

	; Interleave
	movq		mm4, mm0
	punpcklbw	mm3, mm2		; .3.2.1.0

	punpckhbw	mm4, mm2		; .7.6.5.4
	add			rcx, 8

	sub			rax, 16

	movq		[rdx], mm3

	movq		[rdx+8], mm4

	lea			rdx, [rdx+16]
	jg			.mxloop

.mxloopdone

	; The final byte of each row needs special handling
	movq		mm3, mm1
	psrlq		mm1, 8			; .7654321

	movq		mm0, mm3		; mm0 = current
	pand		mm3, mm5		; FF00000000000000

	movq		mm2, mm0		; mm0 = 76543210
	por			mm3, mm1		; mm3 = 77654321

	; Now, produce the average of mm2 and mm3
	movq		mm4, mm2
	psrlq		mm2, 1

	por			mm4, mm3
	psrlq		mm3, 1

	pand		mm4, mm6		; 0101010101010101
	pand		mm2, mm7		; 7F7F7F7F7F7F7F7F

	paddb		mm2, mm4
	pand		mm3, mm7		; 7F7F7F7F7F7F7F7F

	paddb		mm2, mm3		; done (mm2 = average)
	movq		mm3, mm0		; 76543210

	; Interleave
	movq		mm4, mm0
	punpcklbw	mm3, mm2		; .3.2.1.0

	punpckhbw	mm4, mm2		; .7.6.5.4

	movq		[rdx], mm3

	movq		[rdx+8], mm4

	add			rdx, 16
	jmp			.myloop

.finished
	ret


    ; mmx_conv422toYUY2(
    ;   [rcx]       void *srcY,
    ;   [rdx]       void *srcU,
	;	[r8]        void *srcV,
	;	[r9]        void *dst,
    ;   [rsp+28h]   int  width,
    ;   [rsp+30h]   int  height
    ; );

PROC mmx_conv422toYUY2

	mov		r10d, dword [rsp+28h]		; height

.yloop
		mov			eax, dword [rsp+30h]	; width

	; bias pointers
		lea			rcx, [rcx+rax*2]
		add			rdx, rax
		lea			r9, [r9+rax*4]
		add			r8, rax

		neg			rax

.xloop
		movq		mm2, [rdx+rax]		; U7 U6 U5 U4 U3 U2 U1 U0

		movq		mm3, [r8+rax]		; V7 V6 V5 V4 V3 V2 V1 V0
		movq		mm4, mm2			; U7 U6 U5 U4 U3 U2 U1 U0

		movq		mm5, [rcx+rax*2]	; Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
		punpcklbw	mm4, mm3			; V3 U3 V2 U2 V1 U1 V0 U0

		movq		mm0, mm5			; Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
		punpcklbw	mm5, mm4			; V1 Y3 U1 Y2 V0 Y1 U0 Y0 (dst0)

		movq		mm1, [rcx+rax*2+8]	; YF YE YD YC YB YA Y9 Y8
		punpckhbw	mm0, mm4			; V3 Y7 U3 Y6 V2 Y5 U2 Y4 (dst1)

		movq		[r9+rax*4], mm5
		punpckhbw	mm2, mm3			; V7 U7 V6 U6 V5 U5 V4 U4

		movq		mm4, mm1			; YF YE YD YC YB YA Y9 Y8
		punpcklbw	mm1, mm2			; V5 YB U5 YA V4 Y9 U4 Y8 (dst2)

		movq		[r9+rax*4+8], mm0
		punpckhbw	mm4, mm2			; V7 YF U7 YE V6 YD U6 YC (dst3)

		movq		[r9+rax*4+16], mm1

		movq		[r9+rax*4+24], mm4

		add			rax, 8
		jl			.xloop

		dec			r10d
		jg			.yloop

		emms
		ret


    ; mmx_conv422toUYVY(
    ;   [rcx]       void *srcY,
    ;   [rdx]       void *srcU,
	;	[r8]        void *srcV,
	;	[r9]        void *dst,
    ;   [rsp+28h]   int  width,
    ;   [rsp+30h]   int  height
    ; );

PROC mmx_conv422toUYVY

	mov		r10d, dword [rsp+28h]		; height

.yloop
		mov			eax, dword [rsp+30h]	; width

	; bias pointers
		lea			rcx, [rcx+rax*2]
		add			rdx, rax
		lea			r9, [r9+rax*4]
		add			r8, rax

		neg			rax

.xloop
		movq		mm2, [rdx+rax]		; U7 U6 U5 U4 U3 U2 U1 U0

		movq		mm3, [r8+rax]		; V7 V6 V5 V4 V3 V2 V1 V0
		movq		mm4, mm2			; U7 U6 U5 U4 U3 U2 U1 U0

		movq		mm0, [rcx+rax*2]	; Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
		punpcklbw	mm4, mm3			; V3 U3 V2 U2 V1 U1 V0 U0

		movq		mm5, mm4			; V3 U3 V2 U2 V1 U1 V0 U0
		punpcklbw	mm4, mm0			; Y3 V1 Y2 U1 Y1 V0 Y0 U0 (dst0)

		movq		mm1, [rcx+rax*2+8]	; YF YE YD YC YB YA Y9 Y8
		punpckhbw	mm5, mm0			; Y7 V3 Y6 U3 Y5 V2 Y4 U2 (dst1)

		movq		[r9+rax*4], mm4
		punpckhbw	mm2, mm3			; V7 U7 V6 U6 V5 U5 V4 U4

		movq		mm4, mm2			; V7 U7 V6 U6 V5 U5 V4 U4
		punpcklbw	mm2, mm1			; YB V5 YA U5 Y9 V4 Y8 U4 (dst2)

		movq		[r9+rax*4+8], mm5
		punpckhbw	mm4, mm1			; YF V7 YE U7 YD V6 YC U6 (dst3)

		movq		[r9+rax*4+16], mm2

		movq		[r9+rax*4+24], mm4

		add			rax, 8
		jl			.xloop

		dec			r10d
		jg			.yloop

		emms
		ret

