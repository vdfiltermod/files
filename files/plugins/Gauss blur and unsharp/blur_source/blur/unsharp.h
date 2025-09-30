#ifndef unsharp_header
#define unsharp_header

#include "blur.h"

const int power_scale=256;

class UnsharpFilter;

class UnsharpDialog: public VDXVideoFilterDialog{
public:
  UnsharpFilter* filter;
  int old_size;
  int old_power;
  int old_threshold;
	IVDXFilterPreview* const ifp;

  UnsharpDialog(IVDXFilterPreview* ifp): ifp(ifp){}
	bool Show(HWND parent);
	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void init_size();
  void init_power();
  void init_threshold();
  void init_kernel();
};

class UnsharpFilter : public BlurFilter {
public:
  int power;
  int threshold;

  UnsharpFilter(){ init(); }
  UnsharpFilter(const UnsharpFilter& a){ init(); size=a.size; power=a.power; threshold=a.threshold; }
  void init(){ BlurFilter::init(); size=1*size_scale; power=1*power_scale; threshold=0; }
	virtual void Run();
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen){
	  SafePrintf(buf, maxlen, " (%1.1f, %1.1f)", float(size)/size_scale, float(power)/power_scale);
  }
  virtual void GetScriptString(char* buf, int maxlen){
	  SafePrintf(buf, maxlen, "Config(%d,%d,%d)", size, power, threshold);
  }
  VDXVF_DECLARE_SCRIPT_METHODS();

  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
    size = argv[0].asInt();
    power = argv[1].asInt();
    threshold = argv[2].asInt();
  }

  void render_op0();
  void render_op2();
  void op2_slice_hpass_unsharp(int slice_y0, int slice_y1);
  void op2_slice_hpass_unsharp2(int slice_y0, int slice_y1);
};

#endif
