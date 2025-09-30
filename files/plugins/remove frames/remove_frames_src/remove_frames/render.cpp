#include "stdafx.h"
#include "remove_frames.h"
#include <emmintrin.h>
#include <vector>
#include <string>

void copy_plane(uint8* dst, int dst_pitch, const uint8* src, int src_pitch, int w_size, int h)
{
  {for(int y=0; y<h; y++){
    memcpy(dst,src,w_size);
    (uint8*&)src += src_pitch;
    (uint8*&)dst += dst_pitch;
  }}
}

void fill_garbage_r(uint8* dst, int dst_pitch, int w_size, int h)
{
  {for(int y=0; y<h; y++){
    memset(dst,0,w_size);
    (uint8*&)dst += dst_pitch;
  }}
}

void fill_garbage_uv(uint8* dst, int dst_pitch, int w_size, int h)
{
  {for(int y=0; y<h; y++){
    memset(dst,0x80,w_size);
    (uint8*&)dst += dst_pitch;
  }}
}

void line_stat_plane(const uint8* s0, const uint8* s1, int w, int threshold, int& pxdiff, int& maxdiff)
{
  int w16 = w/16;
  __m128i th = _mm_set1_epi8((char)threshold);
  __m128i sat = _mm_set1_epi8((signed char)254);
  __m128i max = _mm_setzero_si128();
  __m128i cnt = _mm_setzero_si128();
  {for(int x=0; x<w16; x++){
    __m128i a = _mm_loadu_si128((__m128i*)s0);
    __m128i b = _mm_loadu_si128((__m128i*)s1);
    __m128i d0 = _mm_subs_epu8(a,b);
    __m128i d1 = _mm_subs_epu8(b,a);
    d0 = _mm_add_epi8(d0,d1);
    max = _mm_max_epu8(max,d0);
    d0 = _mm_subs_epu8(d0,th);
    d0 = _mm_adds_epu8(d0,sat);
    d0 = _mm_subs_epu8(d0,sat);
    cnt = _mm_adds_epu8(cnt,d0);

    s0 += 16;
    s1 += 16;
  }}
  w -= w16*16;
  {for(int i=0; i<16; i++){
    pxdiff += cnt.m128i_u8[i];
    int v = max.m128i_u8[i];
    if(v>maxdiff) maxdiff = v;
  }}

  {for(int x=0; x<w; x++){
    int v = s1[x]-s0[x];
    if(v<0) v = -v;
    if(v>threshold) pxdiff++;
    if(v>maxdiff) maxdiff=v;
  }}
}

void line_stat_rgb(const uint8* s0, const uint8* s1, int w, int threshold, int& pxdiff, int& maxdiff)
{
  // not really counting pixels
  int px = 0;
  line_stat_plane(s0,s1,w*3,threshold,px,maxdiff);
  pxdiff += (px+2)/3;

  /*
  {for(int x=0; x<w*3; x++){
    int v = s1[x]-s0[x];
    if(v<0) v = -v;
    if(v>threshold) pxdiff++;
    if(v>maxdiff) maxdiff=v;
  }}
  */
}

void line_stat_xrgb(const uint8* s0, const uint8* s1, int w, int threshold, int& pxdiff, int& maxdiff)
{
  // not really counting pixels
  int w16 = w/16;
  int th4 = 0xFF000000 + threshold + (threshold<<8) + (threshold<<16);
  __m128i th = _mm_set1_epi32(th4);
  __m128i sat = _mm_set1_epi8((signed char)254);
  __m128i max = _mm_setzero_si128();
  __m128i cnt = _mm_setzero_si128();
  {for(int x=0; x<w16*4; x++){
    __m128i a = _mm_loadu_si128((__m128i*)s0);
    __m128i b = _mm_loadu_si128((__m128i*)s1);
    __m128i d0 = _mm_subs_epu8(a,b);
    __m128i d1 = _mm_subs_epu8(b,a);
    d0 = _mm_add_epi8(d0,d1);
    max = _mm_max_epu8(max,d0);
    d0 = _mm_subs_epu8(d0,th);
    d0 = _mm_adds_epu8(d0,sat);
    d0 = _mm_subs_epu8(d0,sat);
    cnt = _mm_adds_epu8(cnt,d0);

    s0 += 16;
    s1 += 16;
  }}
  w -= w16*16;
  {for(int i=0; i<16; i++){
    pxdiff += cnt.m128i_u8[i];
    int v = max.m128i_u8[i];
    if(v>maxdiff) maxdiff = v;
  }}

  {for(int x=0; x<w*4; x++){
    int v = s1[x]-s0[x];
    if(v<0) v = -v;
    if(v>threshold) pxdiff++;
    if(v>maxdiff) maxdiff=v;
    x++;
    v = s1[x]-s0[x];
    if(v<0) v = -v;
    if(v>threshold) pxdiff++;
    if(v>maxdiff) maxdiff=v;
    x++;
    v = s1[x]-s0[x];
    if(v<0) v = -v;
    if(v>threshold) pxdiff++;
    if(v>maxdiff) maxdiff=v;
    x++;
  }}
}

void DiffFilter::render()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  int format = ExtractBaseFormat(fa->src.mpPixmapLayout->format);
  int w = src.w;
  int h = src.h;
  int w2 = src.w;
  int h2 = src.h;
  int bpp = 1;
  switch(format) {
  case nsVDXPixmap::kPixFormat_RGB888:
    bpp = 3;
    break;
  case nsVDXPixmap::kPixFormat_XRGB8888:
    bpp = 4;
    break;
  case nsVDXPixmap::kPixFormat_YUV420_Planar:
    w2 = (w2+1)>>1;
    h2 = (h2+1)>>1;
    break;
  case nsVDXPixmap::kPixFormat_YUV422_Planar:
    w2 = (w2+1)>>1;
    break;
  }

  bool fill=false;
  if(param.mode==1){
    sint64 frame = fa->mpSourceFrames[0]->mFrameNumber;
    if(frame<(sint64)kill.size()) fill=kill[(unsigned int)frame];
  }

  if(fill){
    fill_garbage_r((uint8*)dst.data,dst.pitch,w*bpp,h);
    if(src.pitch2) fill_garbage_uv((uint8*)dst.data2,dst.pitch2,w2*bpp,h2);
    if(src.pitch3) fill_garbage_uv((uint8*)dst.data3,dst.pitch3,w2*bpp,h2);
  } else {
    copy_plane((uint8*)dst.data,dst.pitch,(const uint8*)src.data,src.pitch,w*bpp,h);
    if(src.pitch2) copy_plane((uint8*)dst.data2,dst.pitch2,(const uint8*)src.data2,src.pitch2,w2,h2);
    if(src.pitch3) copy_plane((uint8*)dst.data3,dst.pitch3,(const uint8*)src.data3,src.pitch3,w2,h2);
  }
}

class StatHandler: public IFilterModPreviewSample{
public:
  int threshold;
  bool cancel;
  std::vector<int> stat;

  StatHandler(){ cancel=false; }
  virtual int Run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff);
  virtual void Cancel(){ cancel=true; }
  virtual void GetNextFrame(sint64 frame, sint64* next_frame, sint64* total_count){}
};

int StatHandler::Run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff)
{
  const VDXPixmap& src0 = *fa->mpSourceFrames[0]->mpPixmap;
  const VDXPixmap& src1 = *fa->mpSourceFrames[1]->mpPixmap;

  int pxdiff = 0;
  int maxdiff = 0;

  {for(int y=0; y<src0.h; y++){
    const uint8* s0 = (const uint8*)src0.data + src0.pitch*y;
    const uint8* s1 = (const uint8*)src1.data + src1.pitch*y;

    switch(src0.format){
    case nsVDXPixmap::kPixFormat_RGB888:
      line_stat_rgb(s0,s1,src0.w,threshold,pxdiff,maxdiff);
      break;
    case nsVDXPixmap::kPixFormat_XRGB8888:
      line_stat_xrgb(s0,s1,src0.w,threshold,pxdiff,maxdiff);
      break;
    case nsVDXPixmap::kPixFormat_YUV444_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Planar:
    case nsVDXPixmap::kPixFormat_Y8:
    case nsVDXPixmap::kPixFormat_Y8_FR:
      line_stat_plane(s0,s1,src0.w,threshold,pxdiff,maxdiff);
      break;
    }
  }}

  if(fa->mpSourceFrames[0]->mFrameNumber!=fa->mpSourceFrames[1]->mFrameNumber+1){
    pxdiff = src0.w*src0.h;
    maxdiff = 255;
  }

  stat.push_back(maxdiff);

  return 0;
}

void DiffFilter::collect_stat()
{
  if(fma && fma->fmpreview && fma->fmproject){
    wchar_t buf[1024];
    size_t size = sizeof buf;
    if(!fma->fmproject->GetMainSource(buf,&size)) return;

    StatHandler handler;
    handler.threshold = 0;//param.threshold;
    scan_mode = true;
    fma->fmpreview->SampleFrames(&handler);
    scan_mode = false;
    std::swap(stat,handler.stat);
    kill.resize(0);
    redo_stat();
  }
}

