/*
 * Copyright (c) 2007 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2013 Clément Bœsch <u pkh me>
 *
 * Part of this file is part of FFmpeg.
 * Some code changed to fit it in VirtualDub.
*/

#include "owdenoise.h"
#include "stdint.h"

#ifndef M_SQRT2
#define M_SQRT2        1.41421356237309504880  /* sqrt(2) */
#endif

#define AV_CEIL_RSHIFT(a,b) -((-(a)) >> (b))

static _inline int avpriv_mirror(int x, int w)
{
    if (!w)
        return 0;

    while ((unsigned)x > (unsigned)w) {
        x = -x;
        if (x < 0)
            x += 2 * w;
    }
    return x;
}

static const uint8_t dither[8][8] = {
    {  0,  48,  12,  60,   3,  51,  15,  63 },
    { 32,  16,  44,  28,  35,  19,  47,  31 },
    {  8,  56,   4,  52,  11,  59,   7,  55 },
    { 40,  24,  36,  20,  43,  27,  39,  23 },
    {  2,  50,  14,  62,   1,  49,  13,  61 },
    { 34,  18,  46,  30,  33,  17,  45,  29 },
    { 10,  58,   6,  54,   9,  57,   5,  53 },
    { 42,  26,  38,  22,  41,  25,  37,  21 },
};

static const double coeff[2][5] = {
    {
         0.6029490182363579  * M_SQRT2,
         0.2668641184428723  * M_SQRT2,
        -0.07822326652898785 * M_SQRT2,
        -0.01686411844287495 * M_SQRT2,
         0.02674875741080976 * M_SQRT2,
    },{
         1.115087052456994   / M_SQRT2,
        -0.5912717631142470  / M_SQRT2,
        -0.05754352622849957 / M_SQRT2,
         0.09127176311424948 / M_SQRT2,
    }
};

static const double icoeff[2][5] = {
    {
         1.115087052456994   / M_SQRT2,
         0.5912717631142470  / M_SQRT2,
        -0.05754352622849957 / M_SQRT2,
        -0.09127176311424948 / M_SQRT2,
    },{
         0.6029490182363579  * M_SQRT2,
        -0.2668641184428723  * M_SQRT2,
        -0.07822326652898785 * M_SQRT2,
         0.01686411844287495 * M_SQRT2,
         0.02674875741080976 * M_SQRT2,
    }
};


static inline void decompose(float *dst_l, float *dst_h, const float *src,
                             int linesize, int w)
{
    int x, i;
    for (x = 0; x < w; x++) {
        double sum_l = src[x * linesize] * coeff[0][0];
        double sum_h = src[x * linesize] * coeff[1][0];
        for (i = 1; i <= 4; i++) {
            const double s = src[avpriv_mirror(x - i, w - 1) * linesize]
                           + src[avpriv_mirror(x + i, w - 1) * linesize];

            sum_l += coeff[0][i] * s;
            sum_h += coeff[1][i] * s;
        }
        dst_l[x * linesize] = sum_l;
        dst_h[x * linesize] = sum_h;
    }
}

static inline void compose(float *dst, const float *src_l, const float *src_h,
                           int linesize, int w)
{
    int x, i;
    for (x = 0; x < w; x++) {
        double sum_l = src_l[x * linesize] * icoeff[0][0];
        double sum_h = src_h[x * linesize] * icoeff[1][0];
        for (i = 1; i <= 4; i++) {
            const int x0 = avpriv_mirror(x - i, w - 1) * linesize;
            const int x1 = avpriv_mirror(x + i, w - 1) * linesize;

            sum_l += icoeff[0][i] * (src_l[x0] + src_l[x1]);
            sum_h += icoeff[1][i] * (src_h[x0] + src_h[x1]);
        }
        dst[x * linesize] = (sum_l + sum_h) * 0.5;
    }
}

static inline void decompose2D(float *dst_l, float *dst_h, const float *src,
                               int xlinesize, int ylinesize,
                               int step, int w, int h)
{
    int y, x;
    for (y = 0; y < h; y++)
        for (x = 0; x < step; x++)
            decompose(dst_l + ylinesize*y + xlinesize*x,
                      dst_h + ylinesize*y + xlinesize*x,
                      src   + ylinesize*y + xlinesize*x,
                      step * xlinesize, (w - x + step - 1) / step);
}

static inline void compose2D(float *dst, const float *src_l, const float *src_h,
                             int xlinesize, int ylinesize,
                             int step, int w, int h)
{
    int y, x;
    for (y = 0; y < h; y++)
        for (x = 0; x < step; x++)
            compose(dst   + ylinesize*y + xlinesize*x,
                    src_l + ylinesize*y + xlinesize*x,
                    src_h + ylinesize*y + xlinesize*x,
                    step * xlinesize, (w - x + step - 1) / step);
}

static void decompose2D2(float *dst[4], float *src, float *temp[2],
                         int linesize, int step, int w, int h)
{
    decompose2D(temp[0], temp[1], src,     1, linesize, step, w, h);
    decompose2D( dst[0],  dst[1], temp[0], linesize, 1, step, h, w);
    decompose2D( dst[2],  dst[3], temp[1], linesize, 1, step, h, w);
}

static void compose2D2(float *dst, float *src[4], float *temp[2],
                       int linesize, int step, int w, int h)
{
    compose2D(temp[0],  src[0],  src[1], linesize, 1, step, h, w);
    compose2D(temp[1],  src[2],  src[3], linesize, 1, step, h, w);
    compose2D(dst,     temp[0], temp[1], 1, linesize, step, w, h);
}

static void filter(OWDenoiseContext *s,
                   uint8_t       *dst, int dst_linesize,
                   const uint8_t *src, int src_linesize,
                   int width, int height, double strength)
{
    int x, y, i, j, depth = s->depth;

    while (1<<depth > width || 1<<depth > height)
        depth--;

    if (s->pixel_depth <= 8) {
        for (y = 0; y < height; y++)
            for(x = 0; x < width; x++)
                s->plane[0][0][y*s->linesize + x] = src[y*src_linesize + x];
    } else {
        const uint16_t *src16 = (const uint16_t *)src;

        src_linesize /= 2;
        for (y = 0; y < height; y++)
            for(x = 0; x < width; x++)
                s->plane[0][0][y*s->linesize + x] = src16[y*src_linesize + x];
    }

    for (i = 0; i < depth; i++)
        decompose2D2(s->plane[i + 1], s->plane[i][0], s->plane[0] + 1, s->linesize, 1<<i, width, height);

    for (i = 0; i < depth; i++) {
        for (j = 1; j < 4; j++) {
            for (y = 0; y < height; y++) {
                for (x = 0; x < width; x++) {
                    double v = s->plane[i + 1][j][y*s->linesize + x];
                    if      (v >  strength) v -= strength;
                    else if (v < -strength) v += strength;
                    else                    v  = 0;
                    s->plane[i + 1][j][x + y*s->linesize] = v;
                }
            }
        }
    }
    for (i = depth-1; i >= 0; i--)
        compose2D2(s->plane[i][0], s->plane[i + 1], s->plane[0] + 1, s->linesize, 1<<i, width, height);

    if (s->pixel_depth <= 8) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                i = s->plane[0][0][y*s->linesize + x] + dither[x&7][y&7]*(1.0/64) + 1.0/128; // yes the rounding is insane but optimal :)
                if ((unsigned)i > 255U) i = ~(i >> 31);
                dst[y*dst_linesize + x] = i;
            }
        }
    } else {
        uint16_t *dst16 = (uint16_t *)dst;

        dst_linesize /= 2;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                i = s->plane[0][0][y*s->linesize + x];
                dst16[y*dst_linesize + x] = i;
            }
        }
    }
}

void copy_plane(uint8_t* dst, int dst_pitch, const uint8_t* src, int src_pitch, int w_size, int h)
{
  {for(int y=0; y<h; y++){
    memcpy(dst,src,w_size);
    (uint8_t*&)src += src_pitch;
    (uint8_t*&)dst += dst_pitch;
  }}
}

void Filter::render_op0()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  OWDenoiseContext *s = &ctx;
  const int cw = AV_CEIL_RSHIFT(src.w, s->hsub);
  const int ch = AV_CEIL_RSHIFT(src.h, s->vsub);
  int bpp = 1;

  if(s->pixel_depth==16){
    FilterModPixmapInfo& dst_info = *fma->fmpixmap->GetPixmapInfo(&dst);
    dst_info.ref_r = 0xFFFF;
  }

  if (s->luma_strength > 0) {
    filter(s, (uint8_t*)dst.data, dst.pitch, (const uint8_t*)src.data, src.pitch, src.w, src.h, s->luma_strength);
  } else {
    copy_plane((uint8_t*)dst.data, dst.pitch, (const uint8_t*)src.data, src.pitch, src.w*bpp, src.h);
  }
  if (s->chroma_strength > 0) {
    filter(s, (uint8_t*)dst.data2, dst.pitch2, (const uint8_t*)src.data2, src.pitch2, cw, ch, s->chroma_strength);
    filter(s, (uint8_t*)dst.data3, dst.pitch3, (const uint8_t*)src.data3, src.pitch3, cw, ch, s->chroma_strength);
  } else {
    copy_plane((uint8_t*)dst.data2, dst.pitch2, (const uint8_t*)src.data2, src.pitch2, cw*bpp, ch);
    copy_plane((uint8_t*)dst.data3, dst.pitch3, (const uint8_t*)src.data3, src.pitch3, cw*bpp, ch);
  }
}

