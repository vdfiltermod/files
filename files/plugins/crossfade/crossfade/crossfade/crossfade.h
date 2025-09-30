#ifndef crossfade_header
#define crossfade_header

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>

class Filter;

struct Param{
  sint64 pos;
  int width;

  Param(){
    pos = -1;
    width = 15;
  }
};

class ConfigDialog: public VDXVideoFilterDialog{
public:
  Filter* filter;
  Param old_param;
  IVDXFilterPreview* const ifp;

  ConfigDialog(IVDXFilterPreview* ifp): ifp(ifp){}
  bool Show(HWND parent);
  virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void init_width();
  void init_pos();
};

class Filter: public VDXVideoFilter {
public:
  Param param;

  Filter(){ init(); }
  Filter(const Filter& a){ init(); param=a.param; }
  void init(){}
  ~Filter(){}
  virtual uint32 GetParams();
  virtual void Start();
  virtual bool Prefetch2(sint64 frame, IVDXVideoPrefetcher* prefetcher);
  virtual void Run();
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen){
    SafePrintf(buf, maxlen, " (pos=%I64d, width=%d)", param.pos, param.width);
  }
  virtual void GetScriptString(char* buf, int maxlen){
    SafePrintf(buf, maxlen, "Config(%I64d,%d)", param.pos, param.width);
  }
  VDXVF_DECLARE_SCRIPT_METHODS();

  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
    param.pos = argv[0].asLong();
    param.width = argv[1].asInt();
  }

  void render();
};

const UINT WM_EDIT_CHANGED = WM_USER+666;
LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam);

#endif
