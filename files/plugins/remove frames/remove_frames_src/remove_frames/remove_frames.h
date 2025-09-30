#ifndef levels_header
#define levels_header

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include <math.h>
#include <vector>

class DiffFilter;

struct DiffParam{
  int threshold;
  int mode;

  DiffParam(){
    threshold = 0;
    mode = 0;
  }
};

class DiffDialog: public VDXVideoFilterDialog{
public:
  DiffFilter* filter;
  DiffParam old_param;
  IVDXFilterPreview* const ifp;

  DiffDialog(IVDXFilterPreview* ifp): ifp(ifp){}
  bool Show(HWND parent);
  virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void init_edit(int id);
  void init_msg();
  void load_mode();
};

class DiffFilter : public VDXVideoFilter {
public:
  DiffParam param;

  std::vector<int> stat;
  std::vector<bool> kill;
  std::vector<int> new_index;
  int kill_count;
  bool scan_mode;

  DiffFilter(){ init(); }
  DiffFilter(const DiffFilter& a){
    param = a.param; init();
    stat = a.stat;
    kill = a.kill;
    new_index = a.new_index;
    kill_count = a.kill_count;
  }
  void init();
	virtual uint32 GetParams();
	virtual void Start();
	virtual void Run();
  virtual bool Prefetch2(sint64 frame, IVDXVideoPrefetcher* prefetcher);
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen);
  virtual void GetScriptString(char* buf, int maxlen);
  VDXVF_DECLARE_SCRIPT_METHODS();
  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc);

  void render();
  void collect_stat();
  void redo_stat();
  void load_kill(const std::string& s);
  void load_kill_from_file(const wchar_t* path);
};

const UINT WM_EDIT_CHANGED = WM_USER+666;
LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam);

#endif
