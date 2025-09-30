/*

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdio.h>
#include <cmath>                    // for sqrt() and pow()

#include "stdafx.h"

#include "resource.h"
#include <vd2/plugin/vdvideofilt.h>
#include "VdubGfxLib.h"
#include <windows.h>


typedef struct ChartData {
	unsigned long       Histogram[272];
	Pixel32 color;
	int min;
	int max;
} ChartData;

static char *hotFixNames[]={
	"To Black",
	"Intenstiy",
	"Saturation",
};

static char *IRENames[]={
	"100",
	"110",
	"120",
};

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

///////////////////////////////////////////////////////////////////////////

int histoRunProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int drawChart(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, int cd, int heightOffset, float scaleF);
int drawWFM(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, int cd, int heightOffset, float scaleF);
int histograms(const VDXFilterActivation *fa, const VDXFilterFunctions *ff); 
int wfm(const VDXFilterActivation *fa, const VDXFilterFunctions *ff); 
int vectorScope(const VDXFilterActivation *fa, const VDXFilterFunctions *ff); 
int hotPixels(const VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int showChannels(const VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int histoStartProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int histoEndProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int MyInitProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
long histoParamProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int histoConfigProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hwnd);
void histoStringProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *str);
void histoScriptConfig(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc);
bool histoFssProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen);
double gc(double val, double gamma, bool mode);
Pixel32 hot(Pixel32 p, const VDXFilterActivation *fa);
void xRectFill(const VDXFilterActivation *fa, PixCoord x, PixCoord y, PixDim dx, PixDim dy, Pixel32 c);


///////////////////////////////////////////////////////////////////////////

#define SCALE	16777216            /* scale factor: do floats with int math */
#define	SHIFT   24		   		 /* shift of scale 2^shift=scale */
#define SCALEhp	8192            /* scale factor for HotPixels */
//#define MAXPIX	 255            /* white value for Hot pix*/
#define	CHROMA_LIM      50.0		/* chroma amplitude limit */
#define	COMPOS_LIM      110.0		/* max IRE amplitude */



typedef struct MyFilterData {
  IVDXFilterPreview* ifp;

    bool    bACCUM;
    bool    bPASSTHRU;
	bool	bNTSC;
	bool	bGAMMA;
	bool	bSTDDEF;
	bool	bRNG255;
	bool	bLUMA;
	bool	bRED;
	bool	bGREEN;
	bool	bBLUE;
	bool	bWFMGRID;
	int		mode;
	int		hotFix;
	int		maxIRE;
	ChartData       chart[4];

	unsigned int	tab[3][3][256];    /* multiply lookup table */

	double	chroma_lim;             /* chroma limit */
	double	compos_lim;             /* composite amplitude limit */
	int	ichroma_lim2;           /* chroma limit squared (scaled integer) */
	int	icompos_lim;            /* composite amplitude limit (scaled integer) */
	int chartHeight;
	int y_offset;
	int clamp[5120];
	int numWFMs;
	double code_matrix[3][3];
	long avd;
	int circle[2][360];
	int Cb[511];
	int Cr[511];

} MyFilterData;


VDXScriptFunctionDef histo_func_defs[]={
    { (VDXScriptFunctionPtr)histoScriptConfig, "Config", "0iiiiiiiiiiiii" },
    { NULL },
};

VDXScriptObject histo_obj={
    NULL, histo_func_defs
};

struct VDXFilterDefinition filterDef_histo = {

    NULL, NULL, NULL,       // next, prev, module
    "Color Tools [1.5]",             // name
    "Histogrms, Hot Pixels, WFM, VectorScope",
                            // desc
    "trevlac",               // maker
    NULL,                   // private_data
    sizeof(MyFilterData),   // inst_data_size

	MyInitProc,				// initProc
    NULL,                   // deinitProc
    histoRunProc,        // runProc
    histoParamProc,      // paramProc
    histoConfigProc,     // configProc
    histoStringProc,     // stringProc
    histoStartProc,      // startProc
    histoEndProc,        // endProc

    &histo_obj,          // script_obj
    histoFssProc,        // fssProc

};

///////////////////////////////////////////////////////////////////////////

extern "C" int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(VDXFilterModule *fm, const VDXFilterFunctions *ff, int& vdfd_ver, int& vdfd_compat);
extern "C" void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(VDXFilterModule *fm, const VDXFilterFunctions *ff);

///////////////////////////////////////////////////////////////////////////

int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(VDXFilterModule *fm, const VDXFilterFunctions *ff, int& vdfd_ver, int& vdfd_compat) {
    if (!ff->addFilter(fm, &filterDef_histo, sizeof(VDXFilterDefinition)))
        return 1;

	vdfd_ver = 12;
	vdfd_compat = 9;

    return 0;
}

void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(VDXFilterModule *fm, const VDXFilterFunctions *ff) {
}

///////////////////////////////////////////////////////////////////////////

static bool g_MMXenabled;

int histoStartProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    //int i;

    g_MMXenabled = ff->isMMXEnabled();

    return 0;
}


//********** Registry Save Stuff ***************

const char* settings_key = "Software\\VirtualDub.org\\ColorTools";

int SaveKey(char* lpClass, char* lpData)
{
    const char* lpSubKey = settings_key;
    HKEY keyHandle;
    long rtn =0;
    DWORD lpdw;
    int aLen = 0; 

    //In order to set the value you must specify the length of the lpData.   
    aLen = (int)strlen(lpData) + 1; 

    //This will create the key if it does not exist or update it if it does.   
     rtn = RegCreateKeyEx(HKEY_CURRENT_USER,
            lpSubKey, 0, "Anything",
            REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &keyHandle, &lpdw); 

    if(rtn == ERROR_SUCCESS){
        //This will write the data to the newly created key or update it.
         rtn = RegSetValueEx(keyHandle, lpClass,
                    0, REG_SZ, (LPBYTE)lpData, aLen );
        //Good programming practice, close what you open.
        rtn = RegCloseKey(keyHandle);
    }
    return rtn;
} 


int LoadKey(LPCTSTR lpSubKey, int def_value)
{
    HKEY hKey = HKEY_CURRENT_USER;
    LPCTSTR sPsth = settings_key;
    TCHAR lpData[255];
    DWORD lpType, lpcbData =255;
    int rtn; 

    if(RegOpenKeyEx(hKey, sPsth, NULL,KEY_QUERY_VALUE, &hKey) == 0)
        if(RegQueryValueEx
            (hKey, lpSubKey, NULL, &lpType, (BYTE*)lpData, &lpcbData) == 0)
            rtn = atoi(lpData);
        else rtn = def_value;
    else rtn = def_value;
    RegCloseKey(hKey); 

    return rtn;
} 


void save_settings(MyFilterData *mfd)
{
	char str[10];
  sprintf( str,"%d",mfd->mode);
  SaveKey("mode", str);
  
  sprintf( str,"%d",mfd->bPASSTHRU);
  SaveKey("PASSTHRU", str);
  
  sprintf( str,"%d",mfd->bACCUM);
  SaveKey("ACCUM", str);
  
  sprintf( str,"%d",mfd->bNTSC);
  SaveKey("NTSC", str);
  
  sprintf( str,"%d",mfd->bGAMMA);
  SaveKey("GAMMA", str);
  
  sprintf( str,"%d",mfd->bSTDDEF);
  SaveKey("STDDEF", str);
  
  sprintf( str,"%d",mfd->bRNG255);
  SaveKey("RNG255", str);
  
  sprintf( str,"%d",mfd->bLUMA);
  SaveKey("LUMA", str);
  
  sprintf( str,"%d",mfd->bRED);
  SaveKey("RED", str);
  
  sprintf( str,"%d",mfd->bGREEN);
  SaveKey("GREEN", str);
  
  sprintf( str,"%d",mfd->bBLUE);
  SaveKey("BLUE", str);
  
  sprintf( str,"%d",mfd->bWFMGRID);
  SaveKey("WFMGRID", str);
  
  sprintf( str,"%d",mfd->hotFix);
  SaveKey("hotFix", str);
  
  sprintf( str,"%d",mfd->maxIRE);
  SaveKey("maxIRE", str);
}

void load_settings(MyFilterData *mfd)
{
  int rtn;
  
  rtn = LoadKey("mode",0);
  mfd->mode = (rtn>-1 && rtn<5)?rtn:0;
  
  rtn = LoadKey("PASSTHRU",0);
  mfd->bPASSTHRU = rtn==0 ? 0:1;

  rtn = LoadKey("ACCUM",0);
  mfd->bACCUM = rtn==0 ? 0:1;
  
  rtn = LoadKey("NTSC",1);
  mfd->bNTSC = rtn<=0 ? 0:1;

  rtn = LoadKey("GAMMA",1);
  mfd->bGAMMA = rtn<=0 ? 0:1;
  
  rtn = LoadKey("STDDEF",1);
  mfd->bSTDDEF = rtn<=0 ? 0:1;
  
  rtn = LoadKey("LUMA",1);
  mfd->bLUMA = rtn<=0 ? 0:1;
  
  rtn = LoadKey("RED",1);
  mfd->bRED = rtn<=0 ? 0:1;
  
  rtn = LoadKey("GREEN",1);
  mfd->bGREEN = rtn<=0 ? 0:1;
  
  rtn = LoadKey("BLUE",1);
  mfd->bBLUE = rtn<=0 ? 0:1;
  
  rtn = LoadKey("WFMGRID",1);
  mfd->bWFMGRID = rtn<=0 ? 0:1;
  
  rtn = LoadKey("RNG255",1);
  mfd->bRNG255 = rtn<=0 ? 0:1;
  
  rtn = LoadKey("maxIRE",110);
  mfd->maxIRE = (rtn>-1 && rtn<121)?rtn:110;
  
  rtn = LoadKey("hotFix",0);
  mfd->hotFix = (rtn>-1 && rtn<3)?rtn:0;
}

//======================= START ============
int histoRunProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	if(mfd->bPASSTHRU) return showChannels(fa, ff);

	 switch(mfd->mode) {
        case 0:	
			return histograms(fa, ff);
			break;

        case 1:	
			return hotPixels(fa, ff);
			break;
        case 2:	
			return showChannels(fa, ff);
			break;
        case 3:	
			return wfm(fa, ff);
			break;
        case 4:	
			return vectorScope(fa, ff);
			break;
	 }
	 return 0;

}

//==========Main Vector =============
int vectorScope(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

    const Pixel32 lnColor = 0xFFFFB700;
    const Pixel32 lnColor2 = 0xFF85EF21;
	int luma, r, g, b;
	int *_Cb = &mfd->Cb[256];
	int *_Cr = &mfd->Cr[256];


		int scaleX[60] = {-47,-68,-65,-45,-47
						,-123,-105,-98,-115,-123
						,170,173,163,160,170
						,123,105,98,115,123
						,47,68,64,44,47
						,-170,-173,-163,-160,-170 };

					//*	,-17,-115,-77,-12,-17	Points for the Outter boxes do
					//	,-181,-94,-55,-105,-181 not look quite right.
					//	,199,216,125,115,199
					//	,181,94,55,105,181
					//	,18,122,70,11,18
					//	,-199,-216,-125,-115,-199 }; 

						
						
	int scaleY[60] = { 243,238,225,230,243
						,-198,-208,-195,-186,-198
						,-47,-32,-30,-44,-47
						,198,208,195,186,198
						,-244,-238,-225,-230,-244
						,47,32,30,44,47 };

					//	,289,266,177,193,289
					//	,-221,-270,-156,-128,-221
					//	,-84,-11,-6,-49,-84
					//	,221,270,156,128,221
					//	,-305,-281,-162,-177,-305
					//	,84,11,6,49,84 }; 


    Pixel32 *src, *dst, *dst2, old_pixel;

	int ws = fa->src.w;
	int hs = fa->src.h;


	int w = fa->dst.w;
	int h = fa->dst.h;
	ptrdiff_t m = fa->dst.modulo;
	ptrdiff_t dpitch = fa->dst.pitch;

	dst = (Pixel32 *)fa->dst.data;

	//*** Clear the screen 
	xRectFill(fa,0,0, w,h,0x00);

	
	//*** Do the color lables 

	VGL_DrawChar( fa,314 -68, 314- 220, 'R', lnColor); 
	VGL_DrawChar( fa,314 -123,314- (-210), 'G', lnColor); 
	VGL_DrawChar( fa,314 +173,314- (-50), 'B', lnColor); 
	VGL_DrawChar( fa,314 +123,314- (185), 'M', lnColor); 
	VGL_DrawChar( fa,314 +68,314- (-245), 'C', lnColor); 
	VGL_DrawChar( fa,314 -175,314- (26), 'Y', lnColor); 




	//** Build overlay and scale
	for(int i=0; i<360; i=i+5) {
		
			//dst2 = dst +  (w+m) * (h - mfd->circle[1][i]);
			dst2 = dst +  mfd->circle[0][i] + (w+m) * (h - mfd->circle[1][i]);
			*dst2++ = lnColor;
			*dst2++ = lnColor;

			dst2 = dst +  mfd->circle[0][i] + (w+m) * (h - mfd->circle[1][i] -1);
			*dst2++ = lnColor;
			*dst2++ = lnColor;
			
	}

	xRectFill(fa,313,0, 1,628,lnColor);  //** Horizontal
	xRectFill(fa,0,314, 628, 1,lnColor); //** Vertical


	for(int nB=0;nB<5*12;nB=nB+5) {
		for(int i=0;i<4;i++) {
			VGL_DrawLine(fa, scaleX[i+nB] + 314, 314 - scaleY[i+nB] , scaleX[i+nB+1] + 314, 314 - scaleY[i+nB+1], lnColor);

	}}

	VGL_DrawLine(fa, -171 + 314, 314 - 263 , 314, 314, lnColor);

	
    src = (Pixel32 *)fa->src.data;

    //***** Chart the data
	do {
        ws = fa->src.w;
		do {
			old_pixel = *src++;

			r = old_pixel>>16 & 0xff;
			g = old_pixel>>8 & 0xff;
			b = old_pixel & 0xff;

			luma = ((unsigned int) ((mfd->tab[0][0][r] 
				+ mfd->tab[0][1][g] 
				+ mfd->tab[0][2][b]) + 1) >> SHIFT) + mfd->y_offset;
			luma = min(max(luma,0),255);


			//Cb = (int)(0.492 * (b-luma) * 2);
			//Cr = (int)(0.877 * (r-luma) * 2);
			//dst2 = dst +  Cb + (314) + (w+m) * (Cr + 314);


			
			dst2 = dst +  _Cb[b-luma] + (314) + (w+m) * (_Cr[r-luma] + 314);
			*dst2++ = old_pixel;
			*dst2 = old_pixel;

			dst2 = (Pixel32*)((char *)dst2 - dpitch);
			*dst2-- = old_pixel;
			*dst2 = old_pixel;

		} while(--ws);

		src = (Pixel32 *)((char *)src + fa->src.modulo);


    } while(--hs);


	//xRectFill(fa,134,186, 4,4,0x00); //*** Black out that damb spot!!



	return 0;
}

//============ Main WFM ============
int wfm(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	int i, line, reso, holdLuma, msgLen, y16, y235;

	char  dbgMsg[10];


	Pixel32 *src, *dst, *dst0, *src0;
	PixDim w = fa->src.w;
	PixDim h;
	PixOffset dpitch = fa->dst.pitch;
	PixOffset spitch = fa->src.pitch;

	PixOffset dmodulo = fa->dst.modulo;
	PixOffset smodulo = fa->src.modulo;

	const Pixel32 lnColor = 0xFFFFB700;
	const Pixel32 lnColor2 = 0xFF009F19;
	

    src = (Pixel32 *)fa->src.data;
    dst = (Pixel32 *)fa->dst.data;

	int	Red[256], Green[256], Blue[256], Luma[256];
	if(mfd->bRED)	for (i = 0; i < 256; i++) Red[i] = 0;
	if(mfd->bGREEN)	for (i = 0; i < 256; i++) Green[i] = 0;
	if(mfd->bBLUE)	for (i = 0; i < 256; i++) Blue[i] = 0;
	if(mfd->bLUMA)	for (i = 0; i < 256; i++) Luma[i] = 0;


	line = 10;
	reso = 256 / mfd->chartHeight;

	//*** Clear the screen 
	xRectFill(fa,0,0, fa->dst.w,fa->dst.h,0x00);


	for(int k=0; k<mfd->numWFMs; k++) {
		//xRectFill(fa,0,line + k*(mfd->chartHeight + 10) ,fa->dst.w-40,mfd->chartHeight,0); //Clear chart
	

		xRectFill(fa,fa->dst.w-40,line + k*(mfd->chartHeight + 10), 2,mfd->chartHeight,lnColor); // right  box
		//fa->dst.RectFill(0,line - 1, fa->dst.w-38,2,lnColor); // top  box

		if(mfd->bWFMGRID) {
			for( int ii=1;ii<8;ii++) {
				xRectFill(fa,0,1 + line + (ii * 32/reso)+ k*(mfd->chartHeight + 10), fa->dst.w-40,1,lnColor); // lines
			}
		}

		//**** 16 and 235 dotted lines
		y235 = -12/reso + line + (1 * 32/reso)+ k*(mfd->chartHeight + 10);
		y16 = 16/reso - 1 + line + (7 * 32/reso)+ k*(mfd->chartHeight + 10);
		for(int ii=0;ii<fa->dst.w-40;ii=ii+10) {
			xRectFill(fa,ii,y16, 5,1,lnColor); // lines
			xRectFill(fa,ii,y235, 5,1,lnColor); // lines
		}


		for(int jj=0;jj<8;jj++) {
			msgLen  = sprintf( dbgMsg,"- %d", (jj * 32 - 255) * -1 );
			VGL_DrawString(fa, fa->dst.w-39, 1 + line + (jj * 32/reso) + k*(mfd->chartHeight + 10) - 2, dbgMsg, lnColor);
		}

		xRectFill(fa,0,mfd->chartHeight + line + k*(mfd->chartHeight + 10), fa->dst.w-38,2,lnColor); // bottom  box
	}


	src0 = fa->src.data + fa->src.w;
	dst0 = fa->dst.data + fa->dst.w  - 40;
	
	

    do {
		src = --src0;
		h = fa->src.h;
		do {
			Pixel32 old_pixel = *src;

			holdLuma = ((mfd->tab[0][0][(old_pixel>>16) & 0xff] + mfd->tab[0][1][(old_pixel>>8) & 0xff] + mfd->tab[0][2][(old_pixel) & 0xff] + 1) >> SHIFT) + mfd->y_offset;
			Luma[holdLuma] = Luma[holdLuma]+5;

			Red[(old_pixel>>16) & 0xff] = Red[(old_pixel>>16) & 0xff]+5;
			Green[(old_pixel>>8) & 0xff] = Green[(old_pixel>>8) & 0xff]+5;
			Blue[(old_pixel) & 0xff] = Blue[(old_pixel) & 0xff]+5;;
			src = (Pixel32*)((char *)src + spitch);
		} while(--h);


		dst = --dst0;

		dst = (Pixel32*)((char *)dst + (fa->dst.h * dpitch)); //** Move to top line

		if (mfd->bLUMA) {
			dst = (Pixel32*)((char *)dst - ((mfd->chartHeight + 10) * dpitch)); //** Move down
			
			for(i=0;i<256;i=i+reso) {
				holdLuma = (*dst & 0xFF) | mfd->clamp[Luma[i]];
				*dst = *dst | ((holdLuma << 16)  + (holdLuma << 8) + (holdLuma));
				Luma[i] = 0;
				dst = (Pixel32*)((char *)dst + dpitch);
			}
			dst = (Pixel32*)((char *)dst - ((mfd->chartHeight) * dpitch)); //** Move down
		}


		if (mfd->bRED) {
			dst = (Pixel32*)((char *)dst - ((mfd->chartHeight + 10) * dpitch)); //** Move down
			
			for(i=0;i<256;i=i+reso) {
				*dst = *dst | mfd->clamp[Red[i]]<<16;
				Red[i] = 0;
				dst = (Pixel32*)((char *)dst + dpitch);
			}
			dst = (Pixel32*)((char *)dst - ((mfd->chartHeight) * dpitch)); //** Move down
		}

		if (mfd->bGREEN) {
			dst = (Pixel32*)((char *)dst - ((mfd->chartHeight + 10) * dpitch)); //** Move down
			
			for(i=0;i<256;i=i+reso) {
				*dst = *dst | mfd->clamp[Green[i]]<<8;
				Green[i] = 0;
				dst = (Pixel32*)((char *)dst + dpitch);
			}
			dst = (Pixel32*)((char *)dst - ((mfd->chartHeight) * dpitch)); //** Move down
		}

		if (mfd->bBLUE) {
			dst = (Pixel32*)((char *)dst - ((mfd->chartHeight + 10) * dpitch)); //** Move down
			
			for(i=0;i<256;i=i+reso) {
				*dst = *dst | mfd->clamp[Blue[i]];
				Blue[i] = 0;
				dst = (Pixel32*)((char *)dst + dpitch);
			}
			dst = (Pixel32*)((char *)dst - ((mfd->chartHeight) * dpitch)); //** Move down
		}
    
	} while(--w);


	return 0;

}

//============ Main Histograms ============
int histograms(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;


    Pixel32 *src, *dst;
	const Pixel32 lnColor = 0xFFFFB700;
	const Pixel32 lnColor2 = 0xFF009F19;
	int i, ii, msgLen;
	unsigned long max;
	float scaleF;
	char  dbgMsg[50];
	int heightOffset=0;

    src = (Pixel32 *)fa->src.data;
    dst = (Pixel32 *)fa->dst.data;
	max = 0;

//----------

	int x, y, r, g, b, luma;
	int lumaR = 19589;
	int lumaG = 38443;
	int lumaB = 7497;

	//*** Clear Histograms **
	src = fa->src.data;
	if(mfd->bLUMA)	for (ii = 0; ii < 256; ii++) mfd->chart[0].Histogram[ii] = 0;
	if(mfd->bRED)	for (ii = 0; ii < 256; ii++) mfd->chart[1].Histogram[ii] = 0;
	if(mfd->bGREEN)	for (ii = 0; ii < 256; ii++) mfd->chart[2].Histogram[ii] = 0;
	if(mfd->bBLUE)	for (ii = 0; ii < 256; ii++) mfd->chart[3].Histogram[ii] = 0;
	mfd->avd=0;


	//*** Build Histos	-- Just do them all so we loop once
	for (y = 0; y < fa->src.h; y++)
	{
		for (x = 0; x < fa->src.w; x++)
		{
			r = (src[x] >> 16) & 0xff;
			g = (src[x] >> 8) & 0xff;
			b = src[x] & 0xff;

			mfd->chart[1].Histogram[r]++;
			mfd->chart[2].Histogram[g]++;
			mfd->chart[3].Histogram[b]++;

			luma = ((unsigned int) ((mfd->tab[0][0][r] + mfd->tab[0][1][g] + mfd->tab[0][2][b]) + 1) >> SHIFT) + mfd->y_offset;
			luma = min(max(luma,0),255);
			mfd->avd += luma;

			mfd->chart[0].Histogram[luma]++;

		}
		src	= (Pixel32 *)((char *)src + fa->src.pitch);
	}




	// Find max for scale
	for (i = 0; i < 256; i++) {
		if ((mfd->bLUMA) & (max < mfd->chart[0].Histogram[i])) max = mfd->chart[0].Histogram[i];
		if ((mfd->bRED) & (max < mfd->chart[1].Histogram[i])) max = mfd->chart[1].Histogram[i];
		if ((mfd->bGREEN) & (max < mfd->chart[2].Histogram[i])) max = mfd->chart[2].Histogram[i];
		if ((mfd->bBLUE) & (max < mfd->chart[3].Histogram[i])) max = mfd->chart[3].Histogram[i];
	}

		
	// Find scale factor
	scaleF = (float) (max / mfd->chartHeight + 0.0 );

	//** If not accum then clear screen.  Accum has problems with some other filters
	if(!(mfd->bACCUM)) xRectFill(fa,0,0, fa->dst.w,fa->dst.h,0x00); // Clear  Center


	//*** Lets start to draw
	xRectFill(fa,0, 0,360,20,0); //Clear Header
	msgLen  = sprintf( dbgMsg,"AVD: %d   Scale: %.1f "
		, mfd->avd/(fa->src.h * fa->src.w), scaleF );
	dbgMsg[msgLen] = 0;

	heightOffset = 10;
		
   	VGL_DrawString(fa, 50, heightOffset, dbgMsg, 0xFFFFB700);
	heightOffset +=10;


	//--- LUMA ------------
	if (mfd->bLUMA) heightOffset = drawChart(fa, ff,  0, heightOffset, scaleF);
	
	//-------- RED ------   
	if (mfd->bRED) {
		heightOffset +=7;
		heightOffset = drawChart(fa, ff, 1, heightOffset, scaleF);
	}
	
		//-------- Green ------   
	if (mfd->bGREEN) {
		heightOffset +=7;
		heightOffset = drawChart(fa, ff, 2, heightOffset, scaleF);
	}

		//-------- Blue ------   
	if (mfd->bBLUE) {
		heightOffset +=7;
		heightOffset = drawChart(fa, ff, 3, heightOffset, scaleF);
	}
	
    return 0;


}

//==========Main Hot Pixel =============
int hotPixels(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {

    Pixel32 *src, *dst;
    PixDim w, h;

    src = (Pixel32 *)fa->src.data;
    dst = (Pixel32 *)fa->dst.data;

	h = fa->src.h;
	//h = 1;
    do {
        w = fa->src.w;
		//w = 91;
		do {
			Pixel32 old_pixel, new_pixel;
			old_pixel = *src++;
			new_pixel = hot(old_pixel, fa);
			*dst++ = new_pixel;
		} while(--w);

        src = (Pixel32 *)((char *)src + fa->src.modulo);
	    dst = (Pixel32 *)((char *)dst + fa->dst.modulo);

    } while(--h);


	return 0;
}

//====== Main Video View ==================

int showChannels(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

    Pixel32 *src, *dst, mask;
    PixDim w, h;
	int r, g, b,x, luma;

	mask = (mfd->bRED)?0xFF0000:0x000000; //Red
	mask = (mfd->bGREEN)?mask | 0x00FF00:mask;	//Green
	mask = (mfd->bBLUE)?mask | 0x0000FF:mask;	//Blue

	if(mfd->bPASSTHRU) mask = 0xFFFFFF;

    src = (Pixel32 *)fa->src.data;
    dst = (Pixel32 *)fa->dst.data;
	Pixel32 old_pixel, new_pixel;


	h = fa->src.h;
    do {
        w = fa->src.w;
		if(mfd->bLUMA & !mfd->bPASSTHRU) {

			for (x = 0; x < w; x++)
			{
				old_pixel = *src++;
				r = (old_pixel >> 16) & 0xff;
				g = (old_pixel>> 8) & 0xff;
				b = old_pixel & 0xff;
				luma = ((mfd->tab[0][0][r] + mfd->tab[0][1][g] + mfd->tab[0][2][b]) >> SHIFT) + mfd->y_offset;

				*dst++ = (luma << 16)  + (luma << 8) + (luma);

			}

		} else {
			do {
				old_pixel = *src++;

				new_pixel = (old_pixel & mask);

				*dst++ = new_pixel;
			} while(--w);
		}

		src = (Pixel32 *)((char *)src + fa->src.modulo);
	    dst = (Pixel32 *)((char *)dst + fa->dst.modulo);


    } while(--h);


	return 0;
}


//======= Draw Histogram ================
int drawChart(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, int Xcd, int heightOffset, float scaleF) {

		MyFilterData *mfd = (MyFilterData *)fa->filter_data;
		ChartData *cd = (ChartData *)mfd->chart;
		long lineHeight;
		int i, msgLen, min, max;
		char  dbgMsg[20];

		min= max= -1;

		const Pixel32 lnColor = 0xFFFFB700;
		const Pixel32 lnColor2 = 0xFF009F19;
		const int leftOffset = 50;




		xRectFill(fa,leftOffset,heightOffset, 260,2,lnColor); // Top  box
		heightOffset +=2;
		//if(!(mfd->bACCUM)) xRectFill(fa,leftOffset + 2,heightOffset , 256,mfd->chartHeight ,0x00); // Clear  Center

		xRectFill(fa,leftOffset,heightOffset + (long) (mfd->chartHeight * 0.25), 260,1,lnColor); // 25%
		xRectFill(fa,leftOffset,heightOffset + (long) (mfd->chartHeight * 0.5), 260,1,lnColor); // 50%
		xRectFill(fa,leftOffset,heightOffset + (long) (mfd->chartHeight * 0.75), 260,1,lnColor); // 75%


		xRectFill(fa,leftOffset,heightOffset + mfd->chartHeight,260,2,lnColor); // Bottom  box
		xRectFill(fa,leftOffset,heightOffset, 2, mfd->chartHeight,lnColor); // Left  box
		xRectFill(fa,leftOffset + 2 + 256,heightOffset, 2, mfd->chartHeight,lnColor); // Right  box


		xRectFill(fa,leftOffset + 2 + 16,heightOffset, 1, mfd->chartHeight,lnColor); // 16
		xRectFill(fa,leftOffset + 2 + 235,heightOffset, 1, mfd->chartHeight,lnColor); // 235

		for (i = 0; i < 256; i++) {
			lineHeight = (long) (cd[Xcd].Histogram[i] / (long) max(scaleF,1)) ;
			if(lineHeight > mfd->chartHeight) lineHeight = mfd->chartHeight;
			if((min == -1) & (lineHeight > 0) ) min = i;
			if(lineHeight > 0 ) max = i;

			xRectFill(fa,i+leftOffset + 2, mfd->chartHeight + heightOffset - lineHeight,1,lineHeight,cd[Xcd].color);
		}


		if((mfd->bACCUM))  {
			if(min < cd[Xcd].min) cd[Xcd].min = min;
			if(max > cd[Xcd].max) cd[Xcd].max = max;
		} else {
			cd[Xcd].min = min;
			cd[Xcd].max = max;
		}


		heightOffset +=  (mfd->chartHeight + 2);


		//*** Do Min Max Avg
		xRectFill(fa,10, heightOffset - 52,leftOffset - 12,12,0); //Clear Min
		xRectFill(fa,320, heightOffset - 52,40,12,0); //Clear Max


		msgLen  = sprintf( dbgMsg,"%d", cd[Xcd].min );
		dbgMsg[msgLen] = 0;
		VGL_DrawString(fa, 10, heightOffset - 60, "Min", lnColor);
		VGL_DrawString(fa, 10, heightOffset - 50, dbgMsg, lnColor);

		msgLen  = sprintf( dbgMsg,"%d", cd[Xcd].max );
		dbgMsg[msgLen] = 0;
		VGL_DrawString(fa, 320, heightOffset - 60, "Max", lnColor);
		VGL_DrawString(fa, 320, heightOffset - 50, dbgMsg, lnColor);

    return heightOffset;
}





int histoEndProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

    //delete[] mfd->tab;
	//delete[] mfd->chart;
	//mfd->tab = NULL;

    return 0;
}

long histoParamProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	unsigned int	f;
	int	pv, h, theMode;
	double y_matrix[3] = { 0.299, 0.587, 0.114,};	//** studio RGB
	double	maxPIX; 
	mfd->y_offset = 0;

	

	if(mfd->bRNG255) {	//** PC RGB
		mfd->y_offset = 16;
		maxPIX = 255.0;

	  if(mfd->bSTDDEF) {	//** 601
		y_matrix[0] = 0.2567890625;
		y_matrix[1] = 0.50412890625;
		y_matrix[2] = 0.0979625;

	  } else { //*** 709
		y_matrix[0] = 0.1825;
		y_matrix[1] = 0.6144;
		y_matrix[2] = 0.06192;
      }

	} else { //****** studio RGB
	  maxPIX = 255.0;// = 235.0; Not correct for Hot Pixels
      mfd->y_offset = 0;

	  if(mfd->bSTDDEF) {	//** 601
		y_matrix[0] = 0.299;
		y_matrix[1] = 0.587;
		y_matrix[2] = 0.114;

	  } else { //*** 709
		y_matrix[0] = 0.1825;
		y_matrix[1] = 0.6144;
		y_matrix[2] = 0.06192;
      }
	}

	double gamma = 2.2;

	mfd->clamp[0] = 0;
	for(int j = 1; j<5120; j++) mfd->clamp[j]=(j+128<256)?j+128:255;


	
	/*{	//** PAL RGB to YUV
			 0.2989,	 0.5866,	 0.1144,
			-0.1473,	-0.2891,	 0.4364,
			0.6149,	-0.5145,	-0.1004,
		};*/

		mfd->code_matrix[0][0] = 0.2989;
		mfd->code_matrix[0][1] = 0.5866;
		mfd->code_matrix[0][2] = 0.1144;
		mfd->code_matrix[1][0] = -0.1473;
		mfd->code_matrix[1][1] = -0.2891;
		mfd->code_matrix[1][2] = 0.4364;
		mfd->code_matrix[2][0] = 0.6149;
		mfd->code_matrix[2][1] = -0.5145;
		mfd->code_matrix[2][2] = -0.1004;




	if(mfd->bNTSC) {				//** NTSC RGB to YIQ
		mfd->code_matrix[1][0] = 0.5959;
		mfd->code_matrix[1][1] = -0.2741;
		mfd->code_matrix[1][2] = -0.3218;
		mfd->code_matrix[2][0] = 0.2113;
		mfd->code_matrix[2][1] = -0.5227;
		mfd->code_matrix[2][2] = 0.3113;
	}

	theMode = (mfd->bPASSTHRU)?2:mfd->mode;  //******* do video on passthru
	switch (theMode) {
	  case 0:				//*** Histograms
		mfd->chart[0].color = 0xFFCCCCCC;
		mfd->chart[1].color = 0xFFCC0000;
		mfd->chart[2].color = 0xFF00CC00;
		mfd->chart[3].color = 0xFF0000CC;

		mfd->chart[0].min = 255;
		mfd->chart[1].min = 255;
		mfd->chart[2].min = 255;
		mfd->chart[3].min = 255;

		mfd->chart[0].max = -1;
		mfd->chart[1].max = -1;
		mfd->chart[2].max = -1;
		mfd->chart[3].max = -1;


		mfd->chartHeight = 504;
		if(mfd->bRED | mfd->bGREEN | mfd->bBLUE | mfd->bLUMA) 
			mfd->chartHeight = mfd->chartHeight / (!!mfd->bRED + !!mfd->bGREEN + !!mfd->bBLUE + !!mfd->bLUMA);


		for (pv = 0; pv <= 255; pv++) {
			f = SCALE * (pv );
			mfd->tab[0][0][pv] = (unsigned int)(f * (y_matrix[0]) + 0.5);
			mfd->tab[0][1][pv] = (unsigned int)(f * (y_matrix[1]) + 0.5);
			mfd->tab[0][2][pv] = (unsigned int)(f * (y_matrix[2]) + 0.5);
		}

		fa->dst.w = 360;
		fa->dst.h = 570;
		fa->dst.pitch = (fa->dst.w*4 + 7) & -8;
		
		break;
	
	  case 1:
	//********** Hot Pix
	

		if(!(mfd->bNTSC)) gamma = 2.8;


		for (pv = 0; pv <= 255; pv++) {
			if(mfd->bGAMMA) {
				f = (unsigned int) (SCALEhp * (pv /maxPIX));
			} else { 
				f = (unsigned int)(SCALEhp * gc((pv /maxPIX), gamma, true));
			}

			mfd->tab[0][0][pv] = (unsigned int)(f * mfd->code_matrix[0][0] + 0.5);
			mfd->tab[0][1][pv] = (unsigned int)(f * mfd->code_matrix[0][1] + 0.5);
			mfd->tab[0][2][pv] = (unsigned int)(f * mfd->code_matrix[0][2] + 0.5);
			mfd->tab[1][0][pv] = (unsigned int)(f * mfd->code_matrix[1][0] + 0.5);
			mfd->tab[1][1][pv] = (unsigned int)(f * mfd->code_matrix[1][1] + 0.5);
			mfd->tab[1][2][pv] = (unsigned int)(f * mfd->code_matrix[1][2] + 0.5);
			mfd->tab[2][0][pv] = (unsigned int)(f * mfd->code_matrix[2][0] + 0.5);
			mfd->tab[2][1][pv] = (unsigned int)(f * mfd->code_matrix[2][1] + 0.5);
			mfd->tab[2][2][pv] = (unsigned int)(f * mfd->code_matrix[2][2] + 0.5);
		}

		mfd->chroma_lim = (double)CHROMA_LIM / (100.0 );
		mfd->compos_lim = ((double)mfd->maxIRE) / (100.0);

		mfd->ichroma_lim2 = (unsigned int)(mfd->chroma_lim * SCALEhp + 0.5);
		mfd->ichroma_lim2 *= mfd->ichroma_lim2;
		mfd->icompos_lim = (unsigned int)(mfd->compos_lim * SCALEhp + 0.5);

		break;

	  case 2: //** Video

		for (pv = 0; pv <= 255; pv++) {
			f = SCALE * (pv );
			mfd->tab[0][0][pv] = (unsigned int)(f * (y_matrix[0]) + 0.5);
			mfd->tab[0][1][pv] = (unsigned int)(f * (y_matrix[1]) + 0.5);
			mfd->tab[0][2][pv] = (unsigned int)(f * (y_matrix[2]) + 0.5);
		}
		break;

	  case 3: //** WFM

		for (pv = 0; pv <= 255; pv++) {
			f = SCALE * (pv );
			mfd->tab[0][0][pv] = (unsigned int)(f * (y_matrix[0]) + 0.5);
			mfd->tab[0][1][pv] = (unsigned int)(f * (y_matrix[1]) + 0.5);
			mfd->tab[0][2][pv] = (unsigned int)(f * (y_matrix[2]) + 0.5);
		}

		fa->dst.w = fa->dst.w + 40;
		fa->dst.pitch = (fa->dst.w*4);


		mfd->numWFMs = (!!mfd->bRED + !!mfd->bGREEN + !!mfd->bBLUE + !!mfd->bLUMA);
		h = fa->dst.h;

		switch(mfd->numWFMs) {

		case 0:
		case 1:
			fa->dst.h = 276;
			mfd->chartHeight = 256;
			break;
		case 2:
			fa->dst.h = 542;
			mfd->chartHeight = 256;
			break;
		case 3:
			fa->dst.h = 442;
			mfd->chartHeight = 128;
			break;
		case 4:
			fa->dst.h = 562;
			mfd->chartHeight = 128;
			break;
		}

		//fa->dst.h = max(h,fa->dst.h);
		break;


	  case 4:		//*** Vectorscope

		fa->dst.w = 632;
		fa->dst.h = 632;
		fa->dst.pitch = (fa->dst.w*4 + 7) & -8;

		y_matrix[0] = 0.299;
		y_matrix[1] = 0.587;
		y_matrix[2] = 0.114;
		mfd->y_offset = 0;

		for (pv = 0; pv <= 255; pv++) {
			f = SCALE * (pv );
			mfd->tab[0][0][pv] = (unsigned int)(f * (y_matrix[0]) + 0.5);
			mfd->tab[0][1][pv] = (unsigned int)(f * (y_matrix[1]) + 0.5);
			mfd->tab[0][2][pv] = (unsigned int)(f * (y_matrix[2]) + 0.5);
		}



		const int radius = 314;
		double pi = 3.1415926535;

    {for(int i=0; i<360; i++) {
		
			mfd->circle[0][i] = (int) (sin(i*pi/180.0)*radius + 0.5) + radius;
			mfd->circle[1][i] = (int) (cos(i*pi/180.0)*radius + 0.5) + radius;
    }}


    {for(int i=0; i<511; i++) {
		
			mfd->Cb[i] = (int)(0.492 * (i-255) * 2);
			mfd->Cr[i] = (int)(0.877 * (i-255) * 2);
    }}

	
		break;

	}




    return FILTERPARAM_SWAP_BUFFERS;
}


double gc(double val, double gamma, bool mode) {
	if(mode) return pow(val, 1.0 / gamma);
	return pow(val, gamma);
}

void redo_frame(MyFilterData *mfd, HWND hdlg)
{
  if(!mfd->ifp) return;

  mfd->bPASSTHRU = !!IsDlgButtonChecked(hdlg, IDC_PASSTHRU);
  mfd->bACCUM = !!IsDlgButtonChecked(hdlg, IDC_ACCUM);
  mfd->bGAMMA = !!IsDlgButtonChecked(hdlg, IDC_GAMMA);
  mfd->bWFMGRID = !!IsDlgButtonChecked(hdlg, IDC_WFMGRID);
  
  mfd->bRED = !!IsDlgButtonChecked(hdlg, IDC_RED);
  mfd->bGREEN = !!IsDlgButtonChecked(hdlg, IDC_GREEN);
  mfd->bBLUE = !!IsDlgButtonChecked(hdlg, IDC_BLUE);
  mfd->bLUMA = !!IsDlgButtonChecked(hdlg, IDC_LUMA);

  mfd->ifp->RedoSystem();
}

BOOL CALLBACK histoConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    MyFilterData *mfd = (MyFilterData *)GetWindowLongPtr(hdlg, DWLP_USER);

	HWND hwndItem;
	int i;

    switch(msg) {
       case WM_INITDIALOG:
            SetWindowLongPtr(hdlg, DWLP_USER, lParam);
            mfd = (MyFilterData *)lParam;

            {
              HWND hwndPreview = GetDlgItem(hdlg, IDC_PREVIEW);
              if(hwndPreview && mfd->ifp){
                EnableWindow(hwndPreview, TRUE);
                mfd->ifp->InitButton((VDXHWND)hwndPreview);
              }
            }

            CheckDlgButton(hdlg, IDC_PASSTHRU, mfd->bPASSTHRU?BST_CHECKED:BST_UNCHECKED);

			CheckDlgButton(hdlg, IDC_MODE0, mfd->mode == 0 ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_MODE1, mfd->mode == 1 ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_MODE2, mfd->mode == 2 ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_MODE3, mfd->mode == 3 ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_MODE4, mfd->mode == 4 ? BST_CHECKED : BST_UNCHECKED);

            CheckDlgButton(hdlg, IDC_ACCUM, mfd->bACCUM?BST_CHECKED:BST_UNCHECKED);
            CheckDlgButton(hdlg, IDC_WFMGRID, mfd->bWFMGRID?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_NTSC,  mfd->bNTSC? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_PAL,   !(mfd->bNTSC)? BST_CHECKED : BST_UNCHECKED);

			CheckDlgButton(hdlg, IDC_STD,   mfd->bSTDDEF? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_HIGH,  !(mfd->bSTDDEF)? BST_CHECKED : BST_UNCHECKED);

			CheckDlgButton(hdlg, IDC_RNG255, mfd->bRNG255? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_RNG235, !(mfd->bRNG255)? BST_CHECKED : BST_UNCHECKED);

			CheckDlgButton(hdlg, IDC_RED, mfd->bRED?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_GREEN, mfd->bGREEN?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_BLUE, mfd->bBLUE?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_LUMA, mfd->bLUMA?BST_CHECKED:BST_UNCHECKED);

			hwndItem = GetDlgItem(hdlg, AFX_IDC_HOTCHANGE);
			for(i=0; i<(sizeof hotFixNames/sizeof hotFixNames[0]); i++)
				SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)hotFixNames[i]);
			SendMessage(hwndItem, CB_SETCURSEL, mfd->hotFix, 0);

			hwndItem = GetDlgItem(hdlg, AFX_IDC_MAXIRE);
			for(i=0; i<(sizeof IRENames/sizeof IRENames[0]); i++)
				SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)IRENames[i]);
			SendMessage(hwndItem, CB_SETCURSEL, (mfd->maxIRE-100)/10, 0);

			
			CheckDlgButton(hdlg, IDC_GAMMA, mfd->bGAMMA?BST_CHECKED:BST_UNCHECKED);



			return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam)) {
            case IDOK:
                mfd->bPASSTHRU = !!IsDlgButtonChecked(hdlg, IDC_PASSTHRU);
                mfd->bACCUM = !!IsDlgButtonChecked(hdlg, IDC_ACCUM);
                mfd->bGAMMA = !!IsDlgButtonChecked(hdlg, IDC_GAMMA);
                mfd->bWFMGRID = !!IsDlgButtonChecked(hdlg, IDC_WFMGRID);

                mfd->bRED = !!IsDlgButtonChecked(hdlg, IDC_RED);
                mfd->bGREEN = !!IsDlgButtonChecked(hdlg, IDC_GREEN);
                mfd->bBLUE = !!IsDlgButtonChecked(hdlg, IDC_BLUE);
                mfd->bLUMA = !!IsDlgButtonChecked(hdlg, IDC_LUMA);


                EndDialog(hdlg, 0);
                return TRUE;
            case IDCANCEL:
                EndDialog(hdlg, 1);
                return FALSE;

            case IDC_SAVE:
              save_settings(mfd);
              return TRUE;

            case IDC_PASSTHRU:
            case IDC_ACCUM:
            case IDC_GAMMA:
            case IDC_WFMGRID:
            case IDC_RED:
            case IDC_GREEN:
            case IDC_BLUE:
            case IDC_LUMA:
              redo_frame(mfd,hdlg);
              return TRUE;

			case AFX_IDC_HOTCHANGE:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					mfd->hotFix = (int)SendDlgItemMessage(hdlg, AFX_IDC_HOTCHANGE, CB_GETCURSEL, 0, 0);
				}
        redo_frame(mfd,hdlg);
				return TRUE;


 			case AFX_IDC_MAXIRE:

				if (HIWORD(wParam) == CBN_SELCHANGE) {
					i = (int)SendDlgItemMessage(hdlg, AFX_IDC_MAXIRE, CB_GETCURSEL, 0, 0);
				}

				switch(i)
				{
					case 0: // 100
						mfd->maxIRE = 100;
						break;
					case 1: // 110
						mfd->maxIRE = 110;
						break;
					case 2: // 120
						mfd->maxIRE = 120;
						break;
					}
        redo_frame(mfd,hdlg);
				break;

 			case IDC_MODE0:
				mfd->mode = 0;
				CheckDlgButton(hdlg, IDC_MODE0, BST_CHECKED);
        redo_frame(mfd,hdlg);
				break;
 			case IDC_MODE1:
				mfd->mode = 1;
				CheckDlgButton(hdlg, IDC_MODE1, BST_CHECKED);
        redo_frame(mfd,hdlg);
				break;
 			case IDC_MODE2:
				mfd->mode = 2;
				CheckDlgButton(hdlg, IDC_MODE2, BST_CHECKED);
        redo_frame(mfd,hdlg);
				break;
 			case IDC_MODE3:
				mfd->mode = 3;
				CheckDlgButton(hdlg, IDC_MODE3, BST_CHECKED);
        redo_frame(mfd,hdlg);
				break;
 			case IDC_MODE4:
				mfd->mode = 4;
				CheckDlgButton(hdlg, IDC_MODE4, BST_CHECKED);
        redo_frame(mfd,hdlg);
				break;

 			case IDC_NTSC:
				mfd->bNTSC = true;
				CheckDlgButton(hdlg, IDC_NTSC, BST_CHECKED);
				CheckDlgButton(hdlg, IDC_PAL, BST_UNCHECKED);
        redo_frame(mfd,hdlg);
				break;
 			case IDC_PAL:
				mfd->bNTSC = false;
				CheckDlgButton(hdlg, IDC_NTSC, BST_UNCHECKED);
				CheckDlgButton(hdlg, IDC_PAL, BST_CHECKED);
        redo_frame(mfd,hdlg);
				break;

 			case IDC_RNG255:
				mfd->bRNG255 = true;
				CheckDlgButton(hdlg, IDC_RNG255, BST_CHECKED);
				CheckDlgButton(hdlg, IDC_RNG235, BST_UNCHECKED);
        redo_frame(mfd,hdlg);
				break;
 			case IDC_RNG235:
				mfd->bRNG255 = false;
				CheckDlgButton(hdlg, IDC_RNG255, BST_UNCHECKED);
				CheckDlgButton(hdlg, IDC_RNG235, BST_CHECKED);
        redo_frame(mfd,hdlg);
				break;

 			case IDC_STD:
				mfd->bSTDDEF = true;
				CheckDlgButton(hdlg, IDC_STD, BST_CHECKED);
				CheckDlgButton(hdlg, IDC_HIGH, BST_UNCHECKED);
        redo_frame(mfd,hdlg);
				break;
 			case IDC_HIGH:
				mfd->bSTDDEF = false;
				CheckDlgButton(hdlg, IDC_STD, BST_UNCHECKED);
				CheckDlgButton(hdlg, IDC_HIGH, BST_CHECKED);
        redo_frame(mfd,hdlg);
				break;
      case IDC_PREVIEW:
        if(mfd->ifp)
          mfd->ifp->Toggle((VDXHWND)hdlg);
        return true;

           }
            break;


    }

    return FALSE;
}

int histoConfigProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hwnd) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    MyFilterData old = *mfd;
    mfd->ifp = fa->ifp;
	  int rtn = DialogBoxParam(hInstance,
            MAKEINTRESOURCE(IDD_FILTER_TUTORIAL), (HWND)hwnd,
            (DLGPROC) histoConfigDlgProc, (LPARAM)fa->filter_data)!=0;
    if(rtn) *mfd = old;
    mfd->ifp = 0;

	return rtn;
}

void histoStringProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *str) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	switch(mfd->mode)	{
		case 0:
			sprintf(str, " (Histograms < %s%s%s%s>)", 
				(mfd->bLUMA)? "Luma " : "",
				(mfd->bRED)? "Red " : "",
				(mfd->bGREEN)? "Green " : "",
				(mfd->bBLUE)? "Blue" : "");
			break;
		case 1:
			sprintf(str, " (Hot Pixel < IRE %d Change %s>)",
				mfd->maxIRE,
				(mfd->hotFix==0)?"to black":((mfd->hotFix==1)?"intensity":"saturation"));
			break;
		case 2:
			if(mfd->bLUMA){
				sprintf(str, " (Video <Luma>)");
			} else {

			  sprintf(str, " (Video <%s%s%s>)",
				(mfd->bRED)? "Red " : "",
				(mfd->bGREEN)? "Green " : "",
				(mfd->bBLUE)? "Blue" : "");
			}

			break;
		case 3:
			sprintf(str, " (Wave Form Monitor <%s%s%s%s>)",
				(mfd->bLUMA)? "Luma " : "",
				(mfd->bRED)? "Red " : "",
				(mfd->bGREEN)? "Green " : "",
				(mfd->bBLUE)? "Blue" : "");
			break;
	}

	if(mfd->bPASSTHRU) sprintf(str, " (Passthru)");


}

void histoScriptConfig(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
    VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

		mfd->mode    = argv[0].asInt();
		mfd->bPASSTHRU    = !!argv[1].asInt();
		mfd->bACCUM    = !!argv[2].asInt();
		mfd->bNTSC    = !!argv[3].asInt();
		mfd->bGAMMA    = !!argv[4].asInt();
		mfd->bSTDDEF    = !!argv[5].asInt();
		mfd->bRNG255    = !!argv[6].asInt();
		mfd->bLUMA    = !!argv[7].asInt();
		mfd->bRED    = !!argv[8].asInt();
		mfd->bGREEN    = !!argv[9].asInt();
		mfd->bBLUE    = !!argv[10].asInt();
		mfd->hotFix	= argv[11].asInt();
		mfd->bWFMGRID    = !!argv[12].asInt();
	
}

bool histoFssProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

    _snprintf(buf, buflen, "Config(%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
        mfd->mode
		, mfd->bPASSTHRU
		, mfd->bACCUM
		, mfd->bNTSC
		, mfd->bGAMMA
		, mfd->bSTDDEF
		, mfd->bRNG255
		, mfd->bLUMA
		, mfd->bRED
		, mfd->bGREEN
		, mfd->bBLUE
		, mfd->hotFix
		, mfd->bWFMGRID
		);

    return true;
}

int MyInitProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	//fa->dst.RectFill(0, 0,fa->dst.w,fa->dst.h,0);
  load_settings(mfd);

	return 0;
}

Pixel32 hot(Pixel32 p, const VDXFilterActivation *fa) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	int	r, g, b;
	int	y, i, q;
	long	y2, c2;
	double		pr, pg, pb;
	double	fy, fc, t, scale;
	//char dbgMsg[50];
	double		py;

	/*double code_matrix[3][3] = {
		 0.2989,	 0.5866,	 0.1144,
		-0.1473,	-0.2891,	 0.4364,
		0.6149,	-0.5145,	-0.1004,
	};*/


	int	prev_r = 0, prev_g = 0, prev_b = 0;
	long new_r=0, new_g=0, new_b=0;


	r = (p & 0x00FF0000)>>16;
	g = (p & 0x0000FF00)>>8;
	b = p & 0x000000FF;

	/*
	 * Pixel decoding, gamma correction, and matrix multiplication
	 * all done by lookup table.
	 *
	 * "i" and "q" are the two chrominance components;
	 * they are I and Q for NTSC.
	 * For PAL, "i" is U (scaled B-Y) and "q" is V (scaled R-Y).
	 * Since we only care about the length of the chroma vector,
	 * not its angle, we don't care which is which.
	 */



	y = mfd->tab[0][0][r] + mfd->tab[0][1][g] + mfd->tab[0][2][b];
	i = mfd->tab[1][0][r] + mfd->tab[1][1][g] + mfd->tab[1][2][b];
	q = mfd->tab[2][0][r] + mfd->tab[2][1][g] + mfd->tab[2][2][b];

	/*
	 * Check to see if the chrominance vector is too long or the
	 * composite waveform amplitude is too large.
	 *
	 * Chrominance is too large if
	 *
	 *	sqrt(i^2, q^2)  >  chroma_lim.
	 *
	 * The composite signal amplitude is too large if
	 *
	 *	y + sqrt(i^2, q^2)  >  compos_lim.
	 *
	 * We avoid doing the sqrt by checking
	 *
	 *	i^2 + q^2  >  chroma_lim^2
	 * and
	 *	y + sqrt(i^2 + q^2)  >  compos_lim
	 *	sqrt(i^2 + q^2)  >  compos_lim - y
	 *	i^2 + q^2  >  (compos_lim - y)^2
	 *
	 */

	c2 = (long)i * i + (long)q * q;
	y2 = (long)mfd->icompos_lim - y;
	y2 *= y2;


	if (c2 <= mfd->ichroma_lim2 && c2 <= y2)	/* no problems */
		return p;

	//*
	// * Pixel is hot, choose desired (compilation time controlled) strategy
	//
	if(mfd->hotFix==0) {
		 //* Set the hot pixel to black to identify it.
		return 0x000000;
	} else {

		/*
		 * Optimization: cache the last-computed hot pixel.
		 */
		if (r == prev_r && g == prev_g && b == prev_b) {
			p = (new_r<<16) && (new_g <<8) && new_b;
			return p;
		}
		prev_r = r;
		prev_g = g;
		prev_b = b;


		fy = (double)y / SCALEhp;
		fc = _hypot((double)i / SCALEhp, (double)q / SCALEhp);

		pr = r/255.0;
		pg = g/255.0;
		pb = b/255.0;

		if(mfd->hotFix==1) { // Intensity
				scale = mfd->chroma_lim / fc;
				t = mfd->compos_lim / (fy + fc);
				if (t < scale)
					scale = t;
				//scale = pow(scale, GAMMA);

				r = (int) ((scale * pr * 255)+ 0.5);
				g = (int) ((scale * pg * 255) + 0.5);
				b = (int) ((scale * pb * 255) + 0.5);

		} else { //** Saturation
				scale = mfd->chroma_lim / fc;
				t = (mfd->compos_lim - fy) / fc;
				if (t < scale)
					scale = t;

				//pr = gc(pr);
				//pg = gc(pg);
				//pb = gc(pb);
				py = pr * mfd->code_matrix[0][0] + pg * mfd->code_matrix[0][1]
					+ pb * mfd->code_matrix[0][2];
				r = (int) (((py + scale * (pr - py))* 255.0) + 0.5);
				g = (int) (((py + scale * (pg - py))* 255.0) + 0.5);
				b = (int) (((py + scale * (pb - py))* 255.0) + 0.5);
		}

		new_r = r;
		new_g = g;
		new_b = b;

		p = (new_r<<16) | (new_g <<8) | new_b;
		return p;

	}
}



//*********************************

void xRectFill(const VDXFilterActivation *fa, PixCoord x, PixCoord y, PixDim dx, PixDim dy, Pixel32 c) {


	// Do the blit

	Pixel32 *dst = (Pixel32 *)fa->dst.data;
	int w = fa->dst.w;
	int h = fa->dst.h;
	ptrdiff_t m = fa->dst.modulo;
	PixOffset pitch = fa->dst.pitch;



	if (dx == -1) dx = w;
	if (dy == -1) dy = h;

	// clip to destination bitmap

	if (x < 0) { dx+=x; x=0; }
	if (y < 0) { dy+=y; y=0; }
	if (x+dx > w) dx=w-x;
	if (y+dy > h) dy=h-y;

	// anything left to fill?

	if (dx<=0 || dy<=0) return;

	// compute coordinates

	dst = fa->dst.Address32(x, y+dy-1);

	// do the fill

	do {
		PixDim dxt = dx;
		Pixel32 *dst2 = dst;

		do {
			*dst2++ = c;
		} while(--dxt);

		dst = (Pixel32 *)((char *)dst + pitch);
	} while(--dy);

return;

}
