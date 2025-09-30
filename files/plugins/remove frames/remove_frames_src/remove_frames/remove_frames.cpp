#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

#include "resource.h"
#include "remove_frames.h"

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

bool OpenFile(HWND hwnd, wchar_t* path, int max_path) {
  OPENFILENAMEW ofn = {0};
  wchar_t szFile[MAX_PATH];

  if (path)
    wcscpy(szFile,path);
  else
    szFile[0] = 0;

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter = L"text files (txt)\0*.txt\0All files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_ENABLETEMPLATE;
  ofn.lpTemplateName = MAKEINTRESOURCEW(IDD_FILE);
  ofn.hInstance = hInstance;

  BOOL result = GetOpenFileNameW(&ofn);

  if(!result && CommDlgExtendedError()){
    szFile[0] = 0;
    ofn.lpstrInitialDir = NULL;
    result = GetOpenFileNameW(&ofn);
  }

  if(result){
    wcscpy(path,szFile);
    return true;
  }

  return false;
}

//-------------------------------------------------------------------------------------------------

void DiffDialog::init_edit(int id)
{
  HWND hWnd = GetDlgItem(mhdlg,id);
  LPARAM p = GetWindowLongPtr(hWnd,GWLP_WNDPROC);
  SetWindowLongPtr(hWnd,GWLP_USERDATA,p);
  SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LPARAM)EditWndProc);
}

bool DiffDialog::Show(HWND parent){
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_DIFF), parent);
}

void DiffDialog::init_msg()
{
  char buf[100];
  if(filter->kill_count)
    sprintf(buf,"Frames to remove: %d",filter->kill_count);
  else
    buf[0] = 0;
  SetDlgItemText(mhdlg,IDC_INFO,buf);
}

void DiffDialog::load_mode()
{
  int mode = 0;
  if(IsDlgButtonChecked(mhdlg,IDC_OPTION_FILL)) mode = 1;
  if(IsDlgButtonChecked(mhdlg,IDC_OPTION_DELETE)) mode = 2;
  if(mode==2 || filter->param.mode==2){
    filter->param.mode = mode;
    ifp->RedoSystem();
  } else {
    filter->param.mode = mode;
    ifp->RedoFrame();
  }
}

INT_PTR DiffDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
      ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));
      init_edit(IDC_THRESHOLD);
      SetDlgItemInt(mhdlg,IDC_THRESHOLD,filter->param.threshold,true);
      EnableWindow(GetDlgItem(mhdlg,IDC_THRESHOLD),!filter->stat.empty());
      init_msg();
      CheckDlgButton(mhdlg,IDC_OPTION_NONE+filter->param.mode,BST_CHECKED);
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
    case IDC_ANALYSE:
      filter->collect_stat();
      init_msg();
      EnableWindow(GetDlgItem(mhdlg,IDC_THRESHOLD),!filter->stat.empty());
      if(filter->param.mode==2) ifp->RedoSystem(); else ifp->RedoFrame();
      return TRUE;
    case IDC_LOAD:
      {
        wchar_t path[MAX_PATH];
        path[0] = 0;
        if(OpenFile(mhdlg, path, MAX_PATH)){
          filter->load_kill_from_file(path);
          filter->redo_stat();
          init_msg();
          EnableWindow(GetDlgItem(mhdlg,IDC_THRESHOLD),!filter->stat.empty());
          if(filter->param.mode==2) ifp->RedoSystem(); else ifp->RedoFrame();
        }
      }
      return TRUE;
    case IDC_OPTION_NONE:
    case IDC_OPTION_FILL:
    case IDC_OPTION_DELETE:
      load_mode();
      return TRUE;
    }
    return TRUE;
  
  case WM_EDIT_CHANGED:
    {
      HWND item = (HWND)lParam;
      BOOL changed = false;
      int value = GetDlgItemInt(mhdlg,IDC_THRESHOLD,&changed,true);
      if(changed){
        filter->param.threshold = value;
        filter->redo_stat();
        init_msg();
        if(filter->param.mode==2) ifp->RedoSystem(); else ifp->RedoFrame();
      }
      return TRUE;
    }
  }

  return FALSE;
}

//-------------------------------------------------------------------------------------------------

void DiffFilter::init()
{
  kill_count = 0;
  scan_mode = false;
}

bool DiffFilter::Configure(VDXHWND hwnd)
{
  DiffDialog dlg(fa->ifp);
  dlg.filter = this;
  dlg.old_param = param;
  return dlg.Show((HWND)hwnd);
}

uint32 DiffFilter::GetParams() {
  if(!scan_mode && param.mode==2){
    sint64 frames = fa->dst.mFrameCount;
    if(frames >= 0) fa->dst.mFrameCount = frames-kill_count;
    if(frames-kill_count>0){
      double oldRate = (double)fa->dst.mFrameRateHi / (double)fa->dst.mFrameRateLo;
      double newRate = oldRate * (frames-kill_count) / frames;
      fa->dst.mFrameRateHi = int(newRate*1000+0.5);
      fa->dst.mFrameRateLo = 1000;
    }
  }

  int format = ExtractBaseFormat(fa->src.mpPixmapLayout->format);

  if (sAPIVersion >= 12) {
    switch(format) {
    case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_XRGB8888:
    case nsVDXPixmap::kPixFormat_YUV444_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Planar:
    case nsVDXPixmap::kPixFormat_Y8:
    case nsVDXPixmap::kPixFormat_Y8_FR:
      break;

    default:
      return FILTERPARAM_NOT_SUPPORTED;
    }
  }

  fa->dst.offset = 0;
  return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void DiffFilter::Start()
{
}

bool DiffFilter::Prefetch2(sint64 frame, IVDXVideoPrefetcher* prefetcher)
{
  if(scan_mode){
    prefetcher->PrefetchFrameSymbolic(0, frame);
    prefetcher->PrefetchFrame(0, frame, 0);
    prefetcher->PrefetchFrame(0, frame-1, 0);
  } else if(param.mode==2 && kill_count && frame>=0){
    sint64 frame2;
    unsigned int n = new_index.size();
    if(frame<n)
      frame2 = new_index[(unsigned int)frame];
    else
      frame2 = frame-n+new_index[n-1];
    prefetcher->PrefetchFrameDirect(0, frame2);
  } else {
    prefetcher->PrefetchFrameDirect(0, frame);
  }
  return true;
}

void DiffFilter::Run()
{
  if(sAPIVersion<12) return;

  render();
}

void DiffFilter::redo_stat()
{
  if(!stat.empty()){
    kill.resize(stat.size());
    new_index.resize(stat.size());
    kill_count = 0;
    {for(unsigned int i=0; i<stat.size(); i++){
      bool x = stat[i]<param.threshold;
      kill[i] = x;
      if(!x) new_index[i-kill_count] = i;
      if(x) kill_count++;
    }}
    new_index.resize(stat.size()-kill_count);
  } else {
    new_index.resize(kill.size());
    kill_count = 0;
    {for(unsigned int i=0; i<kill.size(); i++){
      bool x = kill[i];
      if(!x) new_index[i-kill_count] = i;
      if(x) kill_count++;
    }}
    new_index.resize(kill.size()-kill_count);
  }
}

void DiffFilter::GetSettingString(char* buf, int maxlen){
  SafePrintf(buf, maxlen, " (mode %d)", param.mode);
}

const wchar_t* stat_id = L"frame_stat.txt";
const wchar_t* kill_id = L"kill_list.txt";

void DiffFilter::GetScriptString(char* buf, int maxlen){
  SafePrintf(buf, maxlen, "Config(%d,%d)", param.mode, param.threshold);
  if(fma && fma->fmproject){
    if(!stat.empty()){
      std::string s;
      s.reserve(stat.size()*5);
      char buf[20];
      {for(unsigned int i=0; i<stat.size(); i++){
        sprintf(buf,"%d\r\n",stat[i]);
        s+=buf;
      }}
      fma->fmproject->SetData(s.c_str(),s.length(),stat_id);
    } else {
      fma->fmproject->SetData(0,0,stat_id);
    }
    if(stat.empty() && kill_count){
      std::string s;
      s.reserve(kill_count*5);
      char buf[20];
      {for(unsigned int i=0; i<kill.size(); i++){
        if(kill[i]){
          sprintf(buf,"%d\r\n",i);
          s+=buf;
        }
      }}
      fma->fmproject->SetData(s.c_str(),s.length(),kill_id);
    } else {
      fma->fmproject->SetData(0,0,kill_id);
    }
  }
}

void load_list(const std::string& s, std::vector<int>& list)
{
  const char* p = s.c_str();
  while(1){
    while(1){
      if(*p=='\r' || *p=='\n' || *p==' ' || *p=='\t') p++; else break;
    }
    if(*p==0) break;
    int v;
    if(sscanf(p,"%d",&v)==1) list.push_back(v);
    while(1){
      if(*p=='\r' || *p=='\n' || *p==0) break; else p++;
    }
  }
}

void DiffFilter::load_kill(const std::string& s)
{
  std::vector<int> list;
  load_list(s,list);
  int last = -1;
  {for(unsigned int i=0; i<list.size(); i++){
    int frame = list[i];
    if(frame>last) last = frame;
  }}
  if(last==-1) return;
  kill.resize(last+1,false);
  {for(unsigned int i=0; i<list.size(); i++){
    int frame = list[i];
    kill[frame] = true;
  }}
}

void DiffFilter::load_kill_from_file(const wchar_t* path)
{
  stat.resize(0);
  kill.resize(0);
  new_index.resize(0);
  kill_count = 0;

  HANDLE h = CreateFileW(path,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
  if(h==INVALID_HANDLE_VALUE) return;

  DWORD size = GetFileSize(h,0);
  DWORD r;
  std::string s;
  s.resize(size);
  ReadFile(h,&s[0],size,&r,0);
  CloseHandle(h);
  load_kill(s);
}

void DiffFilter::ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
  param.mode = argv[0].asInt();
  param.threshold = argv[1].asInt();
  stat.resize(0);
  kill.resize(0);
  if(fma && fma->fmproject){
    size_t size = 0;
    fma->fmproject->GetData(0,&size,stat_id);
    if(size){
      std::string s(size+1,0);
      fma->fmproject->GetData(&s[0],&size,stat_id);
      load_list(s,stat);
    } else {
      fma->fmproject->GetData(0,&size,kill_id);
      if(size){
        std::string s(size+1,0);
        fma->fmproject->GetData(&s[0],&size,kill_id);
        load_kill(s);
      }
    }
  }
  redo_stat();
}

//-------------------------------------------------------------------------------------------------

VDXFilterDefinition2 filterDef_diff = VDXVideoFilterDefinition<DiffFilter>("Anton", "remove frames", "Deletes duplicate frames (to undo framerate stretch)");
VDXVF_BEGIN_SCRIPT_METHODS(DiffFilter)
VDXVF_DEFINE_SCRIPT_METHOD(DiffFilter, ScriptConfig, "ii")
VDXVF_END_SCRIPT_METHODS()

