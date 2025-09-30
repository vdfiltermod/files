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


		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		;; MMX and ISSE code for MPEG-2 Decoder ;;
		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

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

	segment .rdata, align=16

%include "idct_tables.inc"


%macro PROC 1
	global %1
	align 16
	%1:
%endmacro


	segment .text

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; MMX and ISSE IDCTs, adapted from Xvid source code
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%include "idct_macros.inc"


PROC _mmx_IDCT

	mov		eax, [esp+4]

	DCT_8_INV_ROW_MMX eax+0*16, eax+0*16, tab_i_04_mmx, rounder_0
	DCT_8_INV_ROW_MMX eax+1*16, eax+1*16, tab_i_17_mmx, rounder_1
	DCT_8_INV_ROW_MMX eax+2*16, eax+2*16, tab_i_26_mmx, rounder_2
	DCT_8_INV_ROW_MMX eax+3*16, eax+3*16, tab_i_35_mmx, rounder_3
	DCT_8_INV_ROW_MMX eax+4*16, eax+4*16, tab_i_04_mmx, rounder_4
	DCT_8_INV_ROW_MMX eax+5*16, eax+5*16, tab_i_35_mmx, rounder_5
	DCT_8_INV_ROW_MMX eax+6*16, eax+6*16, tab_i_26_mmx, rounder_6
	DCT_8_INV_ROW_MMX eax+7*16, eax+7*16, tab_i_17_mmx, rounder_7

	DCT_8_INV_COL eax+0, eax+0
	DCT_8_INV_COL eax+8, eax+8

	ret    


PROC _isse_IDCT

	mov		eax, [esp+4]

	DCT_8_INV_ROW_XMM eax+0*16, eax+0*16, tab_i_04_xmm, rounder_0
	DCT_8_INV_ROW_XMM eax+1*16, eax+1*16, tab_i_17_xmm, rounder_1
	DCT_8_INV_ROW_XMM eax+2*16, eax+2*16, tab_i_26_xmm, rounder_2
	DCT_8_INV_ROW_XMM eax+3*16, eax+3*16, tab_i_35_xmm, rounder_3
	DCT_8_INV_ROW_XMM eax+4*16, eax+4*16, tab_i_04_xmm, rounder_4
	DCT_8_INV_ROW_XMM eax+5*16, eax+5*16, tab_i_35_xmm, rounder_5
	DCT_8_INV_ROW_XMM eax+6*16, eax+6*16, tab_i_26_xmm, rounder_6
	DCT_8_INV_ROW_XMM eax+7*16, eax+7*16, tab_i_17_xmm, rounder_7

	DCT_8_INV_COL eax+0, eax+0
	DCT_8_INV_COL eax+8, eax+8

	ret


	; mmx_Add_Block(
	;   [esp+ 4]    const void* blk,
	;   [esp+ 8]    void*       dst,
	;   [esp+12]    int         pitch,
	;   [esp+16]    int         addflag
	; );

	; The caller is responsible for ensuring that:
	; 1) blk and dst are valid pointers
	; 2) pitch is greater than or equal to 8
	; 3) the blk buffer is at least 128 bytes
	; 4) the dst buffer is at least (pitch * 8) bytes
	; 5) the MMX state is emptied with emms

PROC _mmx_Add_Block

	push	esi
	push	edi

	; Parameters
	%define blk     dword [esp+8+1*4]
	%define dst     dword [esp+8+2*4]
	%define pitch   dword [esp+8+3*4]
	%define addflag dword [esp+8+4*4]

	; "blk" is an array of 64 signed short values
	; "dst" is a series of 8 lines with 8 bytes per line
	; The lines of "dst" are separated by "pitch"

	mov		esi, blk			; esi = blk
	mov		edi, dst			; edi = dst

	mov		edx, pitch			; edx = dst pitch
	mov		eax, 8				; eax = loop counter

	test	addflag, 1			; adding blk and dst?
	jz		.noadd				; jump if not adding

	pxor	mm7, mm7			; clear mm7

.yloop1
	movq		mm0, [edi]		; mm0 = dst[76543210]

	movq		mm1, mm0		; mm1 = copy of dst
	punpcklbw	mm0, mm7		; mm0 = dst[+3][+2][+1][+0]

	paddw		mm0, [esi]		; mm0 = dstlo + blklo
	punpckhbw	mm1, mm7		; mm1 = dst[+7][+6][+5][+4]
	
	paddw		mm1, [esi+8]	; mm1 = dsthi + blkhi

	packuswb	mm0, mm1		; dst finished
	add			esi, 16			; advance blk pointer

	dec			eax				; decrement loop counter

	movq		[edi], mm0		; store dst

	lea			edi, [edi+edx]	; advance dst pointer
	jnz			.yloop1			; continue until eax == zero

	jmp		.finished

.noadd
	movq		mm7, [mmx_0101010101010101]
	psllq		mm7, 7			; mm7 = 8080808080808080h

.yloop2
	movq		mm0, [esi]		; mm0 = blklo
	
	packsswb	mm0, [esi+8]	; mm0 = pack and saturate blklo and blkhi

	paddb		mm0, mm7		; add 128 (convert to unsigned byte)
	add			esi, 16			; advance blk pointer

	dec			eax				; decrement loop counter

	movq		[edi], mm0		; store dst

	lea			edi, [edi+edx]	; advance dst pointer
	jnz			.yloop2			; continue until eax == zero

.finished
	pop		edi
	pop		esi
	ret


	; mmx_form_component_prediction(
	;   [esp+ 4]    const void* src,        source of prediction
	;   [esp+ 8]    void*       dst,        destination for prediction
	;   [esp+12]    int         width,      width of block (8 or 16)
	;   [esp+16]    int         height,     height of block (4, 8 or 16)
	;   [esp+20]    int         pitch,      pitch (both src and dst)
	;   [esp+24]    int         deltax,     bottom bit indicates half-pel
	;   [esp+28]    int         deltay,     bottom bit indicates half-pel
	;   [esp+32]    int         avg         averaging flag (bottom bit)
	; );

	; The caller is responsible for ensuring that:
	; 1) src and dst are valid pointers
	; 2) width is either 8 or 16
	; 3) height is either 4, 8, or 16
	; 4) pitch is greater than or equal to width
	; 5) the MMX state is emptied with emms

	; If we seek to be re-entrant, we can't rely on
	; a data segment parameter.  Instead, we'll modify
	; the last parameter on the stack to signify ISSE.

PROC _isse_form_component_prediction
		or		dword [esp+32], 2
		jmp		_mmx_form_component_prediction.asm_form_component_prediction

PROC _mmx_form_component_prediction
		and		dword [esp+32], 0FFFFFFDh

.asm_form_component_prediction
		push	esi
		push	edi
		push	ebx
		push	ebp

		; Parameters
		%define src     dword [esp+16+1*4]
		%define dst     dword [esp+16+2*4]
		%define width   dword [esp+16+3*4]
		%define height  dword [esp+16+4*4]
		%define pitch   dword [esp+16+5*4]
		%define deltax  dword [esp+16+6*4]
		%define deltay  dword [esp+16+7*4]
		%define avg     dword [esp+16+8*4]

	; These registers are common to every method
	; Note: width is always either 8 or 16

		mov		esi, src		; esi = src
		mov		edi, dst		; edi = dst
		mov		ecx, width		; ecx = width
		mov		ebp, height		; ebp = height
		mov		edx, pitch		; edx = pitch
		mov		ebx, edx		; ebx = pitch
		sub		edx, ecx		; edx = pitch - width

	; Determine which routine handles this block

		test	deltax, 1
		jz		.nohx
		test	deltay, 1
		jz		.nohy
		test	avg, 1
		jnz		.pred_111			; horz and vert halfpel with avg
		jmp		.pred_110			; horz and vert halfpel, no avg
.nohy
		test	avg, 1
		jnz		.pred_101			; horz halfpel no vert with avg
		jmp		.pred_100			; horz halfpel no vert, no avg
.nohx
		test	deltay, 1
		jz		.nohxy
		test	avg, 1
		jnz		.pred_011			; vert halfpel no horz with avg
		jmp		.pred_010			; vert halfpel no horz, no avg
.nohxy
		test	avg, 1
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

.loop0y	mov			eax, ecx

.loop0x		movq		mm0, [esi]

			movq		mm2, [esi+1]
			movq		mm1, mm0

			movq		mm3, mm2
			punpcklbw	mm0, mm7	; mm0 = [esi] low

			movq		mm4, [ebx+esi]
			punpckhbw	mm1, mm7	; mm1 = [edi] high

			movq		mm5, mm4
			punpcklbw	mm2, mm7	; mm2 = [esi+1] low

			paddw		mm0, mm2	; mm0 = X low, mm2 free
			punpckhbw	mm3, mm7	; mm3 = [esi+1] high

			movq		mm2, [ebx+esi+1]
			punpcklbw	mm4, mm7	; mm4 = [ebx] low

			paddw		mm1, mm3	; mm1 = X high, mm3 free
			punpckhbw	mm5, mm7	; mm5 = [ebx] high

			movq		mm3, mm2
			punpcklbw	mm2, mm7	; mm2 = [ebx+1] low

			paddw		mm0, mm6	; mm0 = X low adjusted
			punpckhbw	mm3, mm7	; mm3 = [ebx+1] high

			paddw		mm2, mm4	; mm2 = Y low, mm4 free
			paddw		mm3, mm5	; mm3 = Y high, mm5 free

			movq		mm4, [edi]
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
			add			esi, 8

			packuswb	mm0, mm1
			sub			eax, 8

			movq		[edi], mm0

			lea			edi, [edi+8]
			jg			.loop0x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.loop0y

		jmp		.Done

align 16
.pred_110	; horizontal and vertical half-pel, no averaging

; *dst = (src[x] + src[x+1] + src[x+lx2] + src[x+lx2+1] + 2) >> 2

; Another possible way to do this without unpacking is:
; r1 = (s1 >> 2) + (s2 >> 2) + (s3 >> 2) + (s4 >> 2)
; r2 = ((s1 & 3) + (s2 & 3) + (s3 & 3) + (s4 & 3) + 2) >> 2
; result = r1 + r2
; (I tried it, but it did not improve things...)

		movq		mm6, [mmx_0002000200020002]
		pxor		mm7, mm7

		sub			edi, 8		; bias edi

.loop1y	mov		eax, ecx

.loop1x		movq		mm0, [esi]

			movq		mm2, [esi+1]
			movq		mm1, mm0

			movq		mm3, mm2
			punpcklbw	mm0, mm7		; [esi] low

			paddw		mm0, mm6
			punpckhbw	mm1, mm7		; [esi] high

			paddw		mm1, mm6
			punpcklbw	mm2, mm7		; [esi+1] low

			paddw		mm0, mm2
			punpckhbw	mm3, mm7		; [esi+1] high

			movq		mm2, [ebx+esi]
			paddw		mm1, mm3

			movq		mm3, mm2
			punpcklbw	mm2, mm7		; [ebx] low

			paddw		mm0, mm2
			punpckhbw	mm3, mm7

			movq		mm2, [ebx+esi+1]
			paddw		mm1, mm3

			movq		mm3, mm2
			punpcklbw	mm2, mm7		; [ebx+1] low

			paddw		mm0, mm2
			punpckhbw	mm3, mm7		; [ebx+1] high

			paddw		mm1, mm3
			psrlw		mm0, 2

			psrlw		mm1, 2
			add			edi, 8

			packuswb	mm0, mm1
			add			esi, 8

			movq		[edi], mm0

			sub			eax, 8
			jg			.loop1x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.loop1y

		jmp		.Done

align 16
.pred_101	; horizontal but no vertical half-pel, with averaging

; *dst = ((src[x] + src[x+1] + (*dst) + (*dst) + 3) >> 2

		test	avg, 2		; ISSE allowed?
		jnz		.isseloop2y

		movq	mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop2y	mov			eax, ecx

.loop2x		movq		mm0, [esi]

			movq		mm2, [esi+1]
			movq		mm1, mm0

			pxor		mm0, mm2
			por			mm1, mm2

			movq		mm2, [edi]
			psrlq		mm0, 1

			pand		mm0, mm7	; 7F7F7F7F7F7F7F7F

			psubb		mm1, mm0	; mm1 done, mm0 free

			movq		mm0, mm1
			pxor		mm1, mm2

			por			mm0, mm2
			psrlq		mm1, 1

			pand		mm1, mm7	; 7F7F7F7F7F7F7F7F
			add			esi, 8

			psubb		mm0, mm1
			sub			eax, 8

			movq		[edi], mm0

			lea			edi, [edi+8]
			jg			.loop2x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.loop2y

		jmp		.Done

align	16
.isseloop2y
		mov		eax, ecx

.isseloop2x	movq		mm0, [esi]

			pavgb		mm0, [esi+1]

			pavgb		mm0, [edi]

			add			esi, 8
			sub			eax, 8

			movq		[edi], mm0

			lea			edi, [edi+8]
			jg			.isseloop2x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.isseloop2y

		jmp		.Done

align 16
.pred_100	; horizontal but no vertical half-pel, no averaging

		sub		edi, 8		; bias edi

		test	avg, 2		; ISSE allowed?
		jnz		.isseloop3y

	; There are at least 4 ways to 50/50 average:
	; 1) avg = (s0 + s1 + 1) >> 1
	; 2) avg = (s0 | s1) - ((s0 ^ s1) >> 1)
	; 3) avg = (s0 >> 1) + (s1 >> 1) + ((s0 | s1) & 1)
	; 4) pavgb

		movq		mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop3y	mov			eax, ecx

.loop3x		movq		mm0, [esi]

			movq		mm1, [esi+1]
			movq		mm2, mm0

			pxor		mm2, mm1
			por			mm0, mm1

			psrlq		mm2, 1
			add			edi, 8
			
			pand		mm2, mm7
			add			esi, 8

			psubb		mm0, mm2
			sub			eax, 8

			movq		[edi], mm0
			jg			.loop3x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.loop3y

		jmp		.Done

align	16
.isseloop3y
		mov		eax, ecx

.isseloop3x	movq		mm0, [esi]

			pavgb		mm0, [esi+1]

			add			edi, 8
			add			esi, 8
			sub			eax, 8

			movq		[edi], mm0

			jg			.isseloop3x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.isseloop3y

		jmp		.Done

align 16
.pred_011	; no horizontal but vertical half-pel, with averaging

		test	avg, 2		; ISSE allowed?
		jnz		.isseloop4y

		movq		mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop4y	mov			eax, ecx

	; There are at least 4 ways to 50/50 average:
	; 1) avg = (s0 + s1 + 1) >> 1
	; 2) avg = (s0 | s1) - ((s0 ^ s1) >> 1)
	; 3) avg = (s0 >> 1) + (s1 >> 1) + ((s0 | s1) & 1)
	; 4) pavgb

.loop4x		movq		mm0, [esi]

			movq		mm2, [ebx+esi]
			movq		mm1, mm0

			pxor		mm0, mm2
			por			mm1, mm2

			movq		mm2, [edi]
			psrlq		mm0, 1

			pand		mm0, mm7	; 7F7F7F7F7F7F7F7F

			psubb		mm1, mm0	; mm1 done, mm0 free

			movq		mm0, mm1
			pxor		mm1, mm2

			por			mm0, mm2
			psrlq		mm1, 1

			pand		mm1, mm7	; 7F7F7F7F7F7F7F7F
			add			esi, 8

			psubb		mm0, mm1
			sub			eax, 8

			movq		[edi], mm0

			lea			edi, [edi+8]
			jg			.loop4x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.loop4y

		jmp		.Done

align	16
.isseloop4y
		mov		eax, ecx

.isseloop4x	movq		mm0, [esi]

			pavgb		mm0, [ebx+esi]

			pavgb		mm0, [edi]

			add			esi, 8
			sub			eax, 8

			movq		[edi], mm0

			lea			edi, [edi+8]
			jg			.isseloop4x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.isseloop4y

		jmp		.Done

align 16
.pred_010	; no horizontal but vertical half-pel, no averaging

		sub		edi, 8		; bias edi

		test	avg, 2		; ISSE allowed?
		jnz		.isseloop5y

		movq		mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop5y	mov			eax, ecx

.loop5x		movq		mm0, [esi]

			movq		mm1, [ebx+esi]
			movq		mm2, mm0

			pxor		mm2, mm1
			por			mm0, mm1

			psrlq		mm2, 1
			add			edi, 8

			pand		mm2, mm7	; 7F7F7F7F7F7F7F7F
			add			esi, 8

			psubb		mm0, mm2
			sub			eax, 8

			movq		[edi], mm0
			jg			.loop5x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.loop5y

		jmp		.Done

align	16
.isseloop5y
		mov		eax, ecx

.isseloop5x	movq		mm0, [esi]

			pavgb		mm0, [ebx+esi]

			add			edi, 8
			add			esi, 8
			sub			eax, 8

			movq		[edi], mm0
			jg			.isseloop5x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.isseloop5y

		jmp		.Done

align 16
.pred_001	; no norizontal nor vertical half-pel, with averaging

		test	avg, 2		; ISSE allowed?
		jnz		.isseloop6y

		movq		mm7, [mmx_7F7F7F7F7F7F7F7F]

.loop6y	mov			eax, ecx

.loop6x		movq		mm0, [esi]

			movq		mm1, [edi]
			movq		mm2, mm0

			pxor		mm2, mm1
			por			mm0, mm1

			psrlq		mm2, 1

			pand		mm2, mm7	; 7F7F7F7F7F7F7F7F
			add			esi, 8

			psubb		mm0, mm2
			sub			eax, 8

			movq		[edi], mm0

			lea			edi, [edi+8]
			jg			.loop6x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.loop6y

		jmp		.Done

align	16
.isseloop6y
		mov		eax, ecx

.isseloop6x	movq		mm0, [esi]

			pavgb		mm0, [edi]

			add			esi, 8
			sub			eax, 8

			movq		[edi], mm0

			lea			edi, [edi+8]
			jg			.isseloop6x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.isseloop6y

		jmp		.Done

align 16
.pred_000	; no horizontal nor vertical half-pel, no averaging

.loop7y	mov		eax, ecx

.loop7x		movq		mm0, [esi]

			add			esi, 8
			sub			eax, 8

			movq		[edi], mm0

			lea			edi, [edi+8]
			jg			.loop7x

		add		esi, edx
		add		edi, edx
		dec		ebp
		jg		.loop7y

.Done
		pop		ebp
		pop		ebx
		pop		edi
		pop		esi
		ret


    ; mmx_conv420to422(
    ;   [esp+ 4]    const void* src,        // source chroma buffer
    ;   [esp+ 8]    void*       dst,        // destination chroma buffer
    ;   [esp+12]    int         cpw,        // coded_picture_width
    ;   [esp+16]    int         cph,        // coded_picture_height
    ;   [esp+20]    bool        progressive // is progressive
    ; );

	; The caller is responsible for ensuring that:
	; 1) src and dst are valid pointers
	; 2) cpw is a nonzero multiple of 16
	; 3) cph is a nonzero multiple of 16
	; 4) the src buffer is at least ((cpw / 2) * (cph / 2)) bytes
	; 5) the dst buffer is at least ((cpw / 2) * cph) bytes
	; 6) the MMX state is emptied with emms

PROC _mmx_conv420to422
	push	ebx
	push	esi
	push	edi
	push	ebp

	; Parameters
	%define src         dword [esp+16+1*4]
	%define dst         dword [esp+16+2*4]
	%define cpw         dword [esp+16+3*4]
	%define cph         dword [esp+16+4*4]
	%define progressive dword [esp+16+5*4]

	mov		esi, src
	mov		edi, dst

	test	progressive, 1		; progressive?
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
	shr		cph, 1
	xor		ebp, ebp			; ebp = y loop counter

	shr		cpw, 1
	dec		cph					; we want (cph - 1) in the loop

.syloop
	cmp		ebp, cph			; ebp > (cph - 1)?
	jg		.finished			; break if so

	mov		ebx, cpw			; ebx = coded_picture_width / 2
	xor		ecx, ecx			; ecx = sm1, offset to (src - 1) row

	mov		eax, ebx			; eax = x loop counter
	xor		edx, edx			; edx = sp1, offset to (src + 1) row

    ; The following branches fall through for all iterations but two
	cmp		ebp, ecx			; ebp > 0?
	jle		.sok1
	sub		ecx, ebx			; (sm1 points to the previous line)
.sok1
	cmp		ebp, cph			; ebp < (cph - 1)?
	jge		.sok2
	add		edx, ebx			; (sp1 points to the next line)
.sok2

	; MPEG-1 progressive upsample:
	; (dst+0) = ((src+0) * 3 + (src-1) + 2) >> 2
	; (dst+1) = ((src+0) * 3 + (src+1) + 2) >> 2

.sxloop
	movq		mm0, [esi]		; mm0 = src

	movq		mm2, [esi+ecx]	; mm2 = sm1
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
	movq		mm4, [esi+edx]	; mm4 = sp1

	packuswb	mm2, mm3		; mm2 = [# dst finished #]
	movq		mm5, mm4		; mm5 = copy of sp1

	punpcklbw	mm4, mm7		; mm4 = sp1lo
	add			esi, 8			; increment src pointer

	punpckhbw	mm5, mm7		; mm5 = sp1hi
	paddw		mm0, mm4		; mm0 = srclo * 3 + sp1lo + 2

	psrlw		mm0, 2			; mm0 = [# dp1lo finished #]
	paddw		mm1, mm5		; mm1 = srchi * 3 + sp1hi + 2

	movq		[edi], mm2		; store dst
	psrlw		mm1, 2			; mm1 = [# dp1hi finished #]

	packuswb	mm0, mm1		; mm1 = [# dp1 finished #]
	sub			eax, 8			; decrement counter

	movq		[edi+ebx], mm0	; store dp1

	lea			edi, [edi+8]	; increment dst pointer
	jg			.sxloop

	add			edi, ebx		; advance to next row
	inc			ebp				; y loop counter

	jmp			.syloop

.interlaced

	movq		mm6, [mmx_0002000200020002]

	pxor		mm7, mm7
	paddw		mm6, mm6

	shr			cph, 2				; cph = chroma height / 2
	mov			ebx, cpw			; ebx = chroma width * 2

	dec			cph					; we want (cph - 1) in the loop
	xor			ebp, ebp			; ebp = y loop counter

.myloop
	cmp			ebp, cph			; ebp > (cph - 1)?
	jg			.finished			; break if so

	xor			ecx, ecx            ; sm1 (src minus 1)
	xor			edx, edx            ; sp1 (src plus 1)

	cmp			ebp, ecx
	jle			.mok1
	sub			ecx, ebx
.mok1
	cmp			ebp, cph			; ebp < (cph - 1)?
	jge			.mok2
	add			edx, ebx
.mok2
	mov			eax, ebx

	; top field:
	; (dst+0) = ((src-1) * 1 + (src+0) * 7 + 4) >> 3
	; (dst+1) = ((src+0) * 5 + (src+1) * 3 + 4) >> 3

.mxloop1
	movq		mm0, [esi]		; mm0 = src

    movq        mm2, [esi+ecx]  ; mm2 = sm1
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

    movq        mm0, [esi+edx]  ; mm0 = sp1
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

	movq		[edi], mm2		; store dst
	paddw		mm5, mm1		; mm5 = srchi * 5 + sp1hi * 3

	psrlw		mm4, 3			; mm4 = [# dp1lo finished #]
	paddw		mm5, mm6		; mm5 = srchi * 5 + sp1hi * 3 + 4

	psrlw		mm5, 3			; mm5 = [# dp1hi finished #]
	add			esi, 8

	packuswb	mm4, mm5		; mm4 = [# dp1 finished #]
	sub			eax, 16

	movq		[edi+ebx], mm4

	lea			edi, [edi+8]
    jg          .mxloop1

	; bottom field:
	; (dst+0) = ((src-1) * 3 + (src+0) * 5 + 4) >> 3
	; (dst+1) = ((src+0) * 7 + (src+1) * 1 + 4) >> 3

	mov			eax, ebx

.mxloop2
	movq		mm0, [esi]		; mm0 = src

    movq        mm2, [esi+ecx]  ; mm2 = sm1
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

	movq		mm2, [esi+edx]	; mm2 = sp1
	movq		mm3, mm1		; mm3 = copy of srchi

	psllw		mm1, 3			; mm1 = srchi * 8
	paddw		mm0, mm6		; mm0 = srclo * 7 + 4

	packuswb	mm4, mm5		; mm4 = [# dst finished #]
	psubw		mm1, mm3		; mm1 = srchi * 7

	movq		mm3, mm2		; mm3 = copy of sp1
	punpcklbw	mm2, mm7		; mm2 = sp1lo

	paddw		mm1, mm6		; mm1 = srchi * 7 + 4
	punpckhbw	mm3, mm7		; mm3 = sp1hi
	
	movq		[edi], mm4		; store dst
	paddw		mm0, mm2		; mm0 = srclo * 7 + sp1lo + 4

	psrlw		mm0, 3			; mm0 = [# dp1lo finished #]
	paddw		mm1, mm3		; mm1 = srchi * 7 + sp1hi + 4
	
	psrlw		mm1, 3			; mm1 = [# dp1hi finished #]
	add			esi, 8

	packuswb	mm0, mm1		; [# dp1 finished #]
	sub			eax, 16

	movq		[edi+ebx], mm0	; store dp1
	
	lea			edi, [edi+8]
	jg			.mxloop2

    add			edi, ebx
	inc			ebp

	jmp			.myloop

.finished
	pop		ebp
	pop		edi
	pop		esi
	pop		ebx
	ret



    ; mmx_conv422to444(
    ;   [esp+ 4]    const void* src,    // source chroma buffer
    ;   [esp+ 8]    void*       dst,    // destination chroma buffer
    ;   [esp+12]    int         cpw,    // coded_picture_width
    ;   [esp+16]    int         cph,    // coded_picture_height
    ;   [esp+20]    bool        mpeg2   // is MPEG-2
    ; );

	; The caller is responsible for ensuring that:
	; 1) src and dst are valid pointers
	; 2) cpw is a nonzero multiple of 16
	; 3) cph is a nonzero multiple of 16
	; 4) the src buffer is at least ((cpw / 2) * cph) bytes
	; 5) the dst buffer is at least (cpw * cph) bytes
	; 6) the MMX state is emptied with emms

PROC _mmx_conv422to444

	push	ebx
	push	esi
	push	edi
	push	ebp

	; Parameters
	%define src     dword [esp+16+1*4]
	%define dst     dword [esp+16+2*4]
	%define cpw     dword [esp+16+3*4]
	%define cph     dword [esp+16+4*4]
	%define mpeg2   dword [esp+16+5*4]

	mov		esi, src
	mov		edi, dst

	test	mpeg2, 1		; MPEG-2?
	jnz		.Mpg2

	; MPEG-1 chroma upsampling.

    movq    mm6, [mmx_0002000200020002]
	pxor	mm7, mm7

.syloop
	dec		cph
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

	movq	mm0, [esi]

; Initially, the last pixel of sm1 is the same as the first pixel of src
	movq	mm1, mm0
	psllq	mm0, 56			; mm1 = [0.......]

	mov		ebp, cpw		; ebp = coded_picture_width

	shr		ebp, 4			; ebp = coded_picture_width / 16
	add		esi, 8			; (V-pipe)

.smxloop:
	dec		ebp
	jle		.smxloopdone

	; At the start of each iteration:
	; mm0 contains the prev src sample (we take only its last pixel)
	; mm1 contains the prev sp1 sample [76543210]

	movq		mm2, mm0		; mm2 = prev [76543210]
	movq		mm0, mm1		; rotate prev sp1 into current src

	movq		mm5, mm1		; mm5 = curr [76543210]
	psrlq		mm2, 56			; mm2 = prev [.......7]

	movq		mm1, [esi]		; mm1 = new sp1 sample
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

	movq		[edi], mm2		; store dst, mm2 free
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
	add			esi, 8

	movq		[edi+8], mm2

	add			edi, 16
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

	movq		[edi], mm2		; store dst, mm2 free
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

	movq		[edi+8], mm2

	add			edi, 16
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
	dec			cph
	js			.finished

	movq		mm1, [esi]

	mov			eax, cpw	; eax = coded_picture_width
	add			esi, 8

	sub			eax, 16
	jle			.mxloopdone

	; let mm0 = 76543210
	; let mm1 = FEDCBA98

.mxloop
	movq		mm0, mm1		; prev = current
	movq		mm2, mm1		; mm2 = current

	movq		mm3, [esi]		; fetch next
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
	add			esi, 8

	sub			eax, 16

	movq		[edi], mm3

	movq		[edi+8], mm4

	lea			edi, [edi+16]
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

	movq		[edi], mm3

	movq		[edi+8], mm4

	add			edi, 16
	jmp			.myloop

.finished
	pop		ebp
	pop		edi
	pop		esi
	pop		ebx
	ret



    ; mmx_conv422toYUY2(
    ;   [esp+ 4]    void *srcY,
    ;   [esp+ 8]    void *srcU,
	;	[esp+12]	void *srcV,
	;	[esp+16]	void *dst,
    ;   [esp+20]    int  width,
    ;   [esp+24]    int  height
    ; );

	; The caller is responsible for ensuring that:
	; 1) srcY, srcU, srcV, and dst are valid pointers
	; 2) width is a nonzero multiple of 8 (cpw / 2)
	; 3) height is nonzero
	; 4) all buffers are at least ((cpw / 2) * cph) bytes

PROC _mmx_conv422toYUY2

	push	esi
	push	edi

	; Parameters
	%define srcY    dword [esp+8+1*4]
	%define srcU    dword [esp+8+2*4]
	%define srcV    dword [esp+8+3*4]
	%define dst     dword [esp+8+4*4]
	%define width   dword [esp+8+5*4]
	%define height  dword [esp+8+6*4]

	mov		esi, srcY
	mov		ecx, srcU
	mov		edx, srcV
	mov		edi, dst

.yloop
		mov			eax, width

	; bias pointers
		lea			esi, [esi+eax*2]
		add			ecx, eax
		lea			edi, [edi+eax*4]
		add			edx, eax

		neg			eax

.xloop
		movq		mm2, [ecx+eax]		; U7 U6 U5 U4 U3 U2 U1 U0

		movq		mm3, [edx+eax]		; V7 V6 V5 V4 V3 V2 V1 V0
		movq		mm4, mm2			; U7 U6 U5 U4 U3 U2 U1 U0

		movq		mm5, [esi+eax*2]	; Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
		punpcklbw	mm4, mm3			; V3 U3 V2 U2 V1 U1 V0 U0

		movq		mm0, mm5			; Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
		punpcklbw	mm5, mm4			; V1 Y3 U1 Y2 V0 Y1 U0 Y0 (dst0)

		movq		mm1, [esi+eax*2+8]	; YF YE YD YC YB YA Y9 Y8
		punpckhbw	mm0, mm4			; V3 Y7 U3 Y6 V2 Y5 U2 Y4 (dst1)

		movq		mm4, mm1			; YF YE YD YC YB YA Y9 Y8
		punpckhbw	mm2, mm3			; V7 U7 V6 U6 V5 U5 V4 U4

		movq		[edi+eax*4], mm5
		punpcklbw	mm1, mm2			; V5 YB U5 YA V4 Y9 U4 Y8 (dst2)

		movq		[edi+eax*4+8], mm0
		punpckhbw	mm4, mm2			; V7 YF U7 YE V6 YD U6 YC (dst3)

		movq		[edi+eax*4+16], mm1

		movq		[edi+eax*4+24], mm4

		add			eax, 8
		jl			.xloop

		dec			height
		jg			.yloop

		pop		edi
		pop		esi
		emms
		ret



    ; mmx_conv422toUYVY(
    ;   [esp+ 4]    void *srcY,
    ;   [esp+ 8]    void *srcU,
	;	[esp+12]	void *srcV,
	;	[esp+16]	void *dst,
    ;   [esp+20]    int  width,
    ;   [esp+24]    int  height
    ; );

	; The caller is responsible for ensuring that:
	; 1) srcY, srcU, srcV, and dst are valid pointers
	; 2) width is a nonzero multiple of 8 (cpw / 2)
	; 3) height is nonzero
	; 4) all buffers are at least ((cpw / 2) * cph) bytes

PROC _mmx_conv422toUYVY

	push	esi
	push	edi

	; Parameters
	%define srcY    dword [esp+8+1*4]
	%define srcU    dword [esp+8+2*4]
	%define srcV    dword [esp+8+3*4]
	%define dst     dword [esp+8+4*4]
	%define width   dword [esp+8+5*4]
	%define height  dword [esp+8+6*4]

	mov		esi, srcY
	mov		ecx, srcU
	mov		edx, srcV
	mov		edi, dst

.yloop
		mov			eax, width

	; bias pointers
		lea			esi, [esi+eax*2]
		add			ecx, eax
		lea			edi, [edi+eax*4]
		add			edx, eax

		neg			eax

.xloop
		movq		mm2, [ecx+eax]		; U7 U6 U5 U4 U3 U2 U1 U0

		movq		mm3, [edx+eax]		; V7 V6 V5 V4 V3 V2 V1 V0
		movq		mm4, mm2			; U7 U6 U5 U4 U3 U2 U1 U0

		movq		mm0, [esi+eax*2]	; Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
		punpcklbw	mm4, mm3			; V3 U3 V2 U2 V1 U1 V0 U0

		movq		mm5, mm4			; V3 U3 V2 U2 V1 U1 V0 U0
		punpcklbw	mm4, mm0			; Y3 V1 Y2 U1 Y1 V0 Y0 U0 (dst0)

		movq		mm1, [esi+eax*2+8]	; YF YE YD YC YB YA Y9 Y8
		punpckhbw	mm2, mm3			; V7 U7 V6 U6 V5 U5 V4 U4

		movq		mm4, mm2			; V7 U7 V6 U6 V5 U5 V4 U4
		punpckhbw	mm5, mm0			; Y7 V3 Y6 U3 Y5 V2 Y4 U2 (dst1)

		movq		[edi+eax*4], mm4
		punpcklbw	mm2, mm1			; YB V5 YA U5 Y9 V4 Y8 U4 (dst2)

		movq		[edi+eax*4+8], mm5
		punpckhbw	mm4, mm1			; YF V7 YE U7 YD V6 YC U6 (dst3)

		movq		[edi+eax*4+16], mm2

		movq		[edi+eax*4+24], mm4

		add			eax, 8
		jl			.xloop

		dec			height
		jg			.yloop

		pop		edi
		pop		esi
		emms
		ret

