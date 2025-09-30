#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

#include "resource.h"
#include "levels.h"

extern HINSTANCE hInstance;

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
}

void LevelsControl::draw()
{
  PAINTSTRUCT ps;
  BeginPaint(hwnd,&ps);
  RECT r;
  GetClientRect(hwnd,&r);

  int c0 = GetSysColor(COLOR_BTNFACE);
  int bg_color = ((c0 & 0xFF)<<16) | (c0 & 0xFF00) | ((c0 & 0xFF0000)>>16);

  int color_ofs[] = {16,8,0};
  int h0 = h/2;
  int x0 = param_to_px(0);
  int x1 = param_to_px(1);

  {for(int y=0; y<h; y++){
    uint32* d = (uint32*)bm_p+y*w;
    {for(int x=0; x<w; x++){
      *d = bg_color;
      d++;
    }}
  }}

  {for(int x=x0; x<=x1; x++){
    int c1 = 0xff*(x-x0)/(x1-x0);
    int color = c1<<color_ofs[ch];
    uint32* d = (uint32*)bm_p+x;
    {for(int y=0; y<h0; y++){
      *d = color;
      d += w;
    }}
  }}

  draw_marker(param_to_px(param->v0),h0);
  draw_marker(param_to_px(param->v2),h0);

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

void LevelsControl::begin_drag(int x,int y)
{
  int x0 = param_to_px(param->v0);
  int x2 = param_to_px(param->v2);
  int m02 = (x0+x2)/2;
  
  if(x>x0-10 && x<x0+10 && x<m02){ track_value = 0; track_x = x-x0; }
  if(x>x2-10 && x<x2+10 && x>=m02){ track_value = 2; track_x = x-x2; }
}

void LevelsControl::update_drag(int x,int y)
{
  float v = px_to_param(x-track_x);
  if(track_value==0){ 
    param->v0 = v; 
    owner->validate_v0(*param,edit_v2);
    owner->init_value(edit_v0,param->v0);
  }
  if(track_value==2){ 
    param->v2 = v; 
    owner->validate_v2(*param,edit_v0); 
    owner->init_value(edit_v2,param->v2);
  }
  InvalidateRect(hwnd,0,false);
  RedrawWindow(hwnd,0,0,RDW_UPDATENOW);
  owner->ifp->RedoFrame();
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
    if(val<0) val = 0;
    if(val>1) val = 1;
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
}

bool LevelsDialog::Show(HWND parent){
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_LEVELS), parent);
}

INT_PTR LevelsDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
			ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));

      init_edit(IDC_R_V0);
      init_edit(IDC_R_V1);
      init_edit(IDC_R_V2);
      init_edit(IDC_G_V0);
      init_edit(IDC_G_V1);
      init_edit(IDC_G_V2);
      init_edit(IDC_B_V0);
      init_edit(IDC_B_V1);
      init_edit(IDC_B_V2);

      init_value(IDC_R_V0,filter->param.r.v0);
      init_value(IDC_R_V1,filter->param.r.v1);
      init_value(IDC_R_V2,filter->param.r.v2);
      init_value(IDC_G_V0,filter->param.g.v0);
      init_value(IDC_G_V1,filter->param.g.v1);
      init_value(IDC_G_V2,filter->param.g.v2);
      init_value(IDC_B_V0,filter->param.b.v0);
      init_value(IDC_B_V1,filter->param.b.v1);
      init_value(IDC_B_V2,filter->param.b.v2);
      EnableWindow(GetDlgItem(mhdlg,IDC_R_V1),false);
      EnableWindow(GetDlgItem(mhdlg,IDC_G_V1),false);
      EnableWindow(GetDlgItem(mhdlg,IDC_B_V1),false);

      cr.ch = 0; cr.param = &filter->param.r; cr.edit_v0 = IDC_R_V0; cr.edit_v2 = IDC_R_V2;
      cg.ch = 1; cg.param = &filter->param.g; cg.edit_v0 = IDC_G_V0; cg.edit_v2 = IDC_G_V2;
      cb.ch = 2; cb.param = &filter->param.b; cb.edit_v0 = IDC_B_V0; cb.edit_v2 = IDC_B_V2;
      init_levels(IDC_R_CONTROL,cr);
      init_levels(IDC_G_CONTROL,cg);
      init_levels(IDC_B_CONTROL,cb);
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
  
  case WM_EDIT_CHANGED:
    {
      HWND item = (HWND)lParam;
      bool changed = false;
      if(wParam==IDC_R_V0) if(modify_value(item,filter->param.r.v0)){ changed=true; validate_v0(filter->param.r,IDC_R_V2); }
      if(wParam==IDC_R_V1) if(modify_value(item,filter->param.r.v1)) changed=true;
      if(wParam==IDC_R_V2) if(modify_value(item,filter->param.r.v2)){ changed=true; validate_v2(filter->param.r,IDC_R_V0); }

      if(wParam==IDC_G_V0) if(modify_value(item,filter->param.g.v0)){ changed=true; validate_v0(filter->param.g,IDC_G_V2); }
      if(wParam==IDC_G_V1) if(modify_value(item,filter->param.g.v1)) changed=true;
      if(wParam==IDC_G_V2) if(modify_value(item,filter->param.g.v2)){ changed=true; validate_v2(filter->param.g,IDC_G_V0); }

      if(wParam==IDC_B_V0) if(modify_value(item,filter->param.b.v0)){ changed=true; validate_v0(filter->param.b,IDC_B_V2); }
      if(wParam==IDC_B_V1) if(modify_value(item,filter->param.b.v1)) changed=true;
      if(wParam==IDC_B_V2) if(modify_value(item,filter->param.b.v2)){ changed=true; validate_v2(filter->param.b,IDC_B_V0); }

      if(changed){
        if(wParam==IDC_R_V0 || wParam==IDC_R_V1 || wParam==IDC_R_V2) InvalidateRect(cr.hwnd,0,false);
        if(wParam==IDC_G_V0 || wParam==IDC_G_V1 || wParam==IDC_G_V2) InvalidateRect(cg.hwnd,0,false);
        if(wParam==IDC_B_V0 || wParam==IDC_B_V1 || wParam==IDC_B_V2) InvalidateRect(cb.hwnd,0,false);
        ifp->RedoFrame();
      }
      return TRUE;
    }
  }

  return FALSE;
}

//-------------------------------------------------------------------------------------------------

void LevelsFilter::init()
{
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
}

void LevelsFilter::Run()
{
	if(sAPIVersion<12) return;

  if(fa->src.mpPixmapLayout->format==kPixFormat_XRGB64){
    render_xrgb64();
    return;
  }

  render_xrgb8();
}

//-------------------------------------------------------------------------------------------------

VDXFilterDefinition2 filterDef_levels = VDXVideoFilterDefinition<LevelsFilter>("Anton", "rgb levels", "rgb levels");
VDXVF_BEGIN_SCRIPT_METHODS(LevelsFilter)
VDXVF_DEFINE_SCRIPT_METHOD(LevelsFilter, ScriptConfig, "ddddddddd")
VDXVF_END_SCRIPT_METHODS()

