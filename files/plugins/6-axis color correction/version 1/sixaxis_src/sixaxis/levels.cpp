#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <malloc.h>

#include "resource.h"
#include "levels.h"

extern HINSTANCE hInstance;

#pragma warning(disable:4305)
#pragma warning(disable:4244)

//-------------------------------------------------------------------------------------------------

void edit_changed(HWND wnd)
{
  WPARAM id = GetWindowLong(wnd,GWL_ID);
  SendMessage(GetParent(wnd),WM_EDIT_CHANGED,id,(LONG)wnd);
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

//-------------------------------------------------------------------------------------------------

LRESULT CALLBACK LevelsWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
  LevelsControl* obj = (LevelsControl*)GetWindowLongPtr(wnd,GWLP_USERDATA);

  switch(msg){
  case WM_ERASEBKGND:
    return 1;

  case WM_LBUTTONDOWN:
    {
      SetCapture(wnd);
      POINT p;
      GetCursorPos(&p);
      MapWindowPoints(0,wnd,&p,1);
      obj->begin_drag(p.x,p.y);
      return true;
    }

  case WM_MOUSEMOVE:
    {
      if(obj->track_value!=-1){
        POINT p;
        GetCursorPos(&p);
        MapWindowPoints(0,wnd,&p,1);
        obj->update_drag(p.x,p.y);
      }
      return true;
    }

  case WM_LBUTTONUP:
    {
      if(obj->track_value!=-1){
        POINT p;
        GetCursorPos(&p);
        MapWindowPoints(0,wnd,&p,1);
        obj->update_drag(p.x,p.y);
        obj->track_value = -1;
      }
      ReleaseCapture();
      return true;
    }

  case WM_PAINT:
    {
      obj->draw();
      return 0;
    }
  }

  return CallWindowProc(obj->base_proc,wnd,msg,wparam,lparam);
}

LevelsControl::~LevelsControl()
{
  if(bm) DeleteObject(bm);
  if(dc) DeleteDC(dc);
  if(pen0) DeleteObject(pen0);
  if(pen1) DeleteObject(pen1);
}

void LevelsControl::init()
{
  RECT r;
  GetClientRect(hwnd,&r);
  HDC dc0 = GetDC(hwnd);
  dc = CreateCompatibleDC(dc0);
  w = r.right;
  h = r.bottom;
  ReleaseDC(hwnd,dc0);

  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biPlanes = 1;

  bm = CreateDIBSection(0,&bmi,0,&bm_p,0,0);
  SelectObject(dc,bm);

  pen0 = CreatePen(PS_SOLID,1,RGB(0,0,0));
  pen1 = CreatePen(PS_SOLID,1,RGB(255,0,0));
}

void LevelsControl::draw()
{
  PAINTSTRUCT ps;
  BeginPaint(hwnd,&ps);
  RECT r;
  GetClientRect(hwnd,&r);

  int c0 = GetSysColor(COLOR_BTNFACE);
  int bg_color = ((c0 & 0xFF)<<16) | (c0 & 0xFF00) | ((c0 & 0xFF0000)>>16);

  int color[][6] = {
    {255,255,255, 256,0,0},
    {255,255,255, 256,170,0},
    {255,255,255, 170,256,0},
    {255,255,255, 0,256,140},
    {255,255,255, 0,140,256},
    {255,255,255, 170,0,256},

    {0,0,0, 256,256,256},
    {0,0,0, 256,256,256},

    {255,0,0,     0,256,0},
    {255,255,0,   0,140,255},
  };

  int h0 = h-13;
  int x0 = param_to_px(range0);
  int x1 = param_to_px(range1);

  {for(int y=0; y<h; y++){
    uint32* d = (uint32*)bm_p+y*w;
    {for(int x=0; x<w; x++){
      *d = bg_color;
      d++;
    }}
  }}

  {for(int x=x0; x<=x1; x++){
    int c1 = 0xff*(x-x0)/(x1-x0);
    int c0 = 0xff-c1;
    int cr = (c0*color[ch][0] + c1*color[ch][3])>>8;
    int cg = (c0*color[ch][1] + c1*color[ch][4])>>8;
    int cb = (c0*color[ch][2] + c1*color[ch][5])>>8;
    int color = cr<<16 | cg<<8 | cb;
    uint32* d = (uint32*)bm_p+x;
    {for(int y=0; y<h0; y++){
      *d = color;
      d += w;
    }}
  }}

  SelectObject(dc,pen0);

  show_reset = false;
  if(param->v0!=0 || param->v1!=0.5 || param->v2!=1){
    show_reset = true;
    POINT p[4];
    p[0].x = w-8;
    p[0].y = 0;
    p[1].x = w;
    p[1].y = 8;
    p[2].x = w-8;
    p[2].y = 8;
    p[3].x = w;
    p[3].y = 0;
    Polyline(dc,p,2);
    Polyline(dc,p+2,2);
  }

  if(edit_v0) draw_marker(param_to_px(param->v0),h0);
  if(edit_v2) draw_marker(param_to_px(param->v2),h0);

  if(edit_v1){
    float v1 = param->v1*param->v2 + (1-param->v1)*param->v0;
    if(ch==6) SelectObject(dc,pen1);
    draw_marker1(param_to_px(v1),h0-1);
  }

  BitBlt(ps.hdc, 0,0, r.right, r.bottom, dc, 0,0, SRCCOPY);
  EndPaint(hwnd,&ps);
}

void LevelsControl::draw_marker(int x,int y)
{
  POINT p[4];
  p[0].x = x;
  p[0].y = y;
  p[1].x = x+4;
  p[1].y = y+12;
  p[2].x = x-4;
  p[2].y = y+12;
  p[3].x = x;
  p[3].y = y;
  Polyline(dc,p,4);
}

void LevelsControl::draw_marker1(int x,int y)
{
  POINT p[5];
  p[0].x = x;
  p[0].y = y;
  p[1].x = x+4;
  p[1].y = y-4;
  p[2].x = x;
  p[2].y = y-8;
  p[3].x = x-4;
  p[3].y = y-4;
  p[4].x = x;
  p[4].y = y;
  Polyline(dc,p,5);
}

void LevelsControl::begin_drag(int x,int y)
{
  if(show_reset && x>w-12 && y<8){
    param->v0 = 0;
    param->v1 = 0.5;
    param->v2 = 1;
    if(edit_v0) owner->init_value(edit_v0,param->v0);
    if(edit_v1) owner->init_value(edit_v1,param->v1);
    if(edit_v2) owner->init_value(edit_v2,param->v2);

    InvalidateRect(hwnd,0,false);
    RedrawWindow(hwnd,0,0,RDW_UPDATENOW);
    owner->RedoFrame();
    return;
  }

  int x0 = param_to_px(param->v0);
  int x2 = param_to_px(param->v2);
  float v1 = param->v1*param->v2 + (1-param->v1)*param->v0;
  int x1 = param_to_px(v1);
  
  bool track0 = false;
  bool track1 = false;
  bool track2 = false;
  if(x>x0-10 && x<x0+10 && edit_v0) track0 = true;
  if(x>x1-10 && x<x1+10 && edit_v1) track1 = true;
  if(x>x2-10 && x<x2+10 && edit_v2) track2 = true;

  if(track0 && track1){
    if(y>h-13) track1 = false; else track0 = false;
  }

  if(track2 && track1){
    if(y>h-13) track1 = false; else track2 = false;
  }

  if(track0 && track2){
    int m02 = (x0+x2)/2;
    if(x<m02) track2 = false; else track0 = false;
  }

  if(track0){ track_value = 0; track_x = x-x0; return; }
  if(track1){ track_value = 1; track_x = x-x1; return; }
  if(track2){ track_value = 2; track_x = x-x2; return; }

  track0 = edit_v0!=0;
  track1 = edit_v1!=0;
  track2 = edit_v2!=0;

  if(track0 && track2) track0 = false;
  if(track1 && track2){ if(y>h-13) track1=false; else track2=false; }

  if(track0){
    track_value = 0;
    track_x = 0;
    update_drag(x,y);
    return;
  }
  if(track1){
    track_value = 1;
    track_x = 0;
    update_drag(x,y);
    return;
  }
  if(track2){
    track_value = 2;
    track_x = 0;
    update_drag(x,y);
    return;
  }
}

void LevelsControl::update_drag(int x,int y)
{
  if(track_value==0){ 
    param->v0 = px_to_param(x-track_x); 
    owner->validate_v0(*this);
    owner->init_value(edit_v0,param->v0);
  }
  if(track_value==2){ 
    param->v2 = px_to_param(x-track_x); 
    owner->validate_v2(*this); 
    owner->init_value(edit_v2,param->v2);
  }
  if(track_value==1){
    param->v1 = px1_to_param(x-track_x);
    owner->validate_v1(*this);
    owner->init_value(edit_v1,param->v1);
  }
  InvalidateRect(hwnd,0,false);
  RedrawWindow(hwnd,0,0,RDW_UPDATENOW);
  owner->RedoFrame();
}

//-------------------------------------------------------------------------------------------------

void LevelsDialog::init_value(int id, float v)
{
  char buf[80];
  sprintf(buf,"%g",v);
	SetDlgItemText(mhdlg, id, buf);
}

bool LevelsDialog::modify_value(HWND item, float& v)
{
  char buf[80];
  GetWindowText(item,buf,sizeof(buf));
  float val;
  if(sscanf(buf,"%f",&val)==1){
    v = val;
    return true;
  }
  return false;
}

void LevelsDialog::init_edit(int id)
{
  HWND hWnd = GetDlgItem(mhdlg,id);
  LPARAM p = GetWindowLongPtr(hWnd,GWLP_WNDPROC);
  SetWindowLongPtr(hWnd,GWLP_USERDATA,p);
  SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LPARAM)EditWndProc);
}

void LevelsDialog::init_levels(int id, LevelsControl& obj)
{
  HWND hWnd = GetDlgItem(mhdlg,id);
  obj.hwnd = hWnd;
  obj.base_proc = (WNDPROC)GetWindowLongPtr(hWnd,GWLP_WNDPROC);
  obj.owner = this;
  SetWindowLongPtr(hWnd,GWLP_USERDATA,(LPARAM)&obj);
  SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LPARAM)LevelsWndProc);
  obj.init();

  if(obj.edit_v0){
    init_edit(obj.edit_v0);
    init_value(obj.edit_v0,obj.param->v0);
  }
  if(obj.edit_v1){
    init_edit(obj.edit_v1);
    init_value(obj.edit_v1,obj.param->v1);
  }
  if(obj.edit_v2){
    init_edit(obj.edit_v2);
    init_value(obj.edit_v2,obj.param->v2);
  }
}

bool LevelsDialog::modify_param(LevelsControl& a, HWND item, int id){
  bool changed = false;
  if(id==a.edit_v0) if(modify_value(item,a.param->v0)){ changed=true; validate_v0(a); }
  if(id==a.edit_v1) if(modify_value(item,a.param->v1)) changed=true;
  if(id==a.edit_v2) if(modify_value(item,a.param->v2)){ changed=true; validate_v2(a); }
  if(changed) InvalidateRect(a.hwnd,0,false);
  return changed;
}

bool LevelsDialog::Show(HWND parent){
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_LEVELS), parent);
}

INT_PTR LevelsDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
			ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));

      ci.ch = 6; ci.param = &filter->param.ci; ci.edit_v0 = IDC_I_V0; ci.edit_v1 = IDC_I_V1; ci.edit_v2 = IDC_I_V2;
      cs.ch = 7; cs.range1=2; cs.param = &filter->param.cs; cs.edit_v2 = IDC_S_V2;
      cd0.ch = 8; cd0.range0=-0.5; cd0.range1=0.5; cd0.param = &filter->param.d0; cd0.edit_v0 = IDC_D0_V;
      cd1.ch = 9; cd1.range0=-0.5; cd1.range1=0.5; cd1.param = &filter->param.d1; cd1.edit_v0 = IDC_D1_V;
      cc[0].edit_v1 = IDC_C0_V1; cc[0].edit_v2 = IDC_C0_V2;
      cc[1].edit_v1 = IDC_C1_V1; cc[1].edit_v2 = IDC_C1_V2;
      cc[2].edit_v1 = IDC_C2_V1; cc[2].edit_v2 = IDC_C2_V2;
      cc[3].edit_v1 = IDC_C3_V1; cc[3].edit_v2 = IDC_C3_V2;
      cc[4].edit_v1 = IDC_C4_V1; cc[4].edit_v2 = IDC_C4_V2;
      cc[5].edit_v1 = IDC_C5_V1; cc[5].edit_v2 = IDC_C5_V2;
      {for(int i=0; i<6; i++){ cc[i].ch=i; cc[i].range1=1.25; cc[i].param = &filter->param.cc[i]; }}
      init_levels(IDC_I_CONTROL,ci);
      init_levels(IDC_S_CONTROL,cs);
      init_levels(IDC_D0_CONTROL,cd0);
      init_levels(IDC_D1_CONTROL,cd1);
      init_levels(IDC_C0_CONTROL,cc[0]);
      init_levels(IDC_C1_CONTROL,cc[1]);
      init_levels(IDC_C2_CONTROL,cc[2]);
      init_levels(IDC_C3_CONTROL,cc[3]);
      init_levels(IDC_C4_CONTROL,cc[4]);
      init_levels(IDC_C5_CONTROL,cc[5]);

      edit_br = CreateSolidBrush(RGB(220,220,220));
      return true;
    }

  case WM_DESTROY:
    ifp->InitButton(0);
    break;

  case WM_CTLCOLOREDIT:
    {
      HDC dc = (HDC)wParam;
      SetBkColor(dc,RGB(220, 220, 220));
      return (INT_PTR)edit_br;
    }

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
  
  case WM_EDIT_CHANGED:
    {
      HWND item = (HWND)lParam;
      bool changed = false;
      if(modify_param(ci,item,wParam)) changed = true;
      if(modify_param(cs,item,wParam)) changed = true;
      if(modify_param(cd0,item,wParam)) changed = true;
      if(modify_param(cd1,item,wParam)) changed = true;
      {for(int i=0; i<5; i++) if(modify_param(cc[i],item,wParam)) changed = true; }

      if(changed) RedoFrame();
      return TRUE;
    }
  }

  return FALSE;
}

void LevelsDialog::RedoFrame(){
  filter->eval_tables();
  ifp->RedoFrame();
}

//-------------------------------------------------------------------------------------------------

void LevelsFilter::initParam()
{
  param.cs.v2 = 1;
  param.d0.v0 = 0;
  param.d1.v0 = 0;
}

void LevelsFilter::init()
{
  buf = 0;
}

void LevelsFilter::GetScriptString(char* buf, int maxlen){
  SafePrintf(buf, maxlen, "Config(%g,%g,%g, %g,%g, %g, %g,%g, %g,%g, %g,%g, %g,%g, %g,%g, %g,%g)", 
    param.ci.v0, param.ci.v1, param.ci.v2, 
    param.d0.v0, param.d1.v0,
    param.cs.v2,
    param.cc[0].v1, param.cc[0].v2,
    param.cc[1].v1, param.cc[1].v2,
    param.cc[2].v1, param.cc[2].v2,
    param.cc[3].v1, param.cc[3].v2,
    param.cc[4].v1, param.cc[4].v2,
    param.cc[5].v1, param.cc[5].v2
  );
}

void LevelsFilter::ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
  param.ci.v0 = float(argv[0].asDouble());
  param.ci.v1 = float(argv[1].asDouble());
  param.ci.v2 = float(argv[2].asDouble());

  param.d0.v0 = float(argv[3].asDouble());
  param.d1.v0 = float(argv[4].asDouble());

  param.cs.v2 = float(argv[5].asDouble());

  param.cc[0].v1 = float(argv[6].asDouble());
  param.cc[0].v2 = float(argv[7].asDouble());

  param.cc[1].v1 = float(argv[8].asDouble());
  param.cc[1].v2 = float(argv[9].asDouble());

  param.cc[2].v1 = float(argv[10].asDouble());
  param.cc[2].v2 = float(argv[11].asDouble());

  param.cc[3].v1 = float(argv[12].asDouble());
  param.cc[3].v2 = float(argv[13].asDouble());

  param.cc[4].v1 = float(argv[14].asDouble());
  param.cc[4].v2 = float(argv[15].asDouble());

  param.cc[5].v1 = float(argv[16].asDouble());
  param.cc[5].v2 = float(argv[17].asDouble());

  eval_tables();
}

bool LevelsFilter::Configure(VDXHWND hwnd)
{
  LevelsDialog dlg(fa->ifp);
  dlg.filter = this;
  dlg.old_param = param;
  return dlg.Show((HWND)hwnd);
}

uint32 LevelsFilter::GetParams() {
  kPixFormat_XRGB64 = 0;
  if(fma && fma->fmpixmap) kPixFormat_XRGB64 = fma->fmpixmap->GetFormat_XRGB64();

  if (sAPIVersion >= 12) {
    switch(fa->src.mpPixmapLayout->format) {
    case nsVDXPixmap::kPixFormat_XRGB8888:
      break;

    default:
      if(fa->src.mpPixmapLayout->format==kPixFormat_XRGB64)
        break;
      return FILTERPARAM_NOT_SUPPORTED;
    }
  }

  fa->dst.offset = 0;
  return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void LevelsFilter::Start()
{
  const VDXPixmapLayout& src = *fa->src.mpPixmapLayout;
  int w = (src.w+3)/4;
  buf = (unsigned char*)realloc(buf, w*32+64);

  eval_tables();
}

void LevelsFilter::Run()
{
	if(sAPIVersion<12) return;

  int round_mode = _controlfp(0,0);
  _controlfp(_RC_DOWN, _MCW_RC);

  if(fa->src.mpPixmapLayout->format==kPixFormat_XRGB64){
    const VDXPixmap& dst = *fa->dst.mpPixmap;
    FilterModPixmapInfo& dst_info = *fma->fmpixmap->GetPixmapInfo(&dst);
    dst_info.ref_r = 0xFFFF;
    dst_info.ref_g = 0xFFFF;
    dst_info.ref_b = 0xFFFF;
    render_ref();
  } else {
    //render_ref();
    render_op1();
  }

  _controlfp(round_mode, _MCW_RC);
}

//-------------------------------------------------------------------------------------------------

const char* desc = "Adjust brightness levels, saturation, white balance, individual colors";
VDXFilterDefinition2 filterDef_sixaxis = VDXVideoFilterDefinition<LevelsFilter>("Anton", "6-axis color correction", desc);
VDXVF_BEGIN_SCRIPT_METHODS(LevelsFilter)
VDXVF_DEFINE_SCRIPT_METHOD(LevelsFilter, ScriptConfig, "dddddddddddddddddd")
VDXVF_END_SCRIPT_METHODS()

