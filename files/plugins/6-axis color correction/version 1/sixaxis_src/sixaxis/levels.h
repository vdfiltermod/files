#ifndef levels_header
#define levels_header

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include <math.h>

struct LevelsParam{
  struct Channel{
    float v0,v1,v2;
    double p;

    Channel(){ v0=0; v1=0.5; v2=1; p=1; }
    void validate(){ 
      if(v0<0) v0=0;
      if(v2>1) v2=1;
      if(v0>v2) v0=v2;
    }
    void eval_p(){
      p = log(v1)/log(0.5);
      if(p>10) p=10;
      if(p<0.01) p=0.01;
    }
  };

  Channel ci;
  Channel cs;
  Channel d0,d1;
  Channel cc[6];
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
  HPEN pen0;
  HPEN pen1;
  int ch;
  int edit_v0;
  int edit_v1;
  int edit_v2;
  float range0;
  float range1;
  LevelsParam::Channel* param;
  int track_value;
  int track_x;
  bool show_reset;

  LevelsControl(){ 
    owner=0; hwnd=0; base_proc=0; dc=0; bm=0; bm_p=0; ch=0; param=0; track_value=-1;
    show_reset=false;
    edit_v0=0;
    edit_v1=0;
    edit_v2=0;
    range0=0;
    range1=1;
    pen0=0;
    pen1=0;
  }
  ~LevelsControl();
  void init();
  void draw();
  void draw_marker(int x,int y);
  void draw_marker1(int x,int y);
  void begin_drag(int x,int y);
  void update_drag(int x,int y);

  int param_to_px(float v){
    v = (v-range0)/(range1-range0);
    return int(v*(w-20))+10; 
  }
  float px_to_param(int x){
    float v = float(x-10)/(w-20);
    v = v*(range1-range0)+range0;
    return float(floorf(v*1000)*0.001);
  }
  float px1_to_param(int x){
    float v = float(x-10)/(w-20);
    v = v*(range1-range0)+range0;
    v = (v-param->v0)/(param->v2-param->v0); 
    return float(floorf(v*1000)*0.001);
  }
};

class LevelsDialog: public VDXVideoFilterDialog{
public:
  LevelsFilter* filter;
  LevelsParam old_param;
	IVDXFilterPreview* const ifp;

  LevelsControl ci,cs,cd0,cd1,cc[6];
  HBRUSH edit_br;

  LevelsDialog(IVDXFilterPreview* ifp): ifp(ifp){ edit_br=0; }
  ~LevelsDialog(){ if(edit_br) DeleteObject(edit_br); }
	bool Show(HWND parent);
	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void init_edit(int id);
  void init_value(int id, float v);
  bool modify_value(HWND item, float& v);
  void init_levels(int id, LevelsControl& obj);
  void validate_v0(LevelsControl& a){
    if(a.param->v0<a.range0) a.param->v0=a.range0;
    if(a.param->v0>a.range1) a.param->v0=a.range1;
    if(a.param->v0>a.param->v2){
      a.param->v2 = a.param->v0;
      init_value(a.edit_v2,a.param->v2);
    }
  }
  void validate_v1(LevelsControl& a){
    if(a.param->v1<0) a.param->v1=0;
    if(a.param->v1>1) a.param->v1=1;
  }
  void validate_v2(LevelsControl& a){
    if(a.param->v2<a.range0) a.param->v2=a.range0;
    if(a.param->v2>a.range1) a.param->v2=a.range1;
    if(a.param->v2<a.param->v0){
      a.param->v0 = a.param->v2;
      init_value(a.edit_v0,a.param->v0);
    }
  }
  bool modify_param(LevelsControl& a, HWND item, int id);
  void RedoFrame();
};

const int color_map_size = 64;

class LevelsFilter : public VDXVideoFilter {
public:

  LevelsParam param;
  int64 kPixFormat_XRGB64;
  unsigned char* buf;
  uint16 luma[2048];
  uint16 color[color_map_size][color_map_size];
  bool have_color;

  LevelsFilter(){ init(); }
  LevelsFilter(const LevelsFilter& a){ param=a.param; init(); }
  ~LevelsFilter(){ free(buf); }
  void initParam();
  void init();
	virtual uint32 GetParams();
	virtual void Start();
	virtual void Run();
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen){
	  SafePrintf(buf, maxlen, "", 0);
  }
  virtual void GetScriptString(char* buf, int maxlen);
  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc);
  VDXVF_DECLARE_SCRIPT_METHODS();

  float eval_color_level(float u, float v);
  float eval_color_level(float d, LevelsParam::Channel& c);
  void eval_tables();

  void render_ref();
  void render_op1();
};

const UINT WM_EDIT_CHANGED = WM_USER+666;
LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam);

#endif
