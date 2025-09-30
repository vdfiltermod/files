#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <emmintrin.h>

#include "resource.h"
#include "owdenoise.h"

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
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_OWDENOISE), parent);
}

void ConfigDialog::init_depth()
{
  char buf[80];
  sprintf(buf,"%d",filter->param.depth);
  SetDlgItemText(mhdlg, IDC_DEPTH_EDIT, buf);
}

void ConfigDialog::init_luma()
{
  char buf[80];
  sprintf(buf,"%1.3f",filter->param.luma_strength);
  SetDlgItemText(mhdlg, IDC_LUMA_EDIT, buf);
}

void ConfigDialog::init_chroma()
{
  char buf[80];
  sprintf(buf,"%1.3f",filter->param.chroma_strength);
  SetDlgItemText(mhdlg, IDC_CHROMA_EDIT, buf);
}

INT_PTR ConfigDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
      ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));

      filter->reset();
      init_depth();
      init_luma();
      init_chroma();

      HWND c1 = GetDlgItem(mhdlg, IDC_DEPTH);
      SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(8, 16));
      SendMessage(c1, TBM_SETTICFREQ, 1, 0);
      SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->param.depth);

      c1 = GetDlgItem(mhdlg, IDC_LUMA);
      SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 1000));
      SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, int(filter->param.luma_strength*10000));

      c1 = GetDlgItem(mhdlg, IDC_CHROMA);
      SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 1000));
      SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, int(filter->param.chroma_strength*10000));

      LPARAM p;
      c1 = GetDlgItem(mhdlg,IDC_DEPTH_EDIT);
      p = GetWindowLongPtr(c1,GWLP_WNDPROC);
      SetWindowLongPtr(c1,GWLP_USERDATA,p);
      SetWindowLongPtr(c1,GWLP_WNDPROC,(LPARAM)EditWndProc);

      c1 = GetDlgItem(mhdlg,IDC_LUMA_EDIT);
      p = GetWindowLongPtr(c1,GWLP_WNDPROC);
      SetWindowLongPtr(c1,GWLP_USERDATA,p);
      SetWindowLongPtr(c1,GWLP_WNDPROC,(LPARAM)EditWndProc);

      c1 = GetDlgItem(mhdlg,IDC_CHROMA_EDIT);
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
      if(item==GetDlgItem(mhdlg, IDC_DEPTH)){
        int val = SendMessage(item, TBM_GETPOS, 0, 0);
        if(val!=filter->param.depth){
          filter->param.depth = val;
          filter->reset();
          init_depth();
          ifp->RedoFrame();
        }
      }
      if(item==GetDlgItem(mhdlg, IDC_LUMA)){
        int val = SendMessage(item, TBM_GETPOS, 0, 0);
        filter->param.luma_strength = double(val)/10000;
        filter->reset();
        init_luma();
        ifp->RedoFrame();
      }
      if(item==GetDlgItem(mhdlg, IDC_CHROMA)){
        int val = SendMessage(item, TBM_GETPOS, 0, 0);
        filter->param.chroma_strength = double(val)/10000;
        filter->reset();
        init_chroma();
        ifp->RedoFrame();
      }
      break;
    }
  case WM_EDIT_CHANGED:
    {
      HWND item = (HWND)lParam;
      if(item==GetDlgItem(mhdlg,IDC_DEPTH_EDIT)){
        char buf[80];
        GetWindowText(item,buf,sizeof(buf));
        float val;
        if(sscanf(buf,"%f",&val)==1){
          if(val<8) val = 8;
          if(val>16) val = 16;
          filter->param.depth = int(val);
          filter->reset();
          HWND c1 = GetDlgItem(mhdlg, IDC_DEPTH);
          SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->param.depth);
          ifp->RedoFrame();
        }
      }
      if(item==GetDlgItem(mhdlg,IDC_LUMA_EDIT)){
        char buf[80];
        GetWindowText(item,buf,sizeof(buf));
        float val;
        if(sscanf(buf,"%f",&val)==1){
          if(val<0) val = 0;
          if(val>1) val = 1;
          filter->param.luma_strength = val;
          filter->reset();
          HWND c1 = GetDlgItem(mhdlg, IDC_LUMA);
          SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, int(filter->param.luma_strength*10000));
          ifp->RedoFrame();
        }
      }
      if(item==GetDlgItem(mhdlg,IDC_CHROMA_EDIT)){
        char buf[80];
        GetWindowText(item,buf,sizeof(buf));
        float val;
        if(sscanf(buf,"%f",&val)==1){
          if(val<0) val = 0;
          if(val>1) val = 1;
          filter->param.chroma_strength = val;
          filter->reset();
          HWND c1 = GetDlgItem(mhdlg, IDC_CHROMA);
          SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, int(filter->param.chroma_strength*10000));
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
      supported = true;
      break;
    }
  }

  if(FilterModVersion>=6){
    switch(fa->src.mpPixmapLayout->format) {
    case nsVDXPixmap::kPixFormat_YUV444_Planar16:
    case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    case nsVDXPixmap::kPixFormat_YUV420_Planar16:
      supported = true;
      flags |= FILTERPARAM_NORMALIZE16;
      break;
    }
  }

  if(!supported) return FILTERPARAM_NOT_SUPPORTED;
  return flags;
}

void Filter::Start(){
  reset();
}

void Filter::Run()
{
  if(sAPIVersion<12) return;

  int max_value = 0xFF;
  if(ctx.pixel_depth==16) max_value = 0xFFFF;
  ctx.luma_strength = param.luma_strength*max_value;
  ctx.chroma_strength = param.chroma_strength*max_value;

  render_op0();
}

void Filter::config_input()
{
  const VDXPixmapLayout& src = *fa->src.mpPixmapLayout;
  OWDenoiseContext *s = &ctx;
  int i, j;
  const int h = (src.h+15) & ~15;

  s->hsub = 0;
  s->vsub = 0;
  s->pixel_depth = 8;
  s->depth = param.depth;

  switch(src.format){
  case nsVDXPixmap::kPixFormat_YUV444_Planar16:
  case nsVDXPixmap::kPixFormat_YUV422_Planar16:
  case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    s->pixel_depth = 16;
  }

  switch(src.format){
  case nsVDXPixmap::kPixFormat_YUV422_Planar:
  case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
  case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
  case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
  case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    s->hsub = 1;
    break;
  case nsVDXPixmap::kPixFormat_YUV420_Planar:
  case nsVDXPixmap::kPixFormat_YUV420_Planar_FR:
  case nsVDXPixmap::kPixFormat_YUV420_Planar_709:
  case nsVDXPixmap::kPixFormat_YUV420_Planar_709_FR:
  case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    s->hsub = 1;
    s->vsub = 1;
    break;
  }

  s->linesize = (src.w+15) & ~15;
  for (j = 0; j < 4; j++) {
    for (i = 0; i <= s->depth; i++) {
      s->plane[i][j] = (float*)malloc(s->linesize * h * sizeof(float));
    }
  }
}

void Filter::uninit()
{
  OWDenoiseContext *s = &ctx;
  int i, j;
  for (j = 0; j < 4; j++)
    for (i = 0; i <= s->depth; i++)
      free(s->plane[i][j]);

  s->depth = -1;
}

void Filter::reset()
{
  uninit();
  config_input();
}

//-------------------------------------------------------------------------------------------------

VDXFilterDefinition2 filterDef_owdenoise = VDXVideoFilterDefinition<Filter>("Anton", "owdenoise", "Denoise using wavelets, port from FFMPEG owdenoise code.");
VDXVF_BEGIN_SCRIPT_METHODS(Filter)
VDXVF_DEFINE_SCRIPT_METHOD(Filter, ScriptConfig, "idd")
VDXVF_END_SCRIPT_METHODS()

