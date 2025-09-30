#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <emmintrin.h>

#include "resource.h"
#include "hqdn3d.h"

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
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_DENOISE), parent);
}

void ConfigDialog::init_param(double v, int id)
{
  char buf[80];
  sprintf(buf,"%1.3f",v);
  SetDlgItemText(mhdlg, id, buf);
}

void ConfigDialog::init_param_slider(double v, int id)
{
  HWND c1 = GetDlgItem(mhdlg, id);
  SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 1000));
  SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, int(v*1000/10));
}

void ConfigDialog::init_edit(int id)
{
  HWND c1 = GetDlgItem(mhdlg,id);
  LONG_PTR p = GetWindowLongPtr(c1,GWLP_WNDPROC);
  SetWindowLongPtr(c1,GWLP_USERDATA,p);
  SetWindowLongPtr(c1,GWLP_WNDPROC,(LPARAM)EditWndProc);
}

void ConfigDialog::change_scroll(int id, int edit_id, double& v)
{
  HWND item = GetDlgItem(mhdlg, id);
  int val = SendMessage(item, TBM_GETPOS, 0, 0);
  v = double(val)/1000*10;
  filter->reset();
  init_param(v,edit_id);
  ifp->RedoFrame();
}

void ConfigDialog::change_edit(int id, int edit_id, double& v)
{
  char buf[80];
  HWND item = GetDlgItem(mhdlg, edit_id);
  GetWindowText(item,buf,sizeof(buf));
  float val;
  if(sscanf(buf,"%f",&val)==1){
    if(val<0) val = 0;
    if(val>252) val = 252;
    v = val;
    filter->reset();
    HWND c1 = GetDlgItem(mhdlg, id);
    SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, int(v*1000/10));
    ifp->RedoFrame();
  }
}

void ConfigDialog::init_tdepth()
{
  bool v = filter->param.tdepth>0 || filter->param.recursive;
  EnableWindow(GetDlgItem(mhdlg, IDC_TLUMA), v);
  EnableWindow(GetDlgItem(mhdlg, IDC_TCHROMA), v);
  EnableWindow(GetDlgItem(mhdlg, IDC_TLUMA_EDIT), v);
  EnableWindow(GetDlgItem(mhdlg, IDC_TCHROMA_EDIT), v);
  if(v){
    init_param(filter->param.tluma_strength, IDC_TLUMA_EDIT);
    init_param(filter->param.tchroma_strength, IDC_TCHROMA_EDIT);

    init_param_slider(filter->param.tluma_strength, IDC_TLUMA);
    init_param_slider(filter->param.tchroma_strength, IDC_TCHROMA);
  } else {
    init_param(0, IDC_TLUMA_EDIT);
    init_param(0, IDC_TCHROMA_EDIT);

    init_param_slider(0, IDC_TLUMA);
    init_param_slider(0, IDC_TCHROMA);
  }
}

void ConfigDialog::init_recursive()
{
  bool v = !filter->param.recursive;
  EnableWindow(GetDlgItem(mhdlg, IDC_TDEPTH), v);
  EnableWindow(GetDlgItem(mhdlg, IDC_TDEPTH_EDIT), v);
  if(v){
    SetDlgItemInt(mhdlg, IDC_TDEPTH_EDIT, filter->param.tdepth, TRUE);
    HWND c1 = GetDlgItem(mhdlg, IDC_TDEPTH);
    SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->param.tdepth);
  } else {
    SetDlgItemText(mhdlg, IDC_TDEPTH_EDIT, "yes");
    HWND c1 = GetDlgItem(mhdlg, IDC_TDEPTH);
    SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, 100);
  }
}

INT_PTR ConfigDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
      ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));

      filter->reset();
      init_param(filter->param.luma_strength, IDC_LUMA_EDIT);
      init_param(filter->param.chroma_strength, IDC_CHROMA_EDIT);
      init_param(filter->param.tluma_strength, IDC_TLUMA_EDIT);
      init_param(filter->param.tchroma_strength, IDC_TCHROMA_EDIT);

      init_param_slider(filter->param.luma_strength, IDC_LUMA);
      init_param_slider(filter->param.chroma_strength, IDC_CHROMA);
      init_param_slider(filter->param.tluma_strength, IDC_TLUMA);
      init_param_slider(filter->param.tchroma_strength, IDC_TCHROMA);

      SetDlgItemInt(mhdlg, IDC_TDEPTH_EDIT, filter->param.tdepth, TRUE);
      HWND c1 = GetDlgItem(mhdlg, IDC_TDEPTH);
      SendMessage(c1, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 10));
      SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->param.tdepth);

      init_edit(IDC_LUMA_EDIT);
      init_edit(IDC_CHROMA_EDIT);
      init_edit(IDC_TLUMA_EDIT);
      init_edit(IDC_TCHROMA_EDIT);
      init_edit(IDC_TDEPTH_EDIT);

      CheckDlgButton(mhdlg, IDC_RECURSIVE, filter->param.recursive ? BST_CHECKED:BST_UNCHECKED);

      init_tdepth();
      init_recursive();
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
    case IDC_RECURSIVE:
      filter->param.recursive = IsDlgButtonChecked(mhdlg,IDC_RECURSIVE);
      init_tdepth();
      init_recursive();
      filter->reset();
      ifp->RedoFrame();
    }
    return TRUE;

  case WM_HSCROLL:
    {
      HWND item = (HWND)lParam;
      if(item==GetDlgItem(mhdlg, IDC_LUMA)) change_scroll(IDC_LUMA, IDC_LUMA_EDIT, filter->param.luma_strength);
      if(item==GetDlgItem(mhdlg, IDC_CHROMA)) change_scroll(IDC_CHROMA, IDC_CHROMA_EDIT, filter->param.chroma_strength);
      if(item==GetDlgItem(mhdlg, IDC_TLUMA)) change_scroll(IDC_TLUMA, IDC_TLUMA_EDIT, filter->param.tluma_strength);
      if(item==GetDlgItem(mhdlg, IDC_TCHROMA)) change_scroll(IDC_TCHROMA, IDC_TCHROMA_EDIT, filter->param.tchroma_strength);
      if(item==GetDlgItem(mhdlg, IDC_TDEPTH)){
        int val = SendMessage(item, TBM_GETPOS, 0, 0);
        filter->param.tdepth = val;
        filter->reset();
        SetDlgItemInt(mhdlg, IDC_TDEPTH_EDIT, filter->param.tdepth, TRUE);
        init_tdepth();
        ifp->RedoFrame();
      }
      break;
    }
  case WM_EDIT_CHANGED:
    {
      HWND item = (HWND)lParam;
      if(item==GetDlgItem(mhdlg, IDC_LUMA_EDIT)) change_edit(IDC_LUMA, IDC_LUMA_EDIT, filter->param.luma_strength);
      if(item==GetDlgItem(mhdlg, IDC_CHROMA_EDIT)) change_edit(IDC_CHROMA, IDC_CHROMA_EDIT, filter->param.chroma_strength);
      if(item==GetDlgItem(mhdlg, IDC_TLUMA_EDIT)) change_edit(IDC_TLUMA, IDC_TLUMA_EDIT, filter->param.tluma_strength);
      if(item==GetDlgItem(mhdlg, IDC_TCHROMA_EDIT)) change_edit(IDC_TCHROMA, IDC_TCHROMA_EDIT, filter->param.tchroma_strength);
      if(item==GetDlgItem(mhdlg, IDC_TDEPTH_EDIT)){
        int val = GetDlgItemInt(mhdlg, IDC_TDEPTH_EDIT, 0, TRUE);
        filter->param.tdepth = val;
        filter->reset();
        HWND c1 = GetDlgItem(mhdlg, IDC_TDEPTH);
        SendMessage(c1, TBM_SETPOS, (WPARAM)TRUE, filter->param.tdepth);
        init_tdepth();
        ifp->RedoFrame();
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

  render_op0();
}

bool Filter::Prefetch2(sint64 frame, IVDXVideoPrefetcher* prefetcher)
{
  prefetcher->PrefetchFrameSymbolic(0, frame);
  prefetcher->PrefetchFrame(0, frame, 0);
  if(param.recursive){
    prefetcher->PrefetchFrame(0, frame-1, 0);
    prefetcher->PrefetchFrame(0, frame-2, 0);
    return true;
  }
  
  {for(int i=1; i<=param.tdepth; i++)
    prefetcher->PrefetchFrame(0, frame-i, 0); }

  return true;
}

extern "C" void ff_hqdn3d_init_x86(HQDN3DContext *hqdn3d);

void Filter::config_input()
{
  const VDXPixmapLayout& src = *fa->src.mpPixmapLayout;
  HQDN3DContext *s = &ctx;

  s->hsub = 0;
  s->vsub = 0;
  s->depth = 8;

  switch(src.format){
  case nsVDXPixmap::kPixFormat_YUV444_Planar16:
  case nsVDXPixmap::kPixFormat_YUV422_Planar16:
  case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    s->depth = 16;
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

  s->line = (uint16_t*)malloc(src.w*sizeof(uint16_t));

  s->strength[LUMA_SPATIAL] = param.luma_strength;
  s->strength[CHROMA_SPATIAL] = param.chroma_strength;
  s->strength[LUMA_TMP] = param.tluma_strength;
  s->strength[CHROMA_TMP] = param.tchroma_strength;

  if(param.tdepth==0 && !param.recursive){
    s->strength[LUMA_TMP] = 0;
    s->strength[CHROMA_TMP] = 0;
  }

  /*
  #define PARAM1_DEFAULT 4.0
  #define PARAM2_DEFAULT 3.0
  #define PARAM3_DEFAULT 6.0

  if (!s->strength[LUMA_SPATIAL])
      s->strength[LUMA_SPATIAL] = PARAM1_DEFAULT;
  if (!s->strength[CHROMA_SPATIAL])
      s->strength[CHROMA_SPATIAL] = PARAM2_DEFAULT * s->strength[LUMA_SPATIAL] / PARAM1_DEFAULT;
  if (!s->strength[LUMA_TMP])
      s->strength[LUMA_TMP]   = PARAM3_DEFAULT * s->strength[LUMA_SPATIAL] / PARAM1_DEFAULT;
  if (!s->strength[CHROMA_TMP])
      s->strength[CHROMA_TMP] = s->strength[LUMA_TMP] * s->strength[CHROMA_SPATIAL] / s->strength[LUMA_SPATIAL];
  */

  for (int i = 0; i < 4; i++) {
      s->coefs[i] = precalc_coefs(s->strength[i], s->depth);
  }

  ff_hqdn3d_init_x86(s);

  s->prev_init = -1;

  if(param.luma_strength!=0 || s->strength[LUMA_TMP]){
    s->frame_prev[0] = (uint16_t*)malloc(src.w*src.h*sizeof(uint16_t));
  }
  
  if(param.chroma_strength!=0 || s->strength[CHROMA_TMP]){
    const int cw = AV_CEIL_RSHIFT(src.w, s->hsub);
    const int ch = AV_CEIL_RSHIFT(src.h, s->vsub);
    s->frame_prev[1] = (uint16_t*)malloc(cw*ch*sizeof(uint16_t));
    s->frame_prev[2] = (uint16_t*)malloc(cw*ch*sizeof(uint16_t));
  }
}

void Filter::uninit()
{
  HQDN3DContext *s = &ctx;

  #define av_freep(p) free(*p); *p=0;

  av_freep(&s->coefs[0]);
  av_freep(&s->coefs[1]);
  av_freep(&s->coefs[2]);
  av_freep(&s->coefs[3]);
  av_freep(&s->line);
  av_freep(&s->frame_prev[0]);
  av_freep(&s->frame_prev[1]);
  av_freep(&s->frame_prev[2]);
}

void Filter::reset()
{
  uninit();
  config_input();
}

//-------------------------------------------------------------------------------------------------

VDXFilterDefinition2 filterDef_hqdn3d = VDXVideoFilterDefinition<Filter>("Anton", "hqdn3d", "Denoise spatial+temporal, port from FFMPEG hqdn3d code.");
VDXVF_BEGIN_SCRIPT_METHODS(Filter)
VDXVF_DEFINE_SCRIPT_METHOD(Filter, ScriptConfig, "ddddi")
VDXVF_END_SCRIPT_METHODS()

