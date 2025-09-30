#ifndef owdenoise_header
#define owdenoise_header

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>

class Filter;

struct Param{
  double luma_strength;
  double chroma_strength;
  int depth;

  Param(){
    depth = 8;
    luma_strength = 0.01;
    chroma_strength = 0;
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
  void init_depth();
  void init_luma();
  void init_chroma();
};

typedef struct {
  double luma_strength;
  double chroma_strength;
  int depth;
  float *plane[16+1][4];
  int linesize;
  int hsub, vsub;
  int pixel_depth;
} OWDenoiseContext;

class Filter: public VDXVideoFilter {
public:
  Param param;
  OWDenoiseContext ctx;

  Filter(){ init(); }
  Filter(const Filter& a){ init(); param=a.param; }
  void init(){ ctx.depth=-1; }
  ~Filter(){}
  virtual uint32 GetParams();
  virtual void Start();
  virtual void Run();
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen){
    SafePrintf(buf, maxlen, " (depth=%d, luma=%1.3f, chroma=%1.3f)", param.depth, param.luma_strength, param.chroma_strength);
  }
  virtual void GetScriptString(char* buf, int maxlen){
    SafePrintf(buf, maxlen, "Config(%d,%1.3f,%1.3f)", param.depth, param.luma_strength, param.chroma_strength);
  }
  VDXVF_DECLARE_SCRIPT_METHODS();

  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
    param.depth = argv[0].asInt();
    param.luma_strength = argv[1].asDouble();
    param.chroma_strength = argv[1].asDouble();
  }

  void config_input();
  void uninit();
  void reset();
  void render_op0();
};

const UINT WM_EDIT_CHANGED = WM_USER+666;
LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam);

#endif
