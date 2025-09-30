#include <vd2/VDXFrame/VideoFilterEntry.h>
#include "blur.h"
#include "unsharp.h"

VDX_DECLARE_VIDEOFILTERS_BEGIN()
	VDX_DECLARE_VIDEOFILTER(filterDef_blur)
	VDX_DECLARE_VIDEOFILTER(filterDef_unsharp)
VDX_DECLARE_VIDEOFILTERS_END()

HINSTANCE hInstance;

BOOL WINAPI DllMain(HINSTANCE hModule,ULONG reason,void*) 
{
  switch(reason){
  case DLL_PROCESS_ATTACH:
    hInstance = hModule;
    break;
  }
  return(TRUE);
}

extern "C" int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(VDXFilterModule *fm, const VDXFilterFunctions *ff, int& vdfd_ver, int& vdfd_compat) {
  if(VDXVideoFilterModuleInit2(fm,ff,vdfd_ver)) return 1;

	vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
	vdfd_compat = 9;		// copy constructor support required
	return 0;
}

extern "C" void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(VDXFilterModule *fm, const VDXFilterFunctions *ff) {
}
