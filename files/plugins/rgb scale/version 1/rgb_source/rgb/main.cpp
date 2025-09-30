/*
    Red/Green/Ble (RGB) Adjustment Filter for VirtualDub.
    Copyright (C) 1999-2000 Donald A. Graft

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
	Donald Graft
	neuron2@home.com.
*/

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <crtdbg.h>
#include <math.h>
#include <stdlib.h>

#include "plugin\vdvideofilt.h"

#include "resource.h"
#include <emmintrin.h>

HINSTANCE hInstance;

///////////////////////////////////////////////////////////////////////////

int RunProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int StartProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int EndProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
long ParamProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int InitProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int ConfigProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hwnd);
void StringProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *str);
void ScriptConfig(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc);
bool FssProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen);

///////////////////////////////////////////////////////////////////////////

typedef struct MyFilterData {
	IVDXFilterPreview		*ifp;
	int					red;
	int					green;
	int					blue;
} MyFilterData;

bool FssProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d, %d, %d)",
		mfd->red,
		mfd->green,
		mfd->blue);

	return true;
}

void ScriptConfig(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
	VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->red			= argv[0].asInt();
	mfd->green			= argv[1].asInt();
	mfd->blue			= argv[2].asInt();
}

VDXScriptFunctionDef func_defs[]={
	{ (VDXScriptFunctionPtr)ScriptConfig, "Config", "0iii" },
	{ NULL },
};

VDXScriptObject script_obj={
	NULL, func_defs
};

struct VDXFilterDefinition filterDef = {

	NULL, NULL, NULL,		// next, prev, module
	"rgb scale",	// name
	"Adjust red, green, blue.",
							// desc
	"Donald Graft", 		// maker
	NULL,					// private_data
	sizeof(MyFilterData),	// inst_data_size

	InitProc,				// initProc
	NULL,					// deinitProc
	RunProc,				// runProc
	ParamProc,				// paramProc
	ConfigProc, 			// configProc
	StringProc, 			// stringProc
	StartProc,				// startProc
	EndProc,				// endProc

	&script_obj,			// script_obj
	FssProc,				// fssProc

};

long ParamProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff)
{
  MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	pxldst.pitch = pxlsrc.pitch;

	if(pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_ALIGN_SCANLINES;
}

int StartProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff)
{
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	return 0;
}

int EndProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff)
{
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	return 0;
}

///////////////////////////////////////////////////////////////////////////

/*
int RunProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff)
{
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	int	width = fa->src.w;
	int	height = fa->src.h;
	unsigned int *src, *dst;
	int x, y;
	long r, g, b;

	src = fa->src.data;
	dst = fa->dst.data;
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			r = ((src[x] & 0xff0000) >> 16);
			g = ((src[x] & 0xff00) >> 8);
			b = (src[x] & 0xff);
			r = ((mfd->red + 100) * r) / 100;
			if (r > 255) r = 255;
			g = ((mfd->green + 100) * g) / 100;
			if (g > 255) g = 255;
			b = ((mfd->blue + 100) * b) / 100;
			if (b > 255) b = 255;
			dst[x] = ((int) r << 16) | ((int) g << 8) | ((int) b);			 
		}
		src	= (unsigned int *)((char *)src + fa->src.pitch);
		dst	= (unsigned int *)((char *)dst + fa->dst.pitch);
	}
	return 0;
}
*/

int RunProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff)
{
  MyFilterData *mfd = (MyFilterData *)fa->filter_data;
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  int w = (src.w+3)/4;
  int h = src.h;

  int mr = (mfd->red + 100)*0x1000/100;
  int mg = (mfd->green + 100)*0x1000/100;
  int mb = (mfd->blue + 100)*0x1000/100;
  int ma = 0x1000;

  const __m128i zero = _mm_setzero_si128();
  const __m128i constm = _mm_set_epi16(ma,mr,mg,mb,ma,mr,mg,mb);

  {for(int y=0; y<h; y++){
    const unsigned char* s = (const unsigned char*)src.data + src.pitch*y;
    unsigned char* d = (unsigned char*)dst.data + dst.pitch*y;

    {for(int x=0; x<w; x++){
      __m128i c0 = _mm_load_si128((__m128i*)s);

      __m128i v0 = _mm_unpacklo_epi8(c0,zero);
      __m128i v1 = _mm_unpackhi_epi8(c0,zero);
      v0 = _mm_slli_epi16(v0,4);
      v1 = _mm_slli_epi16(v1,4);
      v0 = _mm_mulhi_epu16(v0,constm);
      v1 = _mm_mulhi_epu16(v1,constm);

      c0 = _mm_packus_epi16(v0,v1);
      _mm_store_si128((__m128i*)d,c0);
      d+=16;
      s+=16;
    }}
  }}

  return 0;
}


extern "C" int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(VDXFilterModule *fm, const VDXFilterFunctions *ff, int& vdfd_ver, int& vdfd_compat) {
	ff->addFilter(fm, &filterDef, sizeof(filterDef));

	vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
	vdfd_compat = 9;

	return 0;
}

extern "C" void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(VDXFilterModule *fm, const VDXFilterFunctions *ff) {
}

int InitProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->red = 0;
	mfd->green = 0;
	mfd->blue = 0;
	return 0;
}

const UINT WM_EDIT_CHANGED = WM_USER+666;

void edit_changed(HWND wnd)
{
  SendMessage(GetParent(wnd),WM_EDIT_CHANGED,0,(LPARAM)wnd);
}

LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
  if(msg==WM_KEYDOWN){
    if(wparam==VK_RETURN){
      edit_changed(wnd);
      return 1;
    }
  }
  if(msg==WM_KILLFOCUS) edit_changed(wnd);

  WNDPROC p = (WNDPROC)GetWindowLongPtr(wnd,GWLP_USERDATA);
  return CallWindowProc(p,wnd,msg,wparam,lparam);
}

INT_PTR CALLBACK ConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	MyFilterData *mfd = (MyFilterData *)GetWindowLongPtr(hdlg, DWLP_USER);

	switch(msg) {
		case WM_INITDIALOG:
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);
			mfd = (MyFilterData *)lParam;
			HWND hWnd;

			hWnd = GetDlgItem(hdlg, IDC_RED);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 300));
			SendMessage(hWnd, TBM_SETTICFREQ, 10 , 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->red+100);
			SetDlgItemInt(hdlg, IDC_REDVAL, mfd->red+100, FALSE);
			hWnd = GetDlgItem(hdlg, IDC_GREEN);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 300));
			SendMessage(hWnd, TBM_SETTICFREQ, 10, 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->green+100);
			SetDlgItemInt(hdlg, IDC_GREENVAL, mfd->green+100, FALSE);
			hWnd = GetDlgItem(hdlg, IDC_BLUE);
			SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 300));
			SendMessage(hWnd, TBM_SETTICFREQ, 10, 0);
			SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->blue+100);
			SetDlgItemInt(hdlg, IDC_BLUEVAL, mfd->blue+100, FALSE);
			mfd->ifp->InitButton((VDXHWND)GetDlgItem(hdlg, IDPREVIEW));

      LPARAM p;
      hWnd = GetDlgItem(hdlg,IDC_REDVAL);
      p = GetWindowLongPtr(hWnd,GWLP_WNDPROC);
      SetWindowLongPtr(hWnd,GWLP_USERDATA,p);
      SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LPARAM)EditWndProc);

      hWnd = GetDlgItem(hdlg,IDC_GREENVAL);
      p = GetWindowLongPtr(hWnd,GWLP_WNDPROC);
      SetWindowLongPtr(hWnd,GWLP_USERDATA,p);
      SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LPARAM)EditWndProc);

      hWnd = GetDlgItem(hdlg,IDC_BLUEVAL);
      p = GetWindowLongPtr(hWnd,GWLP_WNDPROC);
      SetWindowLongPtr(hWnd,GWLP_USERDATA,p);
      SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LPARAM)EditWndProc);
      return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDPREVIEW:
				mfd->ifp->Toggle((VDXHWND)hdlg);
				break;
			case IDOK:
				EndDialog(hdlg, 0);
				return TRUE;
			case IDCANCEL:
				EndDialog(hdlg, 1);
				return TRUE;
			case IDC_ZEROR:
				SendMessage(GetDlgItem(hdlg, IDC_RED), TBM_SETPOS, (WPARAM)TRUE, 100);
				SetDlgItemInt(hdlg, IDC_REDVAL, 100, FALSE);
				mfd->red = 0;
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZEROG:
				SendMessage(GetDlgItem(hdlg, IDC_GREEN), TBM_SETPOS, (WPARAM)TRUE, 100);
				SetDlgItemInt(hdlg, IDC_GREENVAL, 100, FALSE);
				mfd->green = 0;
				mfd->ifp->RedoFrame();
				return TRUE;
			case IDC_ZEROB:
				SendMessage(GetDlgItem(hdlg, IDC_BLUE), TBM_SETPOS, (WPARAM)TRUE, 100);
				SetDlgItemInt(hdlg, IDC_BLUEVAL, 100, FALSE);
				mfd->blue = 0;
				mfd->ifp->RedoFrame();
				return TRUE;
			}
			break;
		case WM_HSCROLL:
			{
        HWND item = (HWND)lParam;
        if(item==GetDlgItem(hdlg, IDC_RED)){
          int val = (int)SendMessage(item, TBM_GETPOS, 0, 0)-100;
          if(val!=mfd->red){
            SetDlgItemInt(hdlg, IDC_REDVAL, val+100, FALSE);
            mfd->red = val;
            mfd->ifp->RedoFrame();
          }
        }
        if(item==GetDlgItem(hdlg, IDC_GREEN)){
          int val = (int)SendMessage(item, TBM_GETPOS, 0, 0)-100;
          if(val!=mfd->green){
            SetDlgItemInt(hdlg, IDC_GREENVAL, val+100, FALSE);
            mfd->green = val;
            mfd->ifp->RedoFrame();
          }
        }
        if(item==GetDlgItem(hdlg, IDC_BLUE)){
          int val = (int)SendMessage(item, TBM_GETPOS, 0, 0)-100;
          if(val!=mfd->blue){
            SetDlgItemInt(hdlg, IDC_BLUEVAL, val+100, FALSE);
            mfd->blue = val;
            mfd->ifp->RedoFrame();
          }
        }
				break;
			}
    case WM_EDIT_CHANGED:
      {
        HWND item = (HWND)lParam;
        if(item==GetDlgItem(hdlg,IDC_REDVAL)){
          char buf[80];
          GetWindowText(item,buf,sizeof(buf));
          int val;
          if(sscanf(buf,"%d",&val)==1){
            if(val<0) val = 0;
            if(val>1000) val = 1000;
            mfd->red = val-100;
      			SendMessage(GetDlgItem(hdlg, IDC_RED), TBM_SETPOS, (WPARAM)TRUE, val);
            mfd->ifp->RedoFrame();
          }
        }
        if(item==GetDlgItem(hdlg,IDC_GREENVAL)){
          char buf[80];
          GetWindowText(item,buf,sizeof(buf));
          int val;
          if(sscanf(buf,"%d",&val)==1){
            if(val<0) val = 0;
            if(val>1000) val = 1000;
            mfd->green = val-100;
      			SendMessage(GetDlgItem(hdlg, IDC_GREEN), TBM_SETPOS, (WPARAM)TRUE, val);
            mfd->ifp->RedoFrame();
          }
        }
        if(item==GetDlgItem(hdlg,IDC_BLUEVAL)){
          char buf[80];
          GetWindowText(item,buf,sizeof(buf));
          int val;
          if(sscanf(buf,"%d",&val)==1){
            if(val<0) val = 0;
            if(val>1000) val = 1000;
            mfd->blue = val-100;
      			SendMessage(GetDlgItem(hdlg, IDC_BLUE), TBM_SETPOS, (WPARAM)TRUE, val);
            mfd->ifp->RedoFrame();
          }
        }
        return TRUE;
      }
	}

	return FALSE;
}

int ConfigProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hwnd)
{
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	MyFilterData mfd_old = *mfd;
	int ret;

	mfd->ifp = fa->ifp;

	if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_FILTER),
		(HWND)hwnd, ConfigDlgProc, (LPARAM)mfd))
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

void StringProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *str)
{
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
}

BOOL WINAPI DllMain(HINSTANCE hModule,ULONG reason,void*) 
{
  switch(reason){
  case DLL_PROCESS_ATTACH:
    hInstance = hModule;
    break;
  }
  return(TRUE);
}
