/*
 * Derived from FFmpeg filter.
 * Changed to fit it in VirtualDub and deterministic mode.
*/

#include "hqdn3d.h"
#include "stdint.h"
#include <float.h>
#include <math.h>

#pragma warning(disable:4244)

void copy_plane(uint8_t* dst, int dst_pitch, const uint8_t* src, int src_pitch, int w_size, int h)
{
  {for(int y=0; y<h; y++){
    memcpy(dst,src,w_size);
    (uint8_t*&)src += src_pitch;
    (uint8_t*&)dst += dst_pitch;
  }}
}

//----------------------------

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMAX3(a,b,c) FFMAX(FFMAX(a,b),c)
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMIN3(a,b,c) FFMIN(FFMIN(a,b),c)

#if !HAVE_LLRINT
#undef llrint
#define llrint(x) ((long long)rint(x))
#endif /* HAVE_LLRINT */

#if !HAVE_RINT
_inline double rint(double x)
{
    return x >= 0 ? floor(x + 0.5) : ceil(x - 0.5);
}
#endif /* HAVE_RINT */

#if !HAVE_LRINT
_inline long int lrint(double x)
{
    return rint(x);
}
#endif /* HAVE_LRINT */

#ifndef AV_RN16A
#   define AV_RN16A(p) *(uint16_t*)(p)
#endif

#ifndef AV_WN16A
#   define AV_WN16A(p, v) *((uint16_t*)(p)) = uint16_t(v)
#endif

#define LUT_BITS (depth==16 ? 8 : 4)
#define LOAD(x) (((depth == 8 ? src[x] : AV_RN16A(src + (x) * 2)) << (16 - depth))\
                 + (((1 << (16 - depth)) - 1) >> 1))
#define STORE(x,val) (depth == 8 ? dst[x] = (val) >> (16 - depth) : \
                                   AV_WN16A(dst + (x) * 2, (val) >> (16 - depth)))

//#define LOAD(x) *(uint16_t*)(src + x*2)
//#define STORE(x,val) *(uint16_t*)(dst + x*2) = val

_inline uint32_t lowpass(int prev, int cur, int16_t *coef, int depth)
{
    int d = (prev - cur) >> (8 - LUT_BITS);
    return cur + coef[d];
}

_inline void denoise_temporal(uint8_t *src, uint8_t *dst,
                             uint16_t *frame_ant,
                             int w, int h, int sstride, int dstride,
                             int16_t *temporal, int depth)
{
    long x, y;
    uint32_t tmp;

    temporal += 256 << LUT_BITS;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            frame_ant[x] = tmp = lowpass(frame_ant[x], LOAD(x), temporal, depth);
            STORE(x, tmp);
        }
        src += sstride;
        dst += dstride;
        frame_ant += w;
    }
}

_inline void denoise_temporal_init(uint8_t *src,
                             uint16_t *frame_ant,
                             int w, int h, int sstride,
                             int depth)
{
    long x, y;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            frame_ant[x] = LOAD(x);
        }
        src += sstride;
        frame_ant += w;
    }
}

_inline void denoise_spatial(HQDN3DContext *s,
                            uint8_t *src, uint8_t *dst,
                            uint16_t *line_ant, uint16_t *frame_ant,
                            int w, int h, int sstride, int dstride,
                            int16_t *spatial, int16_t *temporal, int depth)
{
    long x, y;
    uint32_t pixel_ant;
    uint32_t tmp;

    spatial  += 256 << LUT_BITS;
    temporal += 256 << LUT_BITS;

    /* First line has no top neighbor. Only left one for each tmp and
     * last frame */
    pixel_ant = LOAD(0);
    for (x = 0; x < w; x++) {
        line_ant[x] = tmp = pixel_ant = lowpass(pixel_ant, LOAD(x), spatial, depth);
        frame_ant[x] = tmp = lowpass(frame_ant[x], tmp, temporal, depth);
        STORE(x, tmp);
    }

    for (y = 1; y < h; y++) {
        src += sstride;
        dst += dstride;
        frame_ant += w;
        if (s->denoise_row[depth]) {
            s->denoise_row[depth](src, dst, line_ant, frame_ant, w, spatial, temporal);
            continue;
        }
        pixel_ant = LOAD(0);
        for (x = 0; x < w-1; x++) {
            line_ant[x] = tmp = lowpass(line_ant[x], pixel_ant, spatial, depth);
            pixel_ant = lowpass(pixel_ant, LOAD(x+1), spatial, depth);
            frame_ant[x] = tmp = lowpass(frame_ant[x], tmp, temporal, depth);
            STORE(x, tmp);
        }
        line_ant[x] = tmp = lowpass(line_ant[x], pixel_ant, spatial, depth);
        frame_ant[x] = tmp = lowpass(frame_ant[x], tmp, temporal, depth);
        STORE(x, tmp);
    }
}

_inline void denoise_spatial_init(HQDN3DContext *s,
                            uint8_t *src,
                            uint16_t *line_ant, uint16_t *frame_ant,
                            int w, int h, int sstride,
                            int16_t *spatial, int depth)
{
    long x, y;
    uint32_t pixel_ant;
    uint32_t tmp;

    spatial  += 256 << LUT_BITS;

    /* First line has no top neighbor. Only left one for each tmp and
     * last frame */
    pixel_ant = LOAD(0);
    for (x = 0; x < w; x++) {
        line_ant[x] = tmp = pixel_ant = lowpass(pixel_ant, LOAD(x), spatial, depth);
        frame_ant[x] = tmp;
    }

    for (y = 1; y < h; y++) {
        src += sstride;
        frame_ant += w;
        pixel_ant = LOAD(0);
        for (x = 0; x < w-1; x++) {
            line_ant[x] = tmp = lowpass(line_ant[x], pixel_ant, spatial, depth);
            pixel_ant = lowpass(pixel_ant, LOAD(x+1), spatial, depth);
            frame_ant[x] = tmp;
        }
        line_ant[x] = tmp = lowpass(line_ant[x], pixel_ant, spatial, depth);
        frame_ant[x] = tmp;
    }
}

_inline int denoise_depth(HQDN3DContext *s,
                         uint8_t *src, uint8_t *dst,
                         uint16_t *line_ant, uint16_t *frame_ant,
                         int w, int h, int sstride, int dstride,
                         int16_t *spatial, int16_t *temporal, int depth)
{
  if (temporal){
    if (spatial[0])
      denoise_spatial(s, src, dst, line_ant, frame_ant,
                      w, h, sstride, dstride, spatial, temporal, depth);
    else
      denoise_temporal(src, dst, frame_ant,
                       w, h, sstride, dstride, temporal, depth);
  } else {
    if (spatial[0])
      denoise_spatial_init(s, src, line_ant, frame_ant,
                      w, h, sstride, spatial, depth);
    else
      denoise_temporal_init(src, frame_ant,
                       w, h, sstride, depth);
  }

  //!emms_c();
  return 0;
}

#define denoise(...)                                                          \
    do {                                                                      \
        int ret = 0;                                                          \
        switch (s->depth) {                                                   \
            case  8: ret = denoise_depth(__VA_ARGS__,  8); break;             \
            case  9: ret = denoise_depth(__VA_ARGS__,  9); break;             \
            case 10: ret = denoise_depth(__VA_ARGS__, 10); break;             \
            case 16: ret = denoise_depth(__VA_ARGS__, 16); break;             \
        }                                                                     \
    } while (0)

int16_t *precalc_coefs(double dist25, int depth)
{
    int i;
    double gamma, simil, C;
    int16_t *ct = (int16_t*)malloc((512<<LUT_BITS)*sizeof(int16_t));
    if (!ct)
        return NULL;

    gamma = log(0.25) / log(1.0 - FFMIN(dist25,252.0)/255.0 - 0.00001);

    for (i = -256<<LUT_BITS; i < 256<<LUT_BITS; i++) {
        double f = ((i<<(9-LUT_BITS)) + (1<<(8-LUT_BITS)) - 1) / 512.0; // midpoint of the bin
        simil = FFMAX(0, 1.0 - fabs(f) / 255.0);
        C = pow(simil, gamma) * 256.0 * f;
        ct[(256<<LUT_BITS)+i] = lrint(C);
    }

    ct[0] = !!dist25;
    return ct;
}

void copy_plane8to16(uint16_t* dst, int dst_pitch, const uint8_t* src, int src_pitch, int w, int h)
{
  {for(int y=0; y<h; y++){
    {for(int x=0; x<w; x++){
      dst[x] = src[x]<<8;
    }}
    (uint8_t*&)src += src_pitch;
    (uint8_t*&)dst += dst_pitch;
  }}
}

void denoise_plane(HQDN3DContext* s, int plane, bool init, VDXFilterActivation* fa, void* dst, int w, int h, int dst_pitch, int16_t* cs, int16_t* ct)
{
  int pos = 0;
  if(init) pos = fa->mSourceFrameCount-1;
  uint16_t* prev = s->frame_prev[plane];

  while(pos>=0){
    const VDXPixmap& pxsrc = *fa->mpSourceFrames[pos]->mpPixmap;
    void* src = pxsrc.data;
    int src_pitch = pxsrc.pitch;
    if(plane==1){ src = pxsrc.data2; src_pitch = pxsrc.pitch2; }
    if(plane==2){ src = pxsrc.data3; src_pitch = pxsrc.pitch3; }

    denoise(s, (uint8_t*)src, (uint8_t*)dst, s->line, prev, w, h, src_pitch, dst_pitch, cs, init ? 0:ct);
    pos--;
    init = false;
  }
}

void Filter::render_op0()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  HQDN3DContext *s = &ctx;
  const int cw = AV_CEIL_RSHIFT(src.w, s->hsub);
  const int ch = AV_CEIL_RSHIFT(src.h, s->vsub);
  int bpp = 1;
  if(s->depth==16) bpp = 2;

  bool init = true;
  if(param.recursive && s->prev_init!=-1) init = false;
  int frame_id = fa->pfsi->lCurrentSourceFrame;
  if(abs(s->prev_init-frame_id)>2) init = true;
  s->prev_init = frame_id;

  if(param.luma_strength!=0 || s->strength[LUMA_TMP]){
    denoise_plane(s, 0, init && s->strength[LUMA_TMP], fa, dst.data, src.w, src.h, dst.pitch, s->coefs[LUMA_SPATIAL], s->coefs[LUMA_TMP]);
  } else {
    copy_plane((uint8_t*)dst.data, dst.pitch, (const uint8_t*)src.data, src.pitch, src.w*bpp, src.h);
  }

  if(param.chroma_strength!=0 || s->strength[CHROMA_TMP]){
    denoise_plane(s, 1, init && s->strength[CHROMA_TMP], fa, dst.data2, cw, ch, dst.pitch2, s->coefs[CHROMA_SPATIAL], s->coefs[CHROMA_TMP]);
    denoise_plane(s, 2, init && s->strength[CHROMA_TMP], fa, dst.data3, cw, ch, dst.pitch3, s->coefs[CHROMA_SPATIAL], s->coefs[CHROMA_TMP]);
  } else {
    copy_plane((uint8_t*)dst.data2, dst.pitch2, (const uint8_t*)src.data2, src.pitch2, cw*bpp, ch);
    copy_plane((uint8_t*)dst.data3, dst.pitch3, (const uint8_t*)src.data3, src.pitch3, cw*bpp, ch);
  }
}
