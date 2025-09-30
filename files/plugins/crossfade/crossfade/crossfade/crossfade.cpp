#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <emmintrin.h>

#include "resource.h"
#include "crossfade.h"

extern HINSTANCE hInstance;

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

bool ConfigDialog::Show(HWND parent){
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_CROSSFADE), parent);
}

void ConfigDialog::init_width()
{
  char buf[80];
  sprintf(buf,"%d",filter->param.width);
  SetDlgItemText(mhdlg, IDC_WIDTH_EDIT, buf);
}

void ConfigDialog::init_pos()
{
  char buf[80];
  sprintf(buf,"%I64d",filter->param.pos);
  SetDlgItemText(mhdlg, IDC_POS_EDIT, buf);
}

INT_PTR ConfigDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
      ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));

      init_width();
      init_pos();

      HWND c1 = GetDlgItem(mhdlg, IDC_WIDTH);
      SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(1, 30));
      SendMessage(c1, TBM_SETTICFREQ, 1, 0);
      SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->param.width);

      LPARAM p;
      c1 = GetDlgItem(mhdlg,IDC_WIDTH_EDIT);
      p = GetWindowLongPtr(c1,GWLP_WNDPROC);
      SetWindowLongPtr(c1,GWLP_USERDATA,p);
      SetWindowLongPtr(c1,GWLP_WNDPROC,(LPARAM)EditWndProc);

      c1 = GetDlgItem(mhdlg,IDC_POS_EDIT);
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
      filter->param = old_param;
      EndDialog(mhdlg, false);
      return TRUE;
    }
    return TRUE;

  case WM_HSCROLL:
    {
      HWND item = (HWND)lParam;
      if(item==GetDlgItem(mhdlg, IDC_WIDTH)){
        int val = SendMessage(item, TBM_GETPOS, 0, 0);
        if(val!=filter->param.width){
          filter->param.width = val;
          init_width();
          ifp->RedoFrame();
        }
      }
      break;
    }
  case WM_EDIT_CHANGED:
    {
      HWND item = (HWND)lParam;
      if(item==GetDlgItem(mhdlg,IDC_WIDTH_EDIT)){
        char buf[80];
        GetWindowText(item,buf,sizeof(buf));
        float val;
        if(sscanf(buf,"%f",&val)==1){
          if(val<0) val = 0;
          filter->param.width = int(val);
          HWND c1 = GetDlgItem(mhdlg, IDC_WIDTH);
          SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->param.width);
          ifp->RedoFrame();
        }
      }
      if(item==GetDlgItem(mhdlg,IDC_POS_EDIT)){
        char buf[80];
        GetWindowText(item,buf,sizeof(buf));
        sint64 val;
        if(sscanf(buf,"%I64d",&val)==1){
          if(val<-1) val = -1;
          filter->param.pos = val;
          ifp->RedoFrame();
        }
      }
      return TRUE;
    }
  }

  return FALSE;
}

//-------------------------------------------------------------------------------------------------

bool Filter::Configure(VDXHWND hwnd)
{
  ConfigDialog dlg(fa->ifp);
  dlg.filter = this;
  dlg.old_param = param;
  return dlg.Show((HWND)hwnd);
}

uint32 Filter::GetParams()
{
  if(fa->dst.mFrameCount >= 0 && param.pos!=-1) fa->dst.mFrameCount -= param.width;

  bool supported = false;
  uint32 flags = FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_ALIGN_SCANLINES_16 | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;

  if(sAPIVersion >= 12){
    switch(fa->src.mpPixmapLayout->format) {
    case nsVDXPixmap::kPixFormat_YUV444_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Planar:
    case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
    case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
    case nsVDXPixmap::kPixFormat_YUV420_Planar_FR:
    case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
    case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
    case nsVDXPixmap::kPixFormat_YUV420_Planar_709:
    case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
    case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
    case nsVDXPixmap::kPixFormat_YUV420_Planar_709_FR:
    case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_XRGB8888:
      supported = true;
      break;
    }
  }

  if(FilterModVersion>=6){
    switch(fa->src.mpPixmapLayout->format) {
    case nsVDXPixmap::kPixFormat_YUV444_Planar16:
    case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    case nsVDXPixmap::kPixFormat_XRGB64:
      supported = true;
      flags |= FILTERPARAM_NORMALIZE16;
      break;
    }
  }

  if(!supported) return FILTERPARAM_NOT_SUPPORTED;
  return flags;
}

void Filter::Start(){
}

void Filter::Run()
{
  if(sAPIVersion<12) return;

  render();
}

bool Filter::Prefetch2(sint64 frame, IVDXVideoPrefetcher* prefetcher)
{
  if(param.pos==-1 || frame<param.pos-param.width){
    prefetcher->PrefetchFrameDirect(0, frame);
    return true;
  }
  if(frame>=param.pos){
    prefetcher->PrefetchFrameDirect(0, frame+param.width);
    return true;
  }

  prefetcher->PrefetchFrame(0, frame, 0);
  prefetcher->PrefetchFrame(0, frame+param.width, 0);

  return true;
}

//-------------------------------------------------------------------------------------------------

VDXFilterDefinition2 filterDef_crossfade = VDXVideoFilterDefinition<Filter>("Anton", "crossfade", "Crossfade-overlap two video segments.");
VDXVF_BEGIN_SCRIPT_METHODS(Filter)
VDXVF_DEFINE_SCRIPT_METHOD(Filter, ScriptConfig, "li")
VDXVF_END_SCRIPT_METHODS()

