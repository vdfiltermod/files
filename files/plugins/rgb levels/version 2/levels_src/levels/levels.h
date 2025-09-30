#ifndef levels_header
#define levels_header

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include <math.h>

struct LevelsParam{
  struct Channel{
    float v0,v1,v2;

    Channel(){ v0=0; v1=1; v2=1; }
    void validate(){ 
      if(v0<0) v0=0;
      if(v2>1) v2=1;
      if(v0>v2) v0=v2;
      v1=1;
    }
  };

  Channel r,g,b;
};

class LevelsFilter;
class LevelsDialog;

struct LevelsControl{
  LevelsDialog* owner;
  HWND hwnd;
  WNDPROC base_proc;
  HDC dc;
  int w,h;
  HBITMAP bm;
  void* bm_p;
  int ch;
  int edit_v0;
  int edit_v2;
  LevelsParam::Channel* param;
  int track_value;
  int track_x;

  LevelsControl(){ owner=0; hwnd=0; base_proc=0; dc=0; bm=0; bm_p=0; ch=0; param=0; track_value=-1; }
  ~LevelsControl();
  void init();
  void draw();
  void draw_marker(int x,int y);
  void begin_drag(int x,int y);
  void update_drag(int x,int y);

  int param_to_px(float v){
    return int(v*(w-20))+10; 
  }
  float px_to_param(int x){
    float v = float(x-10)/(w-20);
    return float(floorf(v*1000)*0.001);
  }
};

class LevelsDialog: public VDXVideoFilterDialog{
public:
  LevelsFilter* filter;
  LevelsParam old_param;
	IVDXFilterPreview* const ifp;

  LevelsControl cr,cg,cb;

  LevelsDialog(IVDXFilterPreview* ifp): ifp(ifp){}
	bool Show(HWND parent);
	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void init_edit(int id);
  void init_value(int id, float v);
  bool modify_value(HWND item, float& v);
  void init_levels(int id, LevelsControl& obj);
  void validate_v0(LevelsParam::Channel& a, int id2){
    if(a.v0<0) a.v0=0;
    if(a.v0>1) a.v0=1;
    if(a.v0>a.v2){
      a.v2=a.v0;
      init_value(id2,a.v2);
    }
  }
  void validate_v2(LevelsParam::Channel& a, int id0){
    if(a.v2<0) a.v2=0;
    if(a.v2>1) a.v2=1;
    if(a.v2<a.v0){
      a.v0=a.v2;
      init_value(id0,a.v0);
    }
  }
};

class LevelsFilter : public VDXVideoFilter {
public:

  LevelsParam param;
  int64 kPixFormat_XRGB64;

  LevelsFilter(){ init(); }
  LevelsFilter(const LevelsFilter& a){ param=a.param; init(); }
  void init();
	virtual uint32 GetParams();
	virtual void Start();
	virtual void Run();
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen){
	  SafePrintf(buf, maxlen, "", 0);
  }
  virtual void GetScriptString(char* buf, int maxlen){
	  SafePrintf(buf, maxlen, "Config(%g,%g,%g, %g,%g,%g, %g,%g,%g)", param.r.v0, param.r.v1, param.r.v2, param.g.v0, param.g.v1, param.g.v2, param.b.v0, param.b.v1, param.b.v2);
  }
  VDXVF_DECLARE_SCRIPT_METHODS();

  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
    param.r.v0 = float(argv[0].asDouble());
    param.r.v1 = float(argv[1].asDouble());
    param.r.v2 = float(argv[2].asDouble());
    param.g.v0 = float(argv[3].asDouble());
    param.g.v1 = float(argv[4].asDouble());
    param.g.v2 = float(argv[5].asDouble());
    param.b.v0 = float(argv[6].asDouble());
    param.b.v1 = float(argv[7].asDouble());
    param.b.v2 = float(argv[8].asDouble());
    param.r.validate();
    param.g.validate();
    param.b.validate();
  }

  void render_xrgb8();
  void render_xrgb64();
};

const UINT WM_EDIT_CHANGED = WM_USER+666;
LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam);

#endif
