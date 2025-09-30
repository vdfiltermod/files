#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <emmintrin.h>

#include "resource.h"
#include "blur.h"

extern HINSTANCE hInstance;

float gauss_linear(float x, float rho)
{
  const double pi=3.14159265358979323846;
  float rho2 = 2*rho*rho;
  float g = 1/sqrtf(float(rho2*pi));
  g *= expf(-(x*x)/rho2);
  return g;
}

void BlurKernel::eval(float r)
{
  w = int(r*2)*2+1;
  w0 = w/2;
  data = (unsigned short*)realloc(data,w*2);
  buf2 = (unsigned char*)realloc(buf2,w*16+16);
  data2 = (unsigned short*)(ptrdiff_t(buf2+15) & ~15);
  buf3 = (unsigned char*)realloc(buf3,w*16+16);
  data3 = (unsigned short*)(ptrdiff_t(buf3+15) & ~15);

  float fsum = 0;
  {for(int x=0; x<w; x++){
    float f = gauss_linear(float(x-w0),r);
    fsum += f;
  }}

  float m = 256/fsum;

  sum = 0;
  {for(int x=0; x<w; x++){
    float f = gauss_linear(float(x-w0),r);
    int v = int(floor(f*m+0.5));
    data[x] = v;
    sum += v;
  }}

  data[w0] += 256-sum;
  sum = 256;

  while(!data[0]){
    w-=2;
    w0--;
    memmove(data,data+1,w*2);
  }

  {for(int x=0; x<w; x++){
    __m128i v2 = _mm_set1_epi16(data[x]);
    _mm_store_si128((__m128i*)(data2+x*8),v2);

    __m128i v3 = _mm_slli_epi16(v2,8);
    _mm_store_si128((__m128i*)(data3+x*8),v3);
  }}
}

//-------------------------------------------------------------------------------------------------

void edit_changed(HWND wnd)
{
  SendMessage(GetParent(wnd),WM_EDIT_CHANGED,0,(LONG)wnd);
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

bool BlurDialog::Show(HWND parent){
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_BLUR), parent);
}

void BlurDialog::init_size()
{
  char buf[80];
  sprintf(buf,"%1.1f",float(filter->size)/size_scale);
	SetDlgItemText(mhdlg, IDC_BLUR_SIZE_EDIT, buf);
}

void BlurDialog::init_kernel()
{
  if(filter->bk.w==1){
	  SetDlgItemText(mhdlg, IDC_KERNEL_INFO, "Disabled");
  } else {
    char buf[80];
    sprintf(buf,"Effective kernel: %dx%d px",filter->bk.w,filter->bk.w);
	  SetDlgItemText(mhdlg, IDC_KERNEL_INFO, buf);
  }
}

INT_PTR BlurDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
			ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));

      filter->reset();
      init_size();
      init_kernel();

			HWND c1 = GetDlgItem(mhdlg, IDC_BLUR_SIZE);
			SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 25*size_scale));
			SendMessage(c1, TBM_SETTICFREQ, size_scale, 0);
			SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->size);

      LPARAM p;
      c1 = GetDlgItem(mhdlg,IDC_BLUR_SIZE_EDIT);
      p = GetWindowLongPtr(c1,GWLP_WNDPROC);
      SetWindowLongPtr(c1,GWLP_USERDATA,p);
      SetWindowLongPtr(c1,GWLP_WNDPROC,(LPARAM)EditWndProc);
      return true;
    }

  case WM_DESTROY:
    ifp->InitButton(0);
    break;

  case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDPREVIEW:
			ifp->Toggle((VDXHWND)mhdlg);
			break;
		case IDOK:
			EndDialog(mhdlg, true);
			return TRUE;
		case IDCANCEL:
      filter->size = old_size;
			EndDialog(mhdlg, false);
			return TRUE;
		}
    return TRUE;

	case WM_HSCROLL:
		{
      HWND item = (HWND)lParam;
      if(item==GetDlgItem(mhdlg, IDC_BLUR_SIZE)){
        int val = SendMessage(item, TBM_GETPOS, 0, 0);
        if(val!=filter->size){
          filter->size = val;
          filter->reset();
          init_size();
          init_kernel();
          ifp->RedoFrame();
        }
      }
			break;
		}
  case WM_EDIT_CHANGED:
    {
      HWND item = (HWND)lParam;
      if(item==GetDlgItem(mhdlg,IDC_BLUR_SIZE_EDIT)){
        char buf[80];
        GetWindowText(item,buf,sizeof(buf));
        float val;
        if(sscanf(buf,"%f",&val)==1){
          if(val<0) val = 0;
          if(val>25) val = 25;
          filter->size = int(val*size_scale);
          filter->reset();
          init_kernel();
    			HWND c1 = GetDlgItem(mhdlg, IDC_BLUR_SIZE);
    			SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->size);
          ifp->RedoFrame();
        }
      }
      return TRUE;
    }
  }

  return FALSE;
}

//-------------------------------------------------------------------------------------------------

bool BlurFilter::Configure(VDXHWND hwnd)
{
  BlurDialog dlg(fa->ifp);
  dlg.filter = this;
  dlg.old_size = size;
  return dlg.Show((HWND)hwnd);
}

uint32 BlurFilter::GetParams() {
	if (sAPIVersion >= 12) {
		switch(fa->src.mpPixmapLayout->format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
				break;

			default:
				return FILTERPARAM_NOT_SUPPORTED;
		}
	}

	fa->dst.offset = 0;
	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void BlurFilter::Start() {
  reset();
}

void BlurFilter::Run()
{
	if(sAPIVersion<12) return;
  if(bk.w==1){
    render_skip();
    return;
  }

  //render_op0();
  //render_op1();
  render_op2();
}

void BlurFilter::reset()
{
  const VDXPixmapLayout& src = *fa->src.mpPixmapLayout;
  bk.eval(float(size)/size_scale);

  int bkw1 = (bk.w0*4 + 7) & ~7;
  buf_pitch = ((src.w*4 + 7) & ~7) + bkw1*2;

  slice_h = 0x10000/(buf_pitch*2); // assume buf should not overflow L1 cache
  if(slice_h<1) slice_h = 1;
  if(bk.w>slice_h) slice_h = bk.w; // assume not reloading same source more than twice

  row = (unsigned char*)realloc(row, (slice_h+bk.w0*2)*16 + 16);
  buf = (unsigned short*)realloc(buf, buf_pitch*2*slice_h + 16);
}

//-------------------------------------------------------------------------------------------------

VDXFilterDefinition filterDef_blur = VDXVideoFilterDefinition<BlurFilter>("Anton", "Gauss blur", "Apply gaussian blur.");
VDXVF_BEGIN_SCRIPT_METHODS(BlurFilter)
VDXVF_DEFINE_SCRIPT_METHOD(BlurFilter, ScriptConfig, "i")
VDXVF_END_SCRIPT_METHODS()

