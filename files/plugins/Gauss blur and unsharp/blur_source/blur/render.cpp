#include "blur.h"
#include "unsharp.h"
#include <emmintrin.h>

void BlurFilter::render_op0()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  {for(int y=0; y<src.h; y++){
    unsigned char* d = (unsigned char*)dst.data + y*dst.pitch;
    {for(int x=0; x<src.w; x++){
      int sum0 = 0;
      int sum1 = 0;
      int sum2 = 0;

      {for(int bx=0; bx<bk.w; bx++){
        int sx = x+bx-bk.w0;
        if(sx<0) sx = 0;
        if(sx>src.w-1) sx=src.w-1;

        {for(int by=0; by<bk.w; by++){
          int sy = y+by-bk.w0;
          if(sy<0) sy=0;
          if(sy>src.h-1) sy=src.h-1;

          const unsigned char* s = (const unsigned char*)src.data + sx*4 + sy*src.pitch;
          int m = bk.data[bx]*bk.data[by];
          sum0 += s[0]*m;
          sum1 += s[1]*m;
          sum2 += s[2]*m;
        }}
      }}

      d[0] = sum0 >> 16;
      d[1] = sum1 >> 16;
      d[2] = sum2 >> 16;
      d+=4;
    }}
  }}
}

void UnsharpFilter::render_op0()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  int power = int(float(this->power)/power_scale*256);
  int th = this->threshold<<16;

  {for(int y=0; y<src.h; y++){
    const unsigned char* s = (const unsigned char*)src.data + y*src.pitch;
    unsigned char* d = (unsigned char*)dst.data + y*dst.pitch;
    {for(int x=0; x<src.w; x++){
      int sum0 = 0;
      int sum1 = 0;
      int sum2 = 0;

      {for(int bx=0; bx<bk.w; bx++){
        int sx = x+bx-bk.w0;
        if(sx<0) sx = 0;
        if(sx>src.w-1) sx=src.w-1;

        {for(int by=0; by<bk.w; by++){
          int sy = y+by-bk.w0;
          if(sy<0) sy=0;
          if(sy>src.h-1) sy=src.h-1;

          const unsigned char* s = (const unsigned char*)src.data + sx*4 + sy*src.pitch;
          int m = bk.data[bx]*bk.data[by];
          sum0 += s[0]*m;
          sum1 += s[1]*m;
          sum2 += s[2]*m;
        }}
      }}

      int c0 = (s[0]<<16) - (sum0>>8)*power;
      int c1 = (s[1]<<16) - (sum1>>8)*power;
      int c2 = (s[2]<<16) - (sum2>>8)*power;

      if(abs(c0)>th || abs(c1)>th || abs(c2)>th){
        c0 = (c0+(s[0]<<16))>>16;
        c1 = (c1+(s[1]<<16))>>16;
        c2 = (c2+(s[2]<<16))>>16;

        if(c0<0) c0=0; else if(c0>255) c0=255;
        if(c1<0) c1=0; else if(c1>255) c1=255;
        if(c2<0) c2=0; else if(c2>255) c2=255;

        d[0] = c0;
        d[1] = c1;
        d[2] = c2;
      } else {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
      }

      d+=4;
      s+=4;
    }}
  }}
}

void BlurFilter::render_skip()
{
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;

  {for(int y=0; y<pxsrc.h; y++){
		const unsigned char* s = (const unsigned char*)pxsrc.data + pxsrc.pitch*y;
		unsigned char* d = (unsigned char*)pxdst.data + pxdst.pitch*y;
    memcpy(d,s,pxsrc.w*4);
  }}
}

void BlurFilter::render_op1()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  int slice_y0 = 0;
  while(1){
    int slice_y1 = slice_y0+slice_h;
    if(slice_y1>src.h) slice_y1 = src.h;

    int clamp_y0 = slice_y0-bk.w0;
    int clamp_y1 = slice_y1+bk.w0;
    int extra0 = 0;
    int extra1 = 0;
    if(clamp_y0<0){ extra0 = -clamp_y0; clamp_y0 = 0; }
    if(clamp_y1>src.h){ extra1 = clamp_y1-src.h; clamp_y1 = src.h; }

    {for(int x=0; x<src.w; x++){
      const unsigned char* s = (const unsigned char*)src.data + x*4 + clamp_y0*src.pitch;
      const unsigned char* d = row;
      
      int s0 = *(int*)s;
      {for(int y=0; y<extra0; y++){
        *(int*)d = s0;
        d+=4;
      }}
      {for(int y=clamp_y0; y<clamp_y1; y++){
        *(int*)d = *(int*)s;
        d+=4;
        s+=src.pitch;
      }}
      int s1 = *(int*)(s-src.pitch);
      {for(int y=0; y<extra1; y++){
        *(int*)d = s1;
        d+=4;
      }}

      unsigned short* d2 = buf + (x+bk.w0)*4;

      {for(int y=0; y<slice_y1-slice_y0; y++){
        int sum0 = 0;
        int sum1 = 0;
        int sum2 = 0;
        unsigned char* s = row + y*4;
        {for(int i=0; i<bk.w; i++){
          int m = bk.data[i];
          sum0 += m*s[0];
          sum1 += m*s[1];
          sum2 += m*s[2];
          s+=4;
        }}

        d2[0] = sum0;
        d2[1] = sum1;
        d2[2] = sum2;

        d2 += buf_pitch;
      }}
    }}
    
    {for(int y=slice_y0; y<slice_y1; y++){
      unsigned char* d = (unsigned char*)dst.data + y*dst.pitch;
      unsigned short* row = buf + (y-slice_y0)*buf_pitch;
      {
        unsigned short* s = row + (bk.w0)*4;
        int c0 = s[0];
        int c1 = s[1];
        int c2 = s[2];
        {for(int i=0; i<bk.w0; i++){
          s-=4;
          s[0] = c0;
          s[1] = c1;
          s[2] = c2;
        }}
      }
      {
        unsigned short* s = row + (src.w-1+bk.w0)*4;
        int c0 = s[0];
        int c1 = s[1];
        int c2 = s[2];
        {for(int i=0; i<bk.w0; i++){
          s+=4;
          s[0] = c0;
          s[1] = c1;
          s[2] = c2;
        }}
      }

      {for(int x=0; x<src.w; x++){
        int sum0 = 0;
        int sum1 = 0;
        int sum2 = 0;
        unsigned short* s = row + x*4;
        {for(int i=0; i<bk.w; i++){
          int m = bk.data[i];
          sum0 += m*s[0];
          sum1 += m*s[1];
          sum2 += m*s[2];
          s+=4;
        }}

        d[0] = sum0 >> 16;
        d[1] = sum1 >> 16;
        d[2] = sum2 >> 16;
        d+=4;
      }}
    }}

    if(slice_y1==src.h) break;
    slice_y0 = slice_y1;
  }
}

void BlurFilter::render_op2()
{
  const VDXPixmap& src = *fa->src.mpPixmap;

  int slice_y0 = 0;
  while(1){
    int slice_y1 = slice_y0+slice_h;
    if(slice_y1>src.h) slice_y1 = src.h;

    if(bk.w>21)
      op2_slice_vpass_wide(slice_y0,slice_y1);
    else
      op2_slice_vpass(slice_y0,slice_y1);

    op2_slice_hpass(slice_y0,slice_y1);

    if(slice_y1==src.h) break;
    slice_y0 = slice_y1;
  }
}

void UnsharpFilter::render_op2()
{
  const VDXPixmap& src = *fa->src.mpPixmap;

  int slice_y0 = 0;
  while(1){
    int slice_y1 = slice_y0+slice_h;
    if(slice_y1>src.h) slice_y1 = src.h;

    if(bk.w>21)
      op2_slice_vpass_wide(slice_y0,slice_y1);
    else
      op2_slice_vpass(slice_y0,slice_y1);

    if(threshold)
      op2_slice_hpass_unsharp2(slice_y0,slice_y1);
    else
      op2_slice_hpass_unsharp(slice_y0,slice_y1);

    if(slice_y1==src.h) break;
    slice_y0 = slice_y1;
  }
}

void BlurFilter::op2_slice_vpass_wide(int slice_y0, int slice_y1)
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  int clamp_y0 = slice_y0-bk.w0;
  int clamp_y1 = slice_y1+bk.w0;
  int extra0 = 0;
  int extra1 = 0;
  if(clamp_y0<0){ extra0 = -clamp_y0; clamp_y0 = 0; }
  if(clamp_y1>src.h){ extra1 = clamp_y1-src.h; clamp_y1 = src.h; }

  int src_w4 = (src.w+3)/4;
  unsigned char* src_data = (unsigned char*)src.data;
  int src_pitch = src.pitch;

  unsigned char* row = (unsigned char*)(ptrdiff_t(this->row + 15) & ~15);
  unsigned short* buf1 = (unsigned short*)(ptrdiff_t(this->buf + bk.w0*4 + 7) & ~15);

  {for(int x=0; x<src_w4; x++){
    const unsigned char* s = src_data + x*16 + clamp_y0*src_pitch;
    const unsigned char* d = row;

    int n0 = extra0;
    if(n0){
      __m128i s0 = _mm_load_si128((__m128i*)s);
      for(; n0; n0--){
        _mm_store_si128((__m128i*)d,s0);
        d+=16;
      }
    }
    {for(int y=clamp_y0; y<clamp_y1; y++){
      __m128i v = _mm_load_si128((__m128i*)s);
      _mm_store_si128((__m128i*)d,v);
      d+=16;
      s+=src_pitch;
    }}
    int n1 = extra1;
    if(n1){
      __m128i s1 = _mm_load_si128((__m128i*)(s-src_pitch));
      for(; n1; n1--){
        _mm_store_si128((__m128i*)d,s1);
        d+=16;
      }
    }

    unsigned short* d2 = buf1 + x*16;
  	const __m128i zero = _mm_setzero_si128();
    unsigned short* bk_data2 = bk.data2;
    int bk_w = bk.w;

    {for(int y=0; y<slice_y1-slice_y0; y++){
    	__m128i sum0 = zero;
    	__m128i sum1 = zero;
      unsigned char* s = row + y*16;
      __m128i* data2 = (__m128i*)(bk_data2);
      {for(int i=0; i<bk_w; i++){
        __m128i m = _mm_load_si128(data2);
        __m128i v = _mm_load_si128((__m128i*)s);
        __m128i v0 = _mm_unpacklo_epi8(v,zero);
        __m128i v1 = _mm_unpackhi_epi8(v,zero);
        v0 = _mm_mullo_epi16(v0,m);
        v1 = _mm_mullo_epi16(v1,m);
        sum0 = _mm_add_epi16(sum0,v0);
        sum1 = _mm_add_epi16(sum1,v1);
        s+=16;
        data2++;
      }}

      _mm_store_si128((__m128i*)d2,sum0);
      _mm_store_si128((__m128i*)(d2+8),sum1);

      d2 += buf_pitch;
    }}
  }}
}

void BlurFilter::op2_slice_vpass(int slice_y0, int slice_y1)
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  int src_w4 = (src.w+3)/4;
  unsigned char* src_data = (unsigned char*)src.data;
  int src_pitch = src.pitch;

  unsigned short* buf1 = (unsigned short*)(ptrdiff_t(this->buf + bk.w0*4 + 7) & ~15);

  const __m128i zero = _mm_setzero_si128();
  unsigned short* bk_data2 = bk.data2;
  int bk_w = bk.w;

  {for(int y=slice_y0; y<slice_y1; y++){
    unsigned short* d2 = buf1 + (y-slice_y0)*buf_pitch;

    int clamp_y0 = y-bk.w0;
    int cn0 = 0;
    if(clamp_y0<0){ cn0 = -clamp_y0; clamp_y0 = 0; }
    int clamp_y1 = y+bk.w0+1;
    int cn2 = 0;
    if(clamp_y1>src.h){ cn2 = clamp_y1-src.h; clamp_y1 = src.h;  }

    {for(int x=0; x<src_w4; x++){
      __m128i* data2 = (__m128i*)(bk_data2);
      const unsigned char* s = src_data + x*16 + clamp_y0*src_pitch;
      __m128i v;
      __m128i sum0 = zero;
      __m128i sum1 = zero;
      int n0 = cn0;
      if(n0){
        v = _mm_load_si128((__m128i*)s);
        for(; n0; n0--){
          __m128i v0 = _mm_unpacklo_epi8(v,zero);
          __m128i v1 = _mm_unpackhi_epi8(v,zero);
          __m128i m = _mm_load_si128(data2);
          v0 = _mm_mullo_epi16(v0,m);
          v1 = _mm_mullo_epi16(v1,m);
          sum0 = _mm_add_epi16(sum0,v0);
          sum1 = _mm_add_epi16(sum1,v1);
          data2++;
        }
      }
      {for(int n1=clamp_y0; n1<clamp_y1; n1++){
        v = _mm_load_si128((__m128i*)s);
        __m128i v0 = _mm_unpacklo_epi8(v,zero);
        __m128i v1 = _mm_unpackhi_epi8(v,zero);
        __m128i m = _mm_load_si128(data2);
        v0 = _mm_mullo_epi16(v0,m);
        v1 = _mm_mullo_epi16(v1,m);
        sum0 = _mm_add_epi16(sum0,v0);
        sum1 = _mm_add_epi16(sum1,v1);
        data2++;
        s+=src_pitch;
      }}
      int n2 = cn2;
      for(; n2; n2--){
        __m128i v0 = _mm_unpacklo_epi8(v,zero);
        __m128i v1 = _mm_unpackhi_epi8(v,zero);
        __m128i m = _mm_load_si128(data2);
        v0 = _mm_mullo_epi16(v0,m);
        v1 = _mm_mullo_epi16(v1,m);
        sum0 = _mm_add_epi16(sum0,v0);
        sum1 = _mm_add_epi16(sum1,v1);
        data2++;
      }

      _mm_store_si128((__m128i*)d2,sum0);
      _mm_store_si128((__m128i*)(d2+8),sum1);

      d2 += 16;
    }}
  }}
}

void BlurFilter::op2_slice_hpass(int slice_y0, int slice_y1)
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  unsigned short* buf1 = (unsigned short*)(ptrdiff_t(this->buf + bk.w0*4 + 7) & ~15);
  unsigned short* buf0 = buf1 - bk.w0*4;

  {for(int y=slice_y0; y<slice_y1; y++){
    unsigned char* d = (unsigned char*)dst.data + y*dst.pitch;
    unsigned short* row = buf0 + (y-slice_y0)*buf_pitch;
    {
      unsigned short* s = row + (bk.w0)*4;
      int c0 = *(int*)s;
      int c1 = *(int*)(s+2);
      {for(int i=bk.w0; i; i--){
        s-=2;
        *(int*)s = c1;
        s-=2;
        *(int*)s = c0;
      }}
    }
    {
      unsigned short* s = row + (src.w-1+bk.w0)*4;
      int c0 = *(int*)s;
      int c1 = *(int*)(s+2);
      s+=2;
      {for(int i=bk.w0; i; i--){
        s+=2;
        *(int*)s = c0;
        s+=2;
        *(int*)s = c1;
      }}
    }

  	const __m128i zero = _mm_setzero_si128();
    int src_w4 = (src.w+3)/4;
    int bk_w0 = bk.w0;
    int bk_w = bk.w;
    unsigned short* bk_data3 = bk.data3;

    {for(int x=0; x<src_w4; x++){
    	__m128i sum0 = zero;
    	__m128i sum1 = zero;

      unsigned short* s = row + x*16;
      __m128i* data3 = (__m128i*)bk_data3;
      {for(int i=0; i<bk_w; i++){
        __m128i m = _mm_load_si128(data3);
        __m128i v0 = _mm_loadu_si128((__m128i*)s);
        __m128i v1 = _mm_loadu_si128((__m128i*)(s+8));
        v0 = _mm_mulhi_epu16(v0,m);
        v1 = _mm_mulhi_epu16(v1,m);
        sum0 = _mm_add_epi16(sum0,v0);
        sum1 = _mm_add_epi16(sum1,v1);
        s+=4;
        data3++;
      }}

      sum0 = _mm_srli_epi16(sum0,8);
      sum1 = _mm_srli_epi16(sum1,8);
      __m128i v = _mm_packus_epi16(sum0,sum1);
      _mm_store_si128((__m128i*)d,v);
      d+=16;
    }}
  }}
}

void UnsharpFilter::op2_slice_hpass_unsharp(int slice_y0, int slice_y1)
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  unsigned short* buf1 = (unsigned short*)(ptrdiff_t(this->buf + bk.w0*4 + 7) & ~15);
  unsigned short* buf0 = buf1 - bk.w0*4;

  const __m128i powera = _mm_set1_epi16(power+256);
  const __m128i powerb = _mm_set1_epi16(power);

  {for(int y=slice_y0; y<slice_y1; y++){
    const unsigned char* s0 = (const unsigned char*)src.data + y*src.pitch;
    unsigned char* d = (unsigned char*)dst.data + y*dst.pitch;
    unsigned short* row = buf0 + (y-slice_y0)*buf_pitch;
    {
      unsigned short* s = row + (bk.w0)*4;
      int c0 = *(int*)s;
      int c1 = *(int*)(s+2);
      {for(int i=bk.w0; i; i--){
        s-=2;
        *(int*)s = c1;
        s-=2;
        *(int*)s = c0;
      }}
    }
    {
      unsigned short* s = row + (src.w-1+bk.w0)*4;
      int c0 = *(int*)s;
      int c1 = *(int*)(s+2);
      s+=2;
      {for(int i=bk.w0; i; i--){
        s+=2;
        *(int*)s = c0;
        s+=2;
        *(int*)s = c1;
      }}
    }

  	const __m128i zero = _mm_setzero_si128();
    int src_w4 = (src.w+3)/4;
    int bk_w0 = bk.w0;
    int bk_w = bk.w;
    unsigned short* bk_data3 = bk.data3;

    {for(int x=0; x<src_w4; x++){
    	__m128i sum0 = zero;
    	__m128i sum1 = zero;

      unsigned short* s = row + x*16;
      __m128i* data3 = (__m128i*)bk_data3;
      {for(int i=0; i<bk_w; i++){
        __m128i m = _mm_load_si128(data3);
        __m128i v0 = _mm_loadu_si128((__m128i*)s);
        __m128i v1 = _mm_loadu_si128((__m128i*)(s+8));
        v0 = _mm_mulhi_epu16(v0,m);
        v1 = _mm_mulhi_epu16(v1,m);
        sum0 = _mm_add_epi16(sum0,v0);
        sum1 = _mm_add_epi16(sum1,v1);
        s+=4;
        data3++;
      }}

      __m128i c = _mm_load_si128((__m128i*)s0);
      __m128i v0 = _mm_unpacklo_epi8(c,zero);
      __m128i v1 = _mm_unpackhi_epi8(c,zero);
      v0 = _mm_slli_epi16(v0,8);
      v1 = _mm_slli_epi16(v1,8);
      v0 = _mm_mulhi_epu16(v0,powera);
      v1 = _mm_mulhi_epu16(v1,powera);
      sum0 = _mm_mulhi_epu16(sum0,powerb);
      sum1 = _mm_mulhi_epu16(sum1,powerb);
      v0 = _mm_subs_epu16(v0,sum0);
      v1 = _mm_subs_epu16(v1,sum1);
      __m128i v = _mm_packus_epi16(v0,v1);
      _mm_store_si128((__m128i*)d,v);
      
      d+=16;
      s0+=16;
    }}
  }}
}

void UnsharpFilter::op2_slice_hpass_unsharp2(int slice_y0, int slice_y1)
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  unsigned short* buf1 = (unsigned short*)(ptrdiff_t(this->buf + bk.w0*4 + 7) & ~15);
  unsigned short* buf0 = buf1 - bk.w0*4;

  const __m128i powerb = _mm_set1_epi16(power);
  int th_mask = threshold | threshold<<8 | threshold<<16 | 0xFF000000;
  const __m128i th = _mm_set1_epi32(th_mask);

  {for(int y=slice_y0; y<slice_y1; y++){
    const unsigned char* s0 = (const unsigned char*)src.data + y*src.pitch;
    unsigned char* d = (unsigned char*)dst.data + y*dst.pitch;
    unsigned short* row = buf0 + (y-slice_y0)*buf_pitch;
    {
      unsigned short* s = row + (bk.w0)*4;
      int c0 = *(int*)s;
      int c1 = *(int*)(s+2);
      {for(int i=bk.w0; i; i--){
        s-=2;
        *(int*)s = c1;
        s-=2;
        *(int*)s = c0;
      }}
    }
    {
      unsigned short* s = row + (src.w-1+bk.w0)*4;
      int c0 = *(int*)s;
      int c1 = *(int*)(s+2);
      s+=2;
      {for(int i=bk.w0; i; i--){
        s+=2;
        *(int*)s = c0;
        s+=2;
        *(int*)s = c1;
      }}
    }

  	const __m128i zero = _mm_setzero_si128();
    int src_w4 = (src.w+3)/4;
    int bk_w0 = bk.w0;
    int bk_w = bk.w;
    unsigned short* bk_data3 = bk.data3;

    {for(int x=0; x<src_w4; x++){
    	__m128i sum0 = zero;
    	__m128i sum1 = zero;

      unsigned short* s = row + x*16;
      __m128i* data3 = (__m128i*)bk_data3;
      {for(int i=0; i<bk_w; i++){
        __m128i m = _mm_load_si128(data3);
        __m128i v0 = _mm_loadu_si128((__m128i*)s);
        __m128i v1 = _mm_loadu_si128((__m128i*)(s+8));
        v0 = _mm_mulhi_epu16(v0,m);
        v1 = _mm_mulhi_epu16(v1,m);
        sum0 = _mm_add_epi16(sum0,v0);
        sum1 = _mm_add_epi16(sum1,v1);
        s+=4;
        data3++;
      }}

      __m128i c = _mm_load_si128((__m128i*)s0);
      __m128i v0 = _mm_unpacklo_epi8(c,zero);
      __m128i v1 = _mm_unpackhi_epi8(c,zero);
      v0 = _mm_slli_epi16(v0,8);
      v1 = _mm_slli_epi16(v1,8);
      v0 = _mm_mulhi_epu16(v0,powerb);
      v1 = _mm_mulhi_epu16(v1,powerb);
      sum0 = _mm_mulhi_epu16(sum0,powerb);
      sum1 = _mm_mulhi_epu16(sum1,powerb);
      __m128i d0 = _mm_subs_epu16(v0,sum0);
      __m128i d1 = _mm_subs_epu16(v1,sum1);
      d0 = _mm_packus_epi16(d0,d1);
      __m128i d2 = _mm_subs_epu16(sum0,v0);
      __m128i d3 = _mm_subs_epu16(sum1,v1);
      d1 = _mm_packus_epi16(d2,d3);
      __m128i g = _mm_or_si128(d0,d1);
      g = _mm_subs_epu8(g,th);
      g = _mm_cmpeq_epi32(g,zero);
      d0 = _mm_andnot_si128(g,d0);
      d1 = _mm_andnot_si128(g,d1);
      c = _mm_adds_epu8(c,d0);
      c = _mm_subs_epu8(c,d1);
      _mm_store_si128((__m128i*)d,c);

      d+=16;
      s0+=16;
    }}
  }}
}
