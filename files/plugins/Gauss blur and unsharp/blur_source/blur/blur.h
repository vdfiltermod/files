#ifndef blur_header
#define blur_header

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>

struct BlurKernel{
  int w;
  int w0;
  int sum;
  unsigned short* data;
  unsigned short* data2;
  unsigned short* data3;
  unsigned char* buf2;
  unsigned char* buf3;

  BlurKernel(){ data=0; buf2=0; buf3=0; }
  ~BlurKernel(){ free(data); free(buf2); free(buf3); }
  void eval(float r);
};

const int size_scale = 16;

class BlurFilter;

class BlurDialog: public VDXVideoFilterDialog{
public:
  BlurFilter* filter;
  int old_size;
	IVDXFilterPreview* const ifp;

  BlurDialog(IVDXFilterPreview* ifp): ifp(ifp){}
	bool Show(HWND parent);
	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void init_size();
  void init_kernel();
};

class BlurFilter : public VDXVideoFilter {
public:

  int size;
  BlurKernel bk;
  unsigned char* row;
  unsigned short* buf;
  int buf_pitch; // in shorts
  int slice_h;

  BlurFilter(){ init(); }
  BlurFilter(const BlurFilter& a){ init(); size=a.size; }
  void init(){ size=1*size_scale; row=0; buf=0; }
  ~BlurFilter(){ free(row); free(buf); }
	virtual uint32 GetParams();
	virtual void Start();
	virtual void Run();
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen){
	  SafePrintf(buf, maxlen, " (%1.1f)", float(size)/size_scale);
  }
  virtual void GetScriptString(char* buf, int maxlen){
	  SafePrintf(buf, maxlen, "Config(%d)", size);
  }
  VDXVF_DECLARE_SCRIPT_METHODS();

  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
    size = argv[0].asInt();
  }

  void reset();

  void render_skip();
  void render_op0();
  void render_op1();
  void render_op2();

  void op2_slice_hpass(int slice_y0, int slice_y1);
  void op2_slice_vpass(int slice_y0, int slice_y1);
  void op2_slice_vpass_wide(int slice_y0, int slice_y1);
};

const UINT WM_EDIT_CHANGED = WM_USER+666;
LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam);

#endif
