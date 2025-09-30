#ifndef hqdn3d_header
#define hqdn3d_header

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include "vf_hqdn3d_init.h"

class Filter;

struct Param{
  double luma_strength;
  double chroma_strength;
  double tluma_strength;
  double tchroma_strength;
  int tdepth;
  bool recursive;

  Param(){
    luma_strength = 3;
    chroma_strength = 0;
    tluma_strength = 3;
    tchroma_strength = 0;
    tdepth = 1;
    recursive = false;
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
  void init_param(double v, int id);
  void init_param_slider(double v, int id);
  void init_edit(int id);
  void change_scroll(int id, int edit_id, double& v);
  void change_edit(int id, int edit_id, double& v);
  void init_tdepth();
  void init_recursive();
};

#define LUMA_SPATIAL   0
#define LUMA_TMP       1
#define CHROMA_SPATIAL 2
#define CHROMA_TMP     3

#define AV_CEIL_RSHIFT(a,b) -((-(a)) >> (b))

int16_t *precalc_coefs(double dist25, int depth);

class Filter: public VDXVideoFilter {
public:
  Param param;
  HQDN3DContext ctx;

  Filter(){ init(); }
  Filter(const Filter& a){ init(); param=a.param; }
  void init(){ memset(&ctx,0,sizeof(ctx)); }
  ~Filter(){}
  virtual uint32 GetParams();
  virtual void Start();
  virtual void Run();
  virtual bool Prefetch2(sint64 frame, IVDXVideoPrefetcher* prefetcher);
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen){
    if(param.recursive)
      SafePrintf(buf, maxlen, " (luma=%1.3f, chroma=%1.3f, t.luma=%1.3f, t.chroma=%1.3f, recursive)", param.luma_strength, param.chroma_strength, param.tluma_strength, param.tchroma_strength);
    else
      SafePrintf(buf, maxlen, " (luma=%1.3f, chroma=%1.3f, t.luma=%1.3f, t.chroma=%1.3f, t=%d)", param.luma_strength, param.chroma_strength, param.tluma_strength, param.tchroma_strength, param.tdepth);
  }
  virtual void GetScriptString(char* buf, int maxlen){
    SafePrintf(buf, maxlen, "Config(%1.3f,%1.3f,%1.3f,%1.3f,%d)", param.luma_strength, param.chroma_strength, param.tluma_strength, param.tchroma_strength, param.recursive ? -1 : param.tdepth);
  }
  VDXVF_DECLARE_SCRIPT_METHODS();

  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
    param.luma_strength = argv[0].asDouble();
    param.chroma_strength = argv[1].asDouble();
    param.tluma_strength = argv[2].asDouble();
    param.tchroma_strength = argv[3].asDouble();
    param.tdepth = argv[4].asInt();
    if(param.tdepth<0){
      param.recursive = true;
      param.tdepth = 1;
    }
  }

  void config_input();
  void uninit();
  void reset();
  void render_op0();
};

const UINT WM_EDIT_CHANGED = WM_USER+666;
LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam);

#endif
