#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <emmintrin.h>

#include "resource.h"
#include "unsharp.h"

extern HINSTANCE hInstance;

bool UnsharpDialog::Show(HWND parent){
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_UNSHARP), parent);
}

void UnsharpDialog::init_size()
{
  char buf[80];
  sprintf(buf,"%1.1f",float(filter->size)/size_scale);
	SetDlgItemText(mhdlg, IDC_BLUR_SIZE_EDIT, buf);
}

void UnsharpDialog::init_power()
{
  char buf[80];
  sprintf(buf,"%1.2f",float(filter->power)/power_scale);
	SetDlgItemText(mhdlg, IDC_UNSHARP_POWER_EDIT, buf);
}

void UnsharpDialog::init_threshold()
{
  char buf[80];
  sprintf(buf,"%d",filter->threshold);
	SetDlgItemText(mhdlg, IDC_UNSHARP_THRESHOLD_EDIT, buf);
}

void UnsharpDialog::init_kernel()
{
  if(filter->bk.w==1){
	  SetDlgItemText(mhdlg, IDC_KERNEL_INFO, "Disabled");
  } else {
    char buf[80];
    sprintf(buf,"Effective kernel: %dx%d px",filter->bk.w,filter->bk.w);
	  SetDlgItemText(mhdlg, IDC_KERNEL_INFO, buf);
  }
}

INT_PTR UnsharpDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
			ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));

      filter->reset();
      init_size();
      init_power();
      init_threshold();
      init_kernel();

			HWND c1 = GetDlgItem(mhdlg, IDC_BLUR_SIZE);
			SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 25*size_scale));
			SendMessage(c1, TBM_SETTICFREQ, size_scale, 0);
			SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->size);

			c1 = GetDlgItem(mhdlg, IDC_UNSHARP_POWER);
			SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 2*power_scale));
			SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->power);

			c1 = GetDlgItem(mhdlg, IDC_UNSHARP_THRESHOLD);
			SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 255));
			SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->threshold);

      LPARAM p;
      c1 = GetDlgItem(mhdlg,IDC_BLUR_SIZE_EDIT);
      p = GetWindowLongPtr(c1,GWLP_WNDPROC);
      SetWindowLongPtr(c1,GWLP_USERDATA,p);
      SetWindowLongPtr(c1,GWLP_WNDPROC,(LPARAM)EditWndProc);

      c1 = GetDlgItem(mhdlg,IDC_UNSHARP_POWER_EDIT);
      p = GetWindowLongPtr(c1,GWLP_WNDPROC);
      SetWindowLongPtr(c1,GWLP_USERDATA,p);
      SetWindowLongPtr(c1,GWLP_WNDPROC,(LPARAM)EditWndProc);

      c1 = GetDlgItem(mhdlg,IDC_UNSHARP_THRESHOLD_EDIT);
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
      filter->power = old_power;
      filter->threshold = old_threshold;
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
      if(item==GetDlgItem(mhdlg, IDC_UNSHARP_POWER)){
        int val = SendMessage(item, TBM_GETPOS, 0, 0);
        if(val!=filter->power){
          filter->power = val;
          init_power();
          ifp->RedoFrame();
        }
      }
      if(item==GetDlgItem(mhdlg, IDC_UNSHARP_THRESHOLD)){
        int val = SendMessage(item, TBM_GETPOS, 0, 0);
        if(val!=filter->threshold){
          filter->threshold = val;
          init_threshold();
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
      if(item==GetDlgItem(mhdlg,IDC_UNSHARP_POWER_EDIT)){
        char buf[80];
        GetWindowText(item,buf,sizeof(buf));
        float val;
        if(sscanf(buf,"%f",&val)==1){
          if(val<0) val = 0;
          if(val>4) val = 4;
          filter->power = int(val*power_scale);
    			HWND c1 = GetDlgItem(mhdlg, IDC_UNSHARP_POWER);
    			SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->power);
          ifp->RedoFrame();
        }
      }
      if(item==GetDlgItem(mhdlg,IDC_UNSHARP_THRESHOLD_EDIT)){
        char buf[80];
        GetWindowText(item,buf,sizeof(buf));
        float val;
        if(sscanf(buf,"%f",&val)==1){
          if(val<0) val = 0;
          if(val>255) val = 255;
          filter->threshold = int(val);
    			HWND c1 = GetDlgItem(mhdlg, IDC_UNSHARP_THRESHOLD);
    			SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->threshold);
          ifp->RedoFrame();
        }
      }
      return TRUE;
    }
  }

  return FALSE;
}

//-------------------------------------------------------------------------------------------------

bool UnsharpFilter::Configure(VDXHWND hwnd)
{
  UnsharpDialog dlg(fa->ifp);
  dlg.filter = this;
  dlg.old_size = size;
  dlg.old_power = power;
  dlg.old_threshold = threshold;
  return dlg.Show((HWND)hwnd);
}

void UnsharpFilter::Run()
{
	if(sAPIVersion<12) return;
  if(bk.w==1 || power==0){
    render_skip();
    return;
  }

  //render_op0();
  render_op2();
}

//-------------------------------------------------------------------------------------------------

VDXFilterDefinition filterDef_unsharp = VDXVideoFilterDefinition<UnsharpFilter>("Anton", "Unsharp mask", "Apply unsharp mask.");
VDXVF_BEGIN_SCRIPT_METHODS(UnsharpFilter)
VDXVF_DEFINE_SCRIPT_METHOD(UnsharpFilter, ScriptConfig, "iii")
VDXVF_END_SCRIPT_METHODS()

