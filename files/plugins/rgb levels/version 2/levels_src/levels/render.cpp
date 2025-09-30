#include "levels.h"
#include <emmintrin.h>

void LevelsFilter::render_xrgb8()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  int w = (src.w+3)/4;
  int h = src.h;

  uint32 dr = uint32(0xFF*param.r.v0);
  uint32 dg = uint32(0xFF*param.g.v0);
  uint32 db = uint32(0xFF*param.b.v0);
  uint32 hr = uint32(0xFF*param.r.v2);
  uint32 hg = uint32(0xFF*param.g.v2);
  uint32 hb = uint32(0xFF*param.b.v2);
  float mr1 = 0xFF00/float(hr-dr);
  float mg1 = 0xFF00/float(hg-dg);
  float mb1 = 0xFF00/float(hb-db);
  uint32 mr,mg,mb;
  if(mr1<0xFFFF) mr=uint32(mr1); else mr = 0xFFFF;
  if(mg1<0xFFFF) mg=uint32(mg1); else mg = 0xFFFF;
  if(mb1<0xFFFF) mb=uint32(mb1); else mb = 0xFFFF;
  uint32 r1 = 0xFF-(0xFFFF/mr);
  uint32 g1 = 0xFF-(0xFFFF/mg);
  uint32 b1 = 0xFF-(0xFFFF/mb);
  uint32 r0 = r1>dr ? r1-dr : 0;
  uint32 g0 = g1>dg ? g1-dg : 0;
  uint32 b0 = b1>db ? b1-db : 0;

  const __m128i constd0 = _mm_set_epi8(0, r0, g0, b0, 0, r0, g0, b0, 0, r0, g0, b0, 0, r0, g0, b0);
  const __m128i constd1 = _mm_set_epi8(0, r1, g1, b1, 0, r1, g1, b1, 0, r1, g1, b1, 0, r1, g1, b1);
  const __m128i constm = _mm_set_epi16(0x100,mr,mg,mb,0x100,mr,mg,mb);
  const __m128i zero = _mm_setzero_si128();

  {for(int y=0; y<h; y++){
    const unsigned char* s = (const unsigned char*)src.data + src.pitch*y;
    unsigned char* d = (unsigned char*)dst.data + dst.pitch*y;

    {for(int x=0; x<w; x++){
      __m128i c0 = _mm_load_si128((__m128i*)s);
      c0 = _mm_adds_epu8(c0,constd0);
      c0 = _mm_subs_epu8(c0,constd1);

      __m128i v0 = _mm_unpacklo_epi8(zero,c0);
      __m128i v1 = _mm_unpackhi_epi8(zero,c0);
      v0 = _mm_mulhi_epu16(v0,constm);
      v1 = _mm_mulhi_epu16(v1,constm);

      c0 = _mm_packus_epi16(v0,v1);
      _mm_store_si128((__m128i*)d,c0);
      d+=16;
      s+=16;
    }}
  }}
}

void LevelsFilter::render_xrgb64()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  FilterModPixmapInfo& src_info = *fma->fmpixmap->GetPixmapInfo(&src);
  FilterModPixmapInfo& dst_info = *fma->fmpixmap->GetPixmapInfo(&dst);

  dst_info.ref_r = 0xFFFF;
  dst_info.ref_g = 0xFFFF;
  dst_info.ref_b = 0xFFFF;

  int w = (src.w+1)/2;
  int h = src.h;

  uint32 dr = uint32(src_info.ref_r*param.r.v0);
  uint32 dg = uint32(src_info.ref_g*param.g.v0);
  uint32 db = uint32(src_info.ref_b*param.b.v0);
  uint32 hr = uint32(src_info.ref_r*param.r.v2);
  uint32 hg = uint32(src_info.ref_g*param.g.v2);
  uint32 hb = uint32(src_info.ref_b*param.b.v2);
  float mr1 = 0x10000/float(hr-dr)*dst_info.ref_r;
  float mg1 = 0x10000/float(hg-dg)*dst_info.ref_g;
  float mb1 = 0x10000/float(hb-db)*dst_info.ref_b;
  uint32 mr,mg,mb;
  if(mr1<0xFFFFFFFF) mr=uint32(mr1); else mr = 0xFFFFFFFF;
  if(mg1<0xFFFFFFFF) mg=uint32(mg1); else mg = 0xFFFFFFFF;
  if(mb1<0xFFFFFFFF) mb=uint32(mb1); else mb = 0xFFFFFFFF;
  uint32 r1 = 0xFFFF-(0xFFFFFFFF/mr);
  uint32 g1 = 0xFFFF-(0xFFFFFFFF/mg);
  uint32 b1 = 0xFFFF-(0xFFFFFFFF/mb);
  uint32 r0 = r1>dr ? r1-dr : 0;
  uint32 g0 = g1>dg ? g1-dg : 0;
  uint32 b0 = b1>db ? b1-db : 0;

  const __m128i constd0 = _mm_set_epi16(0, r0, g0, b0, 0, r0, g0, b0);
  const __m128i constd1 = _mm_set_epi16(0, r1, g1, b1, 0, r1, g1, b1);
  const __m128i constmhi = _mm_set_epi16(1, mr>>16, mg>>16, mb>>16, 1, mr>>16, mg>>16, mb>>16);
  const __m128i constmlo = _mm_set_epi16(0, mr & 0xFFFF, mg & 0xFFFF, mb & 0xFFFF, 0, mr & 0xFFFF, mg & 0xFFFF, mb & 0xFFFF);

  {for(int y=0; y<h; y++){
    const unsigned char* s = (const unsigned char*)src.data + src.pitch*y;
    unsigned char* d = (unsigned char*)dst.data + dst.pitch*y;

    {for(int x=0; x<w; x++){
      __m128i c0 = _mm_load_si128((__m128i*)s);
      c0 = _mm_adds_epu16(c0,constd0);
      c0 = _mm_subs_epu16(c0,constd1);

      __m128i v0 = _mm_mullo_epi16(c0,constmhi);
      __m128i v1 = _mm_mulhi_epu16(c0,constmlo);
      v0 = _mm_adds_epu16(v0,v1);

      _mm_store_si128((__m128i*)d,v0);
      d+=16;
      s+=16;
    }}
  }}
}
