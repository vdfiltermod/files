#include "crossfade.h"
#include <emmintrin.h>

void CopyPlane(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, uint32 size, uint32 h) {
  if(!dst0) return;
  uint32 w16 = (size + 15) >> 4;
  uint8* dst = (uint8*)dst0;
  const uint8* src = (uint8*)src0;

  {for(int i=0; i<(int)h; i++){
    memcpy(dst,src,w16*16);
    dst += dstpitch;
    src += srcpitch;
  }}
}

// copied from VirtualDub interpolate filter
void Lerp8_SSE2(void *dst0, ptrdiff_t dstpitch, const void *src10, const void *src20, ptrdiff_t srcpitch, uint32 w, uint32 h, uint32 alpha) {
  if(!dst0) return;
  uint32 w16 = (w + 15) >> 4;
  uint8 *dst = (uint8 *)dst0;
  const uint8 *src1 = (const uint8 *)src10;
  const uint8 *src2 = (const uint8 *)src20;

  int coeffa = (256 - (int)alpha) * 0x00010001;
  int coeffb = (int)alpha * 0x00010001;

  const __m128i coeffa1 = _mm_shuffle_epi32(_mm_cvtsi32_si128(coeffa), 0);
  const __m128i coeffb1 = _mm_shuffle_epi32(_mm_cvtsi32_si128(coeffb), 0);
  const __m128i coeffa0 = _mm_slli_epi16(coeffa1, 8);
  const __m128i coeffb0 = _mm_slli_epi16(coeffb1, 8);
  const __m128i round = { (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0 };
  const __m128i mask = { 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF };

  do {
    __m128i *d = (__m128i *)dst;
    const __m128i *s1 = (const __m128i *)src1;
    const __m128i *s2 = (const __m128i *)src2;
    for(uint32 x=0; x<w16; ++x) {
      __m128i a = *s1++;
      __m128i b = *s2++;
      __m128i a0 = _mm_slli_epi16(a, 8);
      __m128i a1 = _mm_srli_epi16(a, 8);
      __m128i b0 = _mm_slli_epi16(b, 8);
      __m128i b1 = _mm_srli_epi16(b, 8);
      __m128i r0 = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(_mm_mulhi_epu16(a0, coeffa0), _mm_mulhi_epu16(b0, coeffb0)), round), 8);
      __m128i r1 = _mm_add_epi16(_mm_add_epi16(_mm_mullo_epi16(a1, coeffa1), _mm_mullo_epi16(b1, coeffb1)), round);
      __m128i r = _mm_or_si128(r0, _mm_and_si128(r1, mask));

      *d++ = r;
    }

    dst += dstpitch;
    src1 += srcpitch;
    src2 += srcpitch;
  } while(--h);
}

void Lerp16_SSE2(void *dst0, ptrdiff_t dstpitch, const void *src10, const void *src20, ptrdiff_t srcpitch, uint32 w, uint32 h, uint32 alpha) {
  if(!dst0) return;
  if(alpha==0){
    CopyPlane(dst0,dstpitch,src10,srcpitch,w*2,h);
    return;
  }
  if(alpha==0x10000){
    CopyPlane(dst0,dstpitch,src20,srcpitch,w*2,h);
    return;
  }

  const __m128i const_m0 = _mm_set1_epi16(0x10000-alpha);
  const __m128i const_m1 = _mm_set1_epi16(alpha);
  int w8 = (w + 7) >> 3;

  {for(int y=0; y<(int)h; y++){
    uint8 *dst = (uint8 *)dst0 + dstpitch*y;
    const uint8 *src1 = (const uint8 *)src10 + srcpitch*y;
    const uint8 *src2 = (const uint8 *)src20 + srcpitch*y;

    {for(int x=0; x<w8; x++){
      __m128i a0 = _mm_load_si128((const __m128i*)src1);
      __m128i a1 = _mm_load_si128((const __m128i*)src2);
      __m128i b0 = _mm_mullo_epi16(a0,const_m0);
      __m128i b1 = _mm_mullo_epi16(a1,const_m1);
      b0 = _mm_adds_epu16(b0,b1);
      b0 = _mm_srli_epi16(b0,15);
      a0 = _mm_mulhi_epu16(a0,const_m0);
      a1 = _mm_mulhi_epu16(a1,const_m1);
      a0 = _mm_adds_epu16(a0,a1);
      a0 = _mm_adds_epu16(a0,b0);
      _mm_store_si128((__m128i*)dst,a0);

      dst += 16;
      src1 += 16;
      src2 += 16;
    }}
  }}
}

void Filter::render()
{
  const VDXPixmap& pxsrc1 = *fa->mpSourceFrames[0]->mpPixmap;
  const VDXPixmap& pxdst = *fa->mpOutputFrames[0]->mpPixmap;

  int bpp = 1;
  int pw = pxsrc1.w;
  int ph = pxsrc1.h;
  int cw = pxsrc1.w;
  int ch = pxsrc1.h;

  switch(pxdst.format){
  case nsVDXPixmap::kPixFormat_YUV444_Planar16:
  case nsVDXPixmap::kPixFormat_YUV422_Planar16:
  case nsVDXPixmap::kPixFormat_YUV420_Planar16:
  case nsVDXPixmap::kPixFormat_XRGB64:
    bpp = 2;
  }

  switch(pxdst.format){
  case nsVDXPixmap::kPixFormat_YUV422_Planar:
  case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
  case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
  case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
  case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    cw /= 2;
    break;
  case nsVDXPixmap::kPixFormat_YUV420_Planar:
  case nsVDXPixmap::kPixFormat_YUV420_Planar_FR:
  case nsVDXPixmap::kPixFormat_YUV420_Planar_709:
  case nsVDXPixmap::kPixFormat_YUV420_Planar_709_FR:
  case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    cw /= 2;
    ch /= 2;
    break;
  case nsVDXPixmap::kPixFormat_RGB888:
    pw *= 3;
    break;
  case nsVDXPixmap::kPixFormat_XRGB8888:
  case nsVDXPixmap::kPixFormat_XRGB64:
    pw *= 4;
    break;
  }

  if(fa->mSourceFrameCount==1){
    CopyPlane(pxdst.data,  pxdst.pitch,  pxsrc1.data,  pxsrc1.pitch,  pw*bpp, ph);
    CopyPlane(pxdst.data2, pxdst.pitch2, pxsrc1.data2, pxsrc1.pitch2, cw*bpp, ch);
    CopyPlane(pxdst.data3, pxdst.pitch3, pxsrc1.data3, pxsrc1.pitch3, cw*bpp, ch);
    return;
  }

  const VDXPixmap& pxsrc2 = *fa->mpSourceFrames[1]->mpPixmap;
  sint64 f = fa->mpOutputFrames[0]->mFrameNumber;

  if(bpp==1){
    uint32 alpha = (f-(param.pos-param.width-1))*255/(param.width+1);

    Lerp8_SSE2(pxdst.data,  pxdst.pitch,  pxsrc1.data,  pxsrc2.data,  pxsrc1.pitch,  pw, ph, alpha);
    Lerp8_SSE2(pxdst.data2, pxdst.pitch2, pxsrc1.data2, pxsrc2.data2, pxsrc1.pitch2, cw, ch, alpha);
    Lerp8_SSE2(pxdst.data3, pxdst.pitch3, pxsrc1.data3, pxsrc2.data3, pxsrc1.pitch3, cw, ch, alpha);
  }

  if(bpp==2){
    FilterModPixmapInfo& src1_info = *fma->fmpixmap->GetPixmapInfo(&pxsrc1);
    FilterModPixmapInfo& src2_info = *fma->fmpixmap->GetPixmapInfo(&pxsrc2);
    FilterModPixmapInfo& dst_info = *fma->fmpixmap->GetPixmapInfo(&pxdst);
    dst_info.ref_r = 0xFFFF;
    dst_info.ref_g = 0xFFFF;
    dst_info.ref_b = 0xFFFF;
    dst_info.ref_a = 0xFFFF;
    dst_info.alpha_type = FilterModPixmapInfo::kAlphaInvalid;
    if(src1_info.alpha_type==src2_info.alpha_type) dst_info.alpha_type = src1_info.alpha_type;

    uint32 alpha = (f-(param.pos-param.width-1))*0x10000/(param.width+1);

    Lerp16_SSE2(pxdst.data,  pxdst.pitch,  pxsrc1.data,  pxsrc2.data,  pxsrc1.pitch,  pw, ph, alpha);
    Lerp16_SSE2(pxdst.data2, pxdst.pitch2, pxsrc1.data2, pxsrc2.data2, pxsrc1.pitch2, cw, ch, alpha);
    Lerp16_SSE2(pxdst.data3, pxdst.pitch3, pxsrc1.data3, pxsrc2.data3, pxsrc1.pitch3, cw, ch, alpha);
  }
}

