/*
    Color correction in CIE XYZ color space. Filter for VirtualDub.
	Copyright (C) 2003 Neiromaster

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	The author can be contacted at:
	Neiromaster@mysif.ru
	www.mysif.ru
*/

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <crtdbg.h>
#include <math.h>
#include <stdlib.h>

#include "ScriptInterpreter.h"
#include "ScriptError.h"
#include "ScriptValue.h"

#include "resource.h"
#include "filter.h"

///////////////////////////////////////////////////////////////////////////

int RunProc(const FilterActivation *fa, const FilterFunctions *ff);
int StartProc(FilterActivation *fa, const FilterFunctions *ff);
int EndProc(FilterActivation *fa, const FilterFunctions *ff);
long ParamProc(FilterActivation *fa, const FilterFunctions *ff);
int InitProc(FilterActivation *fa, const FilterFunctions *ff);
int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd);
void StringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str);
void ScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc);
bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen);

///////////////////////////////////////////////////////////////////////////


static const short RGB_to_Lab_matr[12]={ // Матрица преобразования RGB в Lab. /2048.
  455, 1447,  146,    0,
  681,-1024,  343,    0,
  263,  761,-1024,    0,
};

static const short Lab_to_RGB_matr[12]={ // Матрица преобразования Lab в RGB. /2048.
 2048, 4142, 1679,    0,
 2048,-1310, -147,    0,
 2048,   92,-3772,    0,
};

static const __int64	Fin_Okrugl=	0x00000100000001000; // Добавка округления выходных значений.

typedef struct MyFilterData {
	short * L_Gamma; // Указатели на таблицы коррекции гаммы для L,A и B соответственно.
	short * A_Gamma;
	short * B_Gamma;
	short * RGB_Gamma; // Указатель на таблицу предварительной коррекции гаммы.

	IFilterPreview		*ifp;
	int			L_smesch;
	int			A_balans;
	int			B_balans;
	int			A_virt_balans;
	int			B_virt_balans;
	int			L_ampl;
	int			A_pol_ampl;
	int			A_otr_ampl;
	int			B_pol_ampl;
	int			B_otr_ampl;
	int			L_Gam;
	int			A_pol_Gam;
	int			A_otr_Gam;
	int			B_pol_Gam;
	int			B_otr_Gam;
	int			RGB_Gam;
	int			RGB_In_Offs;
	int			RGB_Out_Offs;
	int			use_virtual; // Использовать virual white point.
	void*		mem_blok_uk; // Указатель на выделенный блок памяти.
} MyFilterData;

bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
		mfd->RGB_Gam,
		(mfd->RGB_In_Offs<<16)|(mfd->RGB_Out_Offs&0x0000ffff),
		mfd->L_smesch,
		(mfd->A_balans<<16)|(mfd->B_balans&0x0000ffff),
		mfd->L_ampl,
		mfd->A_pol_ampl,
		mfd->A_otr_ampl,
		mfd->B_pol_ampl,
		mfd->B_otr_ampl,
		mfd->L_Gam,
		mfd->A_pol_Gam,
		mfd->A_otr_Gam,
		mfd->B_pol_Gam,
		mfd->B_otr_Gam,
		(mfd->A_virt_balans<<16)|(mfd->B_virt_balans&0x0000ffff),
		mfd->use_virtual);

	return true;
}

void ScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->RGB_Gam		= argv[0].asInt();

	int temp_zn			= argv[1].asInt();
	mfd->RGB_In_Offs	= temp_zn>>16;
	mfd->RGB_Out_Offs	= (temp_zn<<16)>>16;
	
	mfd->L_smesch		= argv[2].asInt();
	temp_zn				= argv[3].asInt();
	mfd->A_balans		= temp_zn>>16;
	mfd->B_balans		= (temp_zn<<16)>>16;

	mfd->L_ampl			= argv[4].asInt();
	mfd->A_pol_ampl		= argv[5].asInt();
	mfd->A_otr_ampl		= argv[6].asInt();
	mfd->B_pol_ampl		= argv[7].asInt();
	mfd->B_otr_ampl		= argv[8].asInt();
	mfd->L_Gam			= argv[9].asInt();
	mfd->A_pol_Gam		= argv[10].asInt();
	mfd->A_otr_Gam		= argv[11].asInt();
	mfd->B_pol_Gam		= argv[12].asInt();
	mfd->B_otr_Gam		= argv[13].asInt();

	temp_zn				= argv[14].asInt();
	mfd->A_virt_balans	= temp_zn>>16;
	mfd->B_virt_balans	= (temp_zn<<16)>>16;

	mfd->use_virtual	= argv[15].asInt();
}

ScriptFunctionDef func_defs[]={
	{ (ScriptFunctionPtr)ScriptConfig, "Config", "0iiiiiiiiiiiiiiii" },
	{ NULL },
};

CScriptObject script_obj={
	NULL, func_defs
};

struct FilterDefinition filterDef_Lab = {

	NULL, NULL, NULL,		// next, prev, module
	"Lab (1.1)",	// name
	"Color correction in CIE Lab color space." 
	"\n\n[MMX optimized][MMX required]",
							// desc
	"Neiromaster", 		// maker
	NULL,					// private_data
	sizeof(MyFilterData),	// inst_data_size

	InitProc,				// initProc
	NULL,					// deinitProc
	RunProc,				// runProc
	NULL,					// paramProc
	ConfigProc, 			// configProc
	StringProc, 			// stringProc
	StartProc,				// startProc
	EndProc,				// endProc

	&script_obj,			// script_obj
	FssProc,				// fssProc

};

void SetRGB_Gamma(short * uk_gamma,int tek_gamma,int tek_in_offset){
	if(!uk_gamma) return;
	double y, x;
	y=64.0/tek_gamma;
	double sd=tek_in_offset;
	double obr_corr=1/(255.0+tek_in_offset);
	for(int i=0;i<256;i++){
		x = (i+sd) *obr_corr;
		if(x<0)x=0.0;
		uk_gamma[i]=(short)(pow(x,y)*1020.0+0.5);
	}
}

void SetLong_Gamma(short * uk_gamma,int tek_gamma,int skale,int sdvig){
	if(!uk_gamma) return;
	double y, x;
	double sd=sdvig;
	double diap=1020.0+sd;
	double obr_diap=1.0/diap;
	diap=diap*(skale/256.0);
	y=256.0/tek_gamma;
	for(int i=0;i<1021;i++){
		x = (i+sd) * obr_diap;
		if(x<0)x=0.0;
		uk_gamma[i]=(short)(pow(x,y)*diap+0.5);
	}
}

void SetDouble_Gamma(short * uk_gamma,int pol_gamma,int otr_gamma,int pol_skale,int otr_skale,int balans,int virt_balans,int use_virt){
	if(!uk_gamma) return;
	double y_pol,y_otr, x;
	y_pol=256.0/pol_gamma;
	y_otr=256.0/otr_gamma;
	if(use_virt){
		double sd=virt_balans;
		double obr_sd=balans-virt_balans;
		double pol_diap=510.0+sd;
		double obr_pol_diap=1.0/pol_diap;
		double otr_diap=sd-510.0;
		double obr_otr_diap=1.0/otr_diap;
		pol_diap=pol_diap*(pol_skale/256.0);
		otr_diap=otr_diap*(otr_skale/256.0);

		for(int i= -510;i< 511;i++){
			x=(i+sd);
			if(x<0){
				x *= obr_otr_diap;
				uk_gamma[i]= (short)(pow(x,y_otr)*otr_diap-0.5+obr_sd);
			}
			else{
				x *= obr_pol_diap;
				uk_gamma[i]=(short)(pow(x,y_pol)*pol_diap+0.5+obr_sd);
			}
		}
	}
	else{
		double sd=balans;
		double pol_diap=510.0+sd;
		double obr_pol_diap=1.0/pol_diap;
		double otr_diap=sd-510.0;
		double obr_otr_diap=1.0/otr_diap;
		pol_diap=pol_diap*(pol_skale/256.0);
		otr_diap=otr_diap*(otr_skale/256.0);

		for(int i= -510;i< 511;i++){
			x=(i+sd);
			if(x<0){
				x *= obr_otr_diap;
				uk_gamma[i]= (short)(pow(x,y_otr)*otr_diap-0.5);
			}
			else{
				x *= obr_pol_diap;
				uk_gamma[i]=(short)(pow(x,y_pol)*pol_diap+0.5);
			}
		}
	}
}


int StartProc(FilterActivation *fa, const FilterFunctions *ff)
{
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	
	int n=256*2+ 2*1024*3;
	if (mfd->mem_blok_uk) { VirtualFree(mfd->mem_blok_uk, 0, MEM_RELEASE); mfd->mem_blok_uk = NULL; }
	mfd->mem_blok_uk = VirtualAlloc(NULL, n, MEM_COMMIT, PAGE_READWRITE);
	memset(mfd->mem_blok_uk, 0, n);
	mfd->RGB_Gamma=((short*)mfd->mem_blok_uk);
	mfd->L_Gamma=((short*)mfd->mem_blok_uk)+256;
	mfd->A_Gamma=((short*)mfd->mem_blok_uk)+768+1024;
	mfd->B_Gamma=((short*)mfd->mem_blok_uk)+768+2048;
	SetRGB_Gamma(mfd->RGB_Gamma,mfd->RGB_Gam,mfd->RGB_In_Offs);
	SetLong_Gamma(mfd->L_Gamma,mfd->L_Gam,mfd->L_ampl,mfd->L_smesch);
	SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
	SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
	
	return 0;
}

int EndProc(FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	if (mfd->mem_blok_uk) { VirtualFree(mfd->mem_blok_uk, 0, MEM_RELEASE); mfd->mem_blok_uk = NULL; }
	mfd->RGB_Gamma=NULL;
	mfd->L_Gamma=NULL;
	mfd->A_Gamma=NULL;
	mfd->B_Gamma=NULL;

	return 0;
}

///////////////////////////////////////////////////////////////////////////


int RunProc(const FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	const PixDim	width = fa->src.w;
	const PixDim	height = fa->src.h;
	Pixel32 *src, *dst;
	int y;

	src = fa->src.data;
	dst = fa->dst.data;
	short * L_Gamma=mfd->L_Gamma;
	short * A_Gamma=mfd->A_Gamma;
	short * B_Gamma=mfd->B_Gamma;
	short * RGB_Gamma=mfd->RGB_Gamma;

	int tek_out_offs=mfd->RGB_Out_Offs;
	__int64 okrugl_and_sdvig;  // Хранит добавку округления + виходной сдвиг.
	__asm{
		mov		eax,tek_out_offs
		shl		eax,12
		movd	mm6,eax
		punpckldq	mm6,mm6
		paddd	mm6,Fin_Okrugl
		movq	okrugl_and_sdvig,mm6
		emms
	}

	y=height;
	do{
		__asm{
			mov		eax,width
			mov		edi,src
			push	eax
			mov		esi,dst
			movq	mm6,okrugl_and_sdvig
			pxor	mm7,mm7
	X_LOOP:
			mov		eax,RGB_Gamma
			movzx	ecx,BYTE PTR[edi+1]		// G
			movzx	ebx,BYTE PTR[edi+2]		// R
			movzx	edx,BYTE PTR[edi]		// B
			mov		cx,word ptr[eax+ecx*2]		
			mov		bx,word ptr[eax+ebx*2]		
			shl		ecx,16
			mov		dx,word ptr[eax+edx*2] //B
			mov		cx,bx // R+G
			movd	mm1,edx
			movd	mm0,ecx
			punpckldq	mm0,mm1 // В mm0 0|B|G|R|
			movq	mm2,mm0
			pmaddwd	mm0,RGB_to_Lab_matr
			movq	mm3,mm2
			pmaddwd	mm2,RGB_to_Lab_matr+8
			movq	mm1,mm0
			psrlq	mm0,32
			pmaddwd	mm3,RGB_to_Lab_matr+16
			paddd	mm0,mm1 //В mm0 L
			movq	mm4,mm2
			mov		eax,L_Gamma
			psrlq	mm2,32
			movd	ebx,mm0
			movq	mm5,mm3
			psrlq	mm3,32
			add		ebx,1024 // Добавляем значение округления.
			paddd	mm2,mm4 //В mm2 A
			paddd	mm3,mm5 //В mm3 B
			sar		ebx,11 // Нормируем значение.
			movd	ecx,mm2
			movd	edx,mm3
			movzx	ebx,word ptr[eax+ebx*2] //В ebx обработанный L
			mov		eax,A_Gamma
			add		ecx,1024 // Добавляем значение округления.
			add		edx,1024 // Добавляем значение округления.
			sar		ecx,11 // Нормируем значение.
			sar		edx,11 // Нормируем значение.
			movzx	ecx,word ptr[eax+ecx*2] //В ecx обработанный A
			movzx	edx,word ptr[eax+edx*2+2048] //В edx обработанный B
			shl		ecx,16
			mov		cx,bx
			movd	mm1,edx
			movd	mm0,ecx
			punpckldq	mm0,mm1 // В mm0 0|B|A|L|

			movq	mm2,mm0
			pmaddwd	mm0,Lab_to_RGB_matr // R матр
			movq	mm3,mm2
			pmaddwd	mm2,Lab_to_RGB_matr+16 // B матр
			movq	mm1,mm0
			psrlq	mm0,32
			pmaddwd	mm3,Lab_to_RGB_matr+8 // G матр
			paddd	mm0,mm1 //В mm0 B
			movq	mm4,mm2
			punpckldq	mm2,mm3
			punpckhdq	mm4,mm3
			paddd	mm0,mm6 // Добавка округления + сдвиг
			paddd	mm2,mm4 //В mm2 R+G
			paddd	mm2,mm6 // Добавка округления + сдвиг
			psrad	mm0,13
			psrad	mm2,13
			packssdw	mm2,mm0
			packuswb	mm2,mm7 // В младших разрядах mm2 0|R|G|B
			movd	[esi],mm2
			add		edi,4
			add		esi,4

			dec		dword ptr[esp]
			jnz	X_LOOP
			pop		eax
			emms
		}
		src	= (Pixel *)((char *)src + fa->src.pitch);
		dst	= (Pixel *)((char *)dst + fa->dst.pitch);
	}while(--y);

	return 0;
}

extern "C" int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat);
extern "C" void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff);

static FilterDefinition *fd_tutorial;

int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat) {
	if (!(fd_tutorial = ff->addFilter(fm, &filterDef_Lab, sizeof(FilterDefinition))))
		return 1;

	vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
	vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

	return 0;
}

void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff) {
	ff->removeFilter(fd_tutorial);
}

int InitProc(FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	
	mfd->L_Gamma=NULL;
	mfd->A_Gamma=NULL;
	mfd->B_Gamma=NULL;
	mfd->RGB_Gamma=NULL;
	mfd->L_smesch=0;
	mfd->A_balans=0;
	mfd->B_balans=0;
	mfd->L_ampl=256; // 1
	mfd->A_pol_ampl=256;
	mfd->A_otr_ampl=256;
	mfd->B_pol_ampl=256;
	mfd->B_otr_ampl=256;
	mfd->L_Gam=256; // 1
	mfd->A_pol_Gam=256;
	mfd->A_otr_Gam=256;
	mfd->B_pol_Gam=256;
	mfd->B_otr_Gam=256;
	mfd->RGB_Gam=64;
	mfd->RGB_In_Offs= 0;
	mfd->RGB_Out_Offs= 0;
	mfd->A_virt_balans=0;
	mfd->B_virt_balans=0;
	mfd->use_virtual=0;
	mfd->mem_blok_uk=NULL;

	return 0;
}

BOOL CALLBACK ConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	MyFilterData *mfd = (MyFilterData *)GetWindowLong(hdlg, DWL_USER);

	switch(msg) {
        char buf[10];
		case WM_INITDIALOG:
			{
			HWND hWnd;

			SetWindowLong(hdlg, DWL_USER, lParam);
			mfd = (MyFilterData *)lParam;

			hWnd = GetDlgItem(hdlg, IDC_RGB_G);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(4, 192));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->RGB_Gam);
			sprintf(buf, "%2.2f", mfd->RGB_Gam/64.0);
			SetWindowText(GetDlgItem(hdlg, IDC_RGB_G_TEXT), buf);

			hWnd = GetDlgItem(hdlg, IDC_RGB_IN_SM);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 128));
			SendMessage(hWnd, TBM_SETTICFREQ, 8 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->RGB_In_Offs+64);
			sprintf(buf, "%d", mfd->RGB_In_Offs);
			SetWindowText(GetDlgItem(hdlg, IDC_RGB_IN_SM_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_RGB_OUT_SM);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 128));
			SendMessage(hWnd, TBM_SETTICFREQ, 8 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->RGB_Out_Offs+64);
			sprintf(buf, "%d", mfd->RGB_Out_Offs);
			SetWindowText(GetDlgItem(hdlg, IDC_RGB_OUT_SM_TEXT), buf);


			hWnd = GetDlgItem(hdlg, IDC_L_G);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(16, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->L_Gam);
			sprintf(buf, "%2.2f", mfd->L_Gam/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_L_G_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_POL_A_G);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(16, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->A_pol_Gam);
			sprintf(buf, "%2.2f", mfd->A_pol_Gam/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_POL_A_G_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_OTR_A_G);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(16, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->A_otr_Gam);
			sprintf(buf, "%2.2f", mfd->A_otr_Gam/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_OTR_A_G_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_POL_B_G);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(16, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->B_pol_Gam);
			sprintf(buf, "%2.2f", mfd->B_pol_Gam/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_POL_B_G_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_OTR_B_G);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(16, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->B_otr_Gam);
			sprintf(buf, "%2.2f", mfd->B_otr_Gam/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_OTR_B_G_TEXT), buf);

			hWnd = GetDlgItem(hdlg, IDC_L_AMP);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 32 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->L_ampl);
			sprintf(buf, "%2.2f", mfd->L_ampl/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_L_AMP_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_POL_A_AMP);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 32 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->A_pol_ampl);
			sprintf(buf, "%2.2f", mfd->A_pol_ampl/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_POL_A_AMP_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_OTR_A_AMP);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 32 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->A_otr_ampl);
			sprintf(buf, "%2.2f", mfd->A_otr_ampl/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_OTR_A_AMP_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_POL_B_AMP);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 32 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->B_pol_ampl);
			sprintf(buf, "%2.2f", mfd->B_pol_ampl/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_POL_B_AMP_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_OTR_B_AMP);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 512));
			SendMessage(hWnd, TBM_SETTICFREQ, 32 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->B_otr_ampl);
			sprintf(buf, "%2.2f", mfd->B_otr_ampl/256.0);
			SetWindowText(GetDlgItem(hdlg, IDC_OTR_B_AMP_TEXT), buf);

			hWnd = GetDlgItem(hdlg, IDC_L_SM);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 128));
			SendMessage(hWnd, TBM_SETTICFREQ, 8 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->L_smesch+64);
			sprintf(buf, "%d", mfd->L_smesch);
			SetWindowText(GetDlgItem(hdlg, IDC_L_SM_TEXT), buf);
			
			hWnd = GetDlgItem(hdlg, IDC_A_BALANCE);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 256));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->A_balans+128);
			sprintf(buf, "%d", mfd->A_balans);
			SetWindowText(GetDlgItem(hdlg, IDC_A_BALANCE_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_B_BALANCE);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 256));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->B_balans+128);
			sprintf(buf, "%d", mfd->B_balans);
			SetWindowText(GetDlgItem(hdlg, IDC_B_BALANCE_TEXT), buf);
			
			
			hWnd = GetDlgItem(hdlg, IDC_A_VIRT_BALANCE);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 256));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->A_virt_balans+128);
			sprintf(buf, "%d", mfd->A_virt_balans);
			SetWindowText(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE_TEXT), buf);
			hWnd = GetDlgItem(hdlg, IDC_B_VIRT_BALANCE);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 256));
			SendMessage(hWnd, TBM_SETTICFREQ, 16 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->B_virt_balans+128);
			sprintf(buf, "%d", mfd->B_virt_balans);
			SetWindowText(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE_TEXT), buf);
			
			if (mfd->use_virtual) CheckDlgButton(hdlg, IDC_USE_VIRT, BST_CHECKED);
			BOOL enable = mfd->use_virtual;
			EnableWindow(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE), enable);
			EnableWindow(GetDlgItem(hdlg, IDC_ZERO_A_VIRT_BALANCE), enable);
			EnableWindow(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE_TEXT), enable);
			EnableWindow(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE_LABEL), enable);					
			EnableWindow(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE), enable);
			EnableWindow(GetDlgItem(hdlg, IDC_ZERO_B_VIRT_BALANCE), enable);
			EnableWindow(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE_TEXT), enable);
			EnableWindow(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE_LABEL), enable);
			
			mfd->ifp->InitButton(GetDlgItem(hdlg, IDPREVIEW));

			return TRUE;
			}
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDPREVIEW:
				mfd->ifp->Toggle(hdlg);
				break;
			case IDOK:
				EndDialog(hdlg, 0);
				return TRUE;
			case IDC_USE_VIRT: 
				{
					BOOL enable = IsDlgButtonChecked(hdlg, IDC_USE_VIRT)==BST_CHECKED;
					EnableWindow(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE), enable);
					EnableWindow(GetDlgItem(hdlg, IDC_ZERO_A_VIRT_BALANCE), enable);
					EnableWindow(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE_TEXT), enable);
					EnableWindow(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE_LABEL), enable);					
					EnableWindow(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE), enable);
					EnableWindow(GetDlgItem(hdlg, IDC_ZERO_B_VIRT_BALANCE), enable);
					EnableWindow(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE_TEXT), enable);
					EnableWindow(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE_LABEL), enable);
					mfd->use_virtual=enable;
					SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
					SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
					return TRUE;
				}
			/*case IDHELP:
				{
				char prog[256];
				char path[256];
				LPTSTR ptr;
				GetModuleFileName(NULL, prog, 255);
				GetFullPathName(prog, 255, path, &ptr);
				*ptr = 0;
				strcat(path, "plugins\\Lab.txt");
				OutputDebugString(path);
				OutputDebugString("\n");
				strcpy(prog, "Notepad ");
				strcat(prog, path);
				WinExec(prog, SW_SHOW);
				return TRUE;
				}*/
			case IDCANCEL:
				EndDialog(hdlg, 1);
				return TRUE;
			case IDC_ZERO_RGB_G:
				SendMessage(GetDlgItem(hdlg, IDC_RGB_G), TBM_SETPOS, (WPARAM)TRUE, 64);
				mfd->RGB_Gam = 64;
				sprintf(buf, "%2.2f", mfd->RGB_Gam/64.0);
				SetWindowText(GetDlgItem(hdlg, IDC_RGB_G_TEXT), buf);
				SetRGB_Gamma(mfd->RGB_Gamma,mfd->RGB_Gam,mfd->RGB_In_Offs);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_L_G:
				SendMessage(GetDlgItem(hdlg, IDC_L_G), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->L_Gam = 256;
				sprintf(buf, "%2.2f", mfd->L_Gam/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_L_G_TEXT), buf);
				SetLong_Gamma(mfd->L_Gamma,mfd->L_Gam,mfd->L_ampl,mfd->L_smesch);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_POL_A_G:
				SendMessage(GetDlgItem(hdlg, IDC_POL_A_G), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->A_pol_Gam = 256;
				sprintf(buf, "%2.2f", mfd->A_pol_Gam/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_POL_A_G_TEXT), buf);
				SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_OTR_A_G:
				SendMessage(GetDlgItem(hdlg, IDC_OTR_A_G), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->A_otr_Gam = 256;
				sprintf(buf, "%2.2f", mfd->A_otr_Gam/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_OTR_A_G_TEXT), buf);
				SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_POL_B_G:
				SendMessage(GetDlgItem(hdlg, IDC_POL_B_G), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->B_pol_Gam = 256;
				sprintf(buf, "%2.2f", mfd->B_pol_Gam/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_POL_B_G_TEXT), buf);
				SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_OTR_B_G:
				SendMessage(GetDlgItem(hdlg, IDC_OTR_B_G), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->B_otr_Gam = 256;
				sprintf(buf, "%2.2f", mfd->B_otr_Gam/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_OTR_B_G_TEXT), buf);
				SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_L_AMP:
				SendMessage(GetDlgItem(hdlg, IDC_L_AMP), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->L_ampl = 256;
				sprintf(buf, "%2.2f", mfd->L_ampl/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_L_AMP_TEXT), buf);
				SetLong_Gamma(mfd->L_Gamma,mfd->L_Gam,mfd->L_ampl,mfd->L_smesch);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_POL_A_AMP:
				SendMessage(GetDlgItem(hdlg, IDC_POL_A_AMP), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->A_pol_ampl = 256;
				sprintf(buf, "%2.2f", mfd->A_pol_ampl/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_POL_A_AMP_TEXT), buf);
				SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_OTR_A_AMP:
				SendMessage(GetDlgItem(hdlg, IDC_OTR_A_AMP), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->A_otr_ampl = 256;
				sprintf(buf, "%2.2f", mfd->A_otr_ampl/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_OTR_A_AMP_TEXT), buf);
				SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_POL_B_AMP:
				SendMessage(GetDlgItem(hdlg, IDC_POL_B_AMP), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->B_pol_ampl = 256;
				sprintf(buf, "%2.2f", mfd->B_pol_ampl/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_POL_B_AMP_TEXT), buf);
				SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_OTR_B_AMP:
				SendMessage(GetDlgItem(hdlg, IDC_OTR_B_AMP), TBM_SETPOS, (WPARAM)TRUE, 256);
				mfd->B_otr_ampl = 256;
				sprintf(buf, "%2.2f", mfd->B_otr_ampl/256.0);
				SetWindowText(GetDlgItem(hdlg, IDC_OTR_B_AMP_TEXT), buf);
				SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;

			case IDC_ZERO_RGB_IN_SM:
				SendMessage(GetDlgItem(hdlg, IDC_RGB_IN_SM), TBM_SETPOS, (WPARAM)TRUE, 64);
				mfd->RGB_In_Offs = 0;
				sprintf(buf, "%d", mfd->RGB_In_Offs);
				SetWindowText(GetDlgItem(hdlg, IDC_RGB_IN_SM_TEXT), buf);
				SetRGB_Gamma(mfd->RGB_Gamma,mfd->RGB_Gam,mfd->RGB_In_Offs);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_RGB_OUT_SM:
				SendMessage(GetDlgItem(hdlg, IDC_RGB_OUT_SM), TBM_SETPOS, (WPARAM)TRUE, 64);
				mfd->RGB_Out_Offs = 0;
				sprintf(buf, "%d", mfd->RGB_Out_Offs);
				SetWindowText(GetDlgItem(hdlg, IDC_RGB_OUT_SM_TEXT), buf);
				mfd->ifp->RedoFrame();
				return TRUE;

			case IDC_ZERO_L_SM:
				SendMessage(GetDlgItem(hdlg, IDC_L_SM), TBM_SETPOS, (WPARAM)TRUE, 64);
				mfd->L_smesch = 0;
				sprintf(buf, "%d", mfd->L_smesch);
				SetWindowText(GetDlgItem(hdlg, IDC_L_SM_TEXT), buf);
				SetLong_Gamma(mfd->L_Gamma,mfd->L_Gam,mfd->L_ampl,mfd->L_smesch);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_A_BALANCE:
				SendMessage(GetDlgItem(hdlg, IDC_A_BALANCE), TBM_SETPOS, (WPARAM)TRUE, 128);
				mfd->A_balans = 0;
				sprintf(buf, "%d", mfd->A_balans);
				SetWindowText(GetDlgItem(hdlg, IDC_A_BALANCE_TEXT), buf);
				SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_B_BALANCE:
				SendMessage(GetDlgItem(hdlg, IDC_B_BALANCE), TBM_SETPOS, (WPARAM)TRUE, 128);
				mfd->B_balans = 0;
				sprintf(buf, "%d", mfd->B_balans);
				SetWindowText(GetDlgItem(hdlg, IDC_B_BALANCE_TEXT), buf);
				SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_A_VIRT_BALANCE:
				SendMessage(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE), TBM_SETPOS, (WPARAM)TRUE, 128);
				mfd->A_virt_balans = 0;
				sprintf(buf, "%d", mfd->A_virt_balans);
				SetWindowText(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE_TEXT), buf);
				SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZERO_B_VIRT_BALANCE:
				SendMessage(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE), TBM_SETPOS, (WPARAM)TRUE, 128);
				mfd->B_virt_balans = 0;
				sprintf(buf, "%d", mfd->B_virt_balans);
				SetWindowText(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE_TEXT), buf);
				SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
				mfd->ifp->RedoFrame();
				return TRUE;
				break;
			}
			break;
		case WM_HSCROLL:
			{
				int n;
				HWND hwnd = GetDlgItem(hdlg, IDC_RGB_G);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->RGB_Gam){
					mfd->RGB_Gam=n;
					sprintf(buf, "%2.2f", mfd->RGB_Gam/64.0);
					SetWindowText(GetDlgItem(hdlg, IDC_RGB_G_TEXT), buf);
					SetRGB_Gamma(mfd->RGB_Gamma,mfd->RGB_Gam,mfd->RGB_In_Offs);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_L_G);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->L_Gam){
					mfd->L_Gam=n;
					sprintf(buf, "%2.2f", mfd->L_Gam/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_L_G_TEXT), buf);
					SetLong_Gamma(mfd->L_Gamma,mfd->L_Gam,mfd->L_ampl,mfd->L_smesch);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_POL_A_G);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->A_pol_Gam){
					mfd->A_pol_Gam=n;
					sprintf(buf, "%2.2f", mfd->A_pol_Gam/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_POL_A_G_TEXT), buf);
					SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_OTR_A_G);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->A_otr_Gam){
					mfd->A_otr_Gam=n;
					sprintf(buf, "%2.2f", mfd->A_otr_Gam/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_OTR_A_G_TEXT), buf);
					SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_POL_B_G);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->B_pol_Gam){
					mfd->B_pol_Gam=n;
					sprintf(buf, "%2.2f", mfd->B_pol_Gam/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_POL_B_G_TEXT), buf);
					SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_OTR_B_G);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->B_otr_Gam){
					mfd->B_otr_Gam=n;
					sprintf(buf, "%2.2f", mfd->B_otr_Gam/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_OTR_B_G_TEXT), buf);
					SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_L_AMP);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->L_ampl){
					mfd->L_ampl=n;
					sprintf(buf, "%2.2f", mfd->L_ampl/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_L_AMP_TEXT), buf);
					SetLong_Gamma(mfd->L_Gamma,mfd->L_Gam,mfd->L_ampl,mfd->L_smesch);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_POL_A_AMP);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->A_pol_ampl){
					mfd->A_pol_ampl=n;
					sprintf(buf, "%2.2f", mfd->A_pol_ampl/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_POL_A_AMP_TEXT), buf);
					SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_OTR_A_AMP);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->A_otr_ampl){
					mfd->A_otr_ampl=n;
					sprintf(buf, "%2.2f", mfd->A_otr_ampl/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_OTR_A_AMP_TEXT), buf);
					SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_POL_B_AMP);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->B_pol_ampl){
					mfd->B_pol_ampl=n;
					sprintf(buf, "%2.2f", mfd->B_pol_ampl/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_POL_B_AMP_TEXT), buf);
					SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_OTR_B_AMP);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0);
				if(n!=mfd->B_otr_ampl){
					mfd->B_otr_ampl=n;
					sprintf(buf, "%2.2f", mfd->B_otr_ampl/256.0);
					SetWindowText(GetDlgItem(hdlg, IDC_OTR_B_AMP_TEXT), buf);
					SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_RGB_IN_SM);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0)-64;
				if(n!=mfd->RGB_In_Offs){
					mfd->RGB_In_Offs=n;
					sprintf(buf, "%d", mfd->RGB_In_Offs);
					SetWindowText(GetDlgItem(hdlg, IDC_RGB_IN_SM_TEXT), buf);
					SetRGB_Gamma(mfd->RGB_Gamma,mfd->RGB_Gam,mfd->RGB_In_Offs);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_RGB_OUT_SM);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0)-64;
				if(n!=mfd->RGB_Out_Offs){
					mfd->RGB_Out_Offs=n;
					sprintf(buf, "%d", mfd->RGB_Out_Offs);
					SetWindowText(GetDlgItem(hdlg, IDC_RGB_OUT_SM_TEXT), buf);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_L_SM);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0)-64;
				if(n!=mfd->L_smesch){
					mfd->L_smesch=n;
					sprintf(buf, "%d", mfd->L_smesch);
					SetWindowText(GetDlgItem(hdlg, IDC_L_SM_TEXT), buf);
					SetLong_Gamma(mfd->L_Gamma,mfd->L_Gam,mfd->L_ampl,mfd->L_smesch);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_A_BALANCE);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0)-128;
				if(n!=mfd->A_balans){
					mfd->A_balans=n;
					sprintf(buf, "%d", mfd->A_balans);
					SetWindowText(GetDlgItem(hdlg, IDC_A_BALANCE_TEXT), buf);
					SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_B_BALANCE);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0)-128;
				if(n!=mfd->B_balans){
					mfd->B_balans=n;
					sprintf(buf, "%d", mfd->B_balans);
					SetWindowText(GetDlgItem(hdlg, IDC_B_BALANCE_TEXT), buf);
					SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_A_VIRT_BALANCE);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0)-128;
				if(n!=mfd->A_virt_balans){
					mfd->A_virt_balans=n;
					sprintf(buf, "%d", mfd->A_virt_balans);
					SetWindowText(GetDlgItem(hdlg, IDC_A_VIRT_BALANCE_TEXT), buf);
					SetDouble_Gamma(mfd->A_Gamma,mfd->A_pol_Gam,mfd->A_otr_Gam,mfd->A_pol_ampl,mfd->A_otr_ampl,mfd->A_balans,mfd->A_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
				hwnd = GetDlgItem(hdlg, IDC_B_VIRT_BALANCE);
				n = SendMessage(hwnd, TBM_GETPOS, 0, 0)-128;
				if(n!=mfd->B_virt_balans){
					mfd->B_virt_balans=n;
					sprintf(buf, "%d", mfd->B_virt_balans);
					SetWindowText(GetDlgItem(hdlg, IDC_B_VIRT_BALANCE_TEXT), buf);
					SetDouble_Gamma(mfd->B_Gamma,mfd->B_pol_Gam,mfd->B_otr_Gam,mfd->B_pol_ampl,mfd->B_otr_ampl,mfd->B_balans,mfd->B_virt_balans,mfd->use_virtual);
					mfd->ifp->RedoFrame();
				}
			}
			break;
	}

	return FALSE;
}

int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd)
{
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	MyFilterData mfd_old = *mfd;
	int ret;

	mfd->ifp = fa->ifp;

	if (DialogBoxParam(fa->filter->module->hInstModule, MAKEINTRESOURCE(IDD_FILTER),
		hwnd, ConfigDlgProc, (LPARAM)mfd))
	{
		*mfd = mfd_old;
		ret = TRUE;
	}
    else
	{
		ret = FALSE;
	}
	return(ret);
}

void StringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str)
{
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	sprintf(str, "(%.2f, L(%.2f,%.2f,%d),A(%.2f,%.2f,%.2f,%.2f, %d),B(%.2f,%.2f,%.2f,%.2f,%d))",
				mfd->RGB_Gam/64.0, mfd->L_Gam/256.0,mfd->L_ampl/256.0,mfd->L_smesch,
				 mfd->A_pol_Gam/256.0,mfd->A_otr_Gam/256.0,mfd->A_pol_ampl/256.0,mfd->A_otr_ampl/256.0,mfd->A_balans,
				  mfd->B_pol_Gam/256.0,mfd->B_otr_Gam/256.0,mfd->B_pol_ampl/256.0,mfd->B_otr_ampl/256.0,mfd->B_balans);
}
