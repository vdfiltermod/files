#include "levels.h"
#include <emmintrin.h>

#pragma warning(disable:4305)
#pragma warning(disable:4244)

#ifdef _WIN64
static __inline int simple_ftoi(double f){
  return f;
}
#else
static __inline int simple_ftoi(double f){
  int i;
  __asm{
    fld f
    fistp i
  }
  return i;
}
#endif

float LevelsFilter::eval_color_level(float d, LevelsParam::Channel& c){
  float r = powf(d/c.v2, c.p);
  return r/d;
}

float LevelsFilter::eval_color_level(float u, float v){
  float d = sqrtf(u*u+v*v);
  if(d==0) return 1;

  float a = atan2(u,v)*180/3.1416;
  float r0;
  float r1;
  float f;

  if(a<90 && a>=0){
    r0 = eval_color_level(d,param.cc[0]);
    r1 = eval_color_level(d,param.cc[1]);
    f = (a-0)/(90-0);
  }
  if(a<0 && a>=-45){
    r0 = eval_color_level(d,param.cc[1]);
    r1 = eval_color_level(d,param.cc[2]);
    f = (a+45)/(0+45);
  }
  if(a<-45 && a>=-90){
    r0 = eval_color_level(d,param.cc[2]);
    r1 = eval_color_level(d,param.cc[3]);
    f = (a+90)/(-45+90);
  }
  if(a<-90 && a>=-190){
    r0 = eval_color_level(d,param.cc[3]);
    r1 = eval_color_level(d,param.cc[4]);
    f = (a+180)/(-90+180);
  }
  if(a<190 && a>=135){
    r0 = eval_color_level(d,param.cc[4]);
    r1 = eval_color_level(d,param.cc[5]);
    f = (a-135)/(180-135);
  }
  if(a<135 && a>=90){
    r0 = eval_color_level(d,param.cc[5]);
    r1 = eval_color_level(d,param.cc[0]);
    f = (a-90)/(135-90);
  }

  f = (3-2*f)*f*f;
  float r = r0*f + r1*(1-f);
  float s = param.cs.v2;
  return r*s;
}

void LevelsFilter::eval_tables()
{
  param.ci.eval_p();
  {for(int i=0; i<2048; i++){
    double x = float(i)/2048;
    if(x<param.ci.v0){
      luma[i] = 0;
    } else if(x>param.ci.v2){
      luma[i] = 2047;
    } else {
      luma[i] = powf((x-param.ci.v0)/(param.ci.v2-param.ci.v0), param.ci.p)*2048;
    }
  }}

  have_color = false;
  {for(int i=0; i<6; i++){
    LevelsParam::Channel& c = param.cc[i];
    if(c.v1!=0.5 || c.v2!=1) have_color = true;
    c.eval_p();
  }}
  if(!have_color){
    float m = param.cs.v2*2048;
    if(m<0x7fff)
      color[0][0] = m;
    else
      color[0][0] = 0x7fff;

  } else {
    float color_scale = 2048*4096.0/3969;

    {for(int x=0; x<color_map_size; x++) for(int y=0; y<color_map_size; y++){
      float u = (float(x)/color_map_size-0.5)*2;
      float v = (float(y)/color_map_size-0.5)*2;

      float m = eval_color_level(u,v)*color_scale;
      if(m<0x7fff)
        color[x][y] = m;
      else
        color[x][y] = 0x7fff;
    }}
  }
}

// base 2048
static const int rgb_to_lab[9]={
  455, 1447,  146,
  681,-1024,  343,
  263,  761,-1024,
};

// base 2048
static const int lab_to_rgb[9]={
 2048, 4142, 1679,
 2048,-1311, -147,
 2048,   89,-3774,
};

//#pragma optimize("",off)


void LevelsFilter::render_op1()
{
  uint16 luma[2048];
  uint16 color[color_map_size][color_map_size];
  memcpy(luma,this->luma,sizeof(luma));
  if(have_color) memcpy(color,this->color,sizeof(color)); else color[0][0] = this->color[0][0];
  uint16* p_luma = luma;
  uint16* p_color = &color[0][0];

  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  int A0 = (-param.d0.v0)*65536;
  int B0 = (-param.d1.v0)*65536;
  if(A0>32767) A0 = 32767;
  if(B0>32767) B0 = 32767;
  if(A0<-32768) A0 = -32768;
  if(B0<-32768) B0 = -32768;

  int w = (src.w+3)/4;
  int w2 = (src.w+7)/8;
  int16* row_start = (int16*)(size_t(buf+15) & ~15);
  memset(row_start+w*16, 0, (w2*2-w)*32);

  {for(int y=0; y<src.h; y++){
    const unsigned char* s = (const unsigned char*)src.data + src.pitch*y;
    int16* row;
    row = row_start;

  	const __m128i zero = _mm_setzero_si128();
    const int z = 16;
    const int ma = 1024*z;
    const __m128i crgb = _mm_set_epi16(ma,  rgb_to_lab[0]*z, rgb_to_lab[4]*z, rgb_to_lab[8]*z, ma,  rgb_to_lab[0]*z, rgb_to_lab[4]*z, rgb_to_lab[8]*z);
    const __m128i cgbr = _mm_set_epi16(ma,  rgb_to_lab[1]*z, rgb_to_lab[5]*z, rgb_to_lab[6]*z, ma,  rgb_to_lab[1]*z, rgb_to_lab[5]*z, rgb_to_lab[6]*z);
    const __m128i cbrg = _mm_set_epi16(0,   rgb_to_lab[2]*z, rgb_to_lab[3]*z, rgb_to_lab[7]*z, 0,   rgb_to_lab[2]*z, rgb_to_lab[3]*z, rgb_to_lab[7]*z);
    const __m128i cab0 = _mm_set_epi16(A0, B0, A0, B0, A0, B0, A0, B0);

    {for(int x=0; x<w; x++){
      __m128i c0 = _mm_load_si128((__m128i*)s);
      __m128i ab0,ab1;
      {
        __m128i rgb = _mm_slli_epi16(_mm_unpacklo_epi8(c0,zero),7);             // 3 2 1 0
        __m128i gbr = _mm_shufflelo_epi16(_mm_shufflehi_epi16(rgb,0xD2),0xD2);  // 3 1 0 2
        __m128i brg = _mm_shufflelo_epi16(_mm_shufflehi_epi16(rgb,0xC9),0xC9);  // 3 0 2 1

        __m128i v0 = _mm_mulhi_epi16(rgb,crgb);
        __m128i v1 = _mm_mulhi_epi16(gbr,cgbr);
        __m128i v2 = _mm_mulhi_epi16(brg,cbrg);
        v0 = _mm_add_epi16(v0,v1);
        v0 = _mm_add_epi16(v0,v2);
        ab0 = _mm_shuffle_epi32(v0,0xD8); // 3 1 2 0
      }

      {
        __m128i rgb = _mm_slli_epi16(_mm_unpackhi_epi8(c0,zero),7);             // 3 2 1 0
        __m128i gbr = _mm_shufflelo_epi16(_mm_shufflehi_epi16(rgb,0xD2),0xD2);  // 3 1 0 2
        __m128i brg = _mm_shufflelo_epi16(_mm_shufflehi_epi16(rgb,0xC9),0xC9);  // 3 0 2 1

        __m128i v0 = _mm_mulhi_epi16(rgb,crgb);
        __m128i v1 = _mm_mulhi_epi16(gbr,cgbr);
        __m128i v2 = _mm_mulhi_epi16(brg,cbrg);
        v0 = _mm_add_epi16(v0,v1);
        v0 = _mm_add_epi16(v0,v2);
        ab1 = _mm_shuffle_epi32(v0,0xD8); // 3 1 2 0
      }

      __m128i xlxl = _mm_unpackhi_epi64(ab0,ab1);
      __m128i mm = _mm_shufflelo_epi16(_mm_shufflehi_epi16(xlxl,0xA0),0xA0); // 2 2 0 0
      xlxl = _mm_srli_epi16(xlxl,3);
      _mm_store_si128((__m128i*)(row+8),xlxl);

      mm = _mm_mulhi_epi16(mm,cab0);
      __m128i abab = _mm_unpacklo_epi64(ab0,ab1);
      abab = _mm_add_epi16(abab,mm);
      abab = _mm_srai_epi16(abab,3);
      _mm_store_si128((__m128i*)(row),abab);

      row+=16;
      s+=16;
    }}

    row = row_start;

    const __m128i cr63 = _mm_set1_epi16((int16)0xFC00);
    const __m128i cmax = _mm_set1_epi16(1023);
    const __m128i cmin = _mm_set1_epi16(-1023);
    const __m128i cc00 = _mm_set1_epi16((int16)p_color[0]);

    if(!have_color){for(int x=0; x<w2; x++){
      {for(int i=0; i<4; i++){
        row[8] = p_luma[row[8]];
        row+=2;
      }}

      {for(int i=0; i<4; i++){
        row[16] = p_luma[row[16]];
        row+=2;
      }}

      row+=16;

      __m128i ab0 = _mm_load_si128((__m128i*)(row-32));
      ab0 = _mm_min_epi16(ab0,cmax);
      ab0 = _mm_max_epi16(ab0,cmin);
      ab0 = _mm_slli_epi16(ab0,5);
      ab0 = _mm_mulhi_epi16(ab0,cc00);
      _mm_store_si128((__m128i*)(row-32),ab0);
      __m128i ab1 = _mm_load_si128((__m128i*)(row-16));
      ab1 = _mm_min_epi16(ab1,cmax);
      ab1 = _mm_max_epi16(ab1,cmin);
      ab1 = _mm_slli_epi16(ab1,5);
      ab1 = _mm_mulhi_epi16(ab1,cc00);
      _mm_store_si128((__m128i*)(row-16),ab1);
    }}

    if(have_color){for(int x=0; x<w2; x++){
      __m128i c0,c1,c2,c3;
      __m128i mx,my;

      {for(int i=0; i<4; i++){
        int CA = row[1];
        int CB = row[0];
        int CAM = CA+2048;
        int CBM = CB+2048;
        int px = CAM>>6;
        int py = CBM>>6;

        uint16* cm = p_color + px*color_map_size + py;
        c0.m128i_u16[i] = cm[0];
        c1.m128i_u16[i] = cm[1];
        c2.m128i_u16[i] = cm[color_map_size];
        c3.m128i_u16[i] = cm[color_map_size+1];
        mx.m128i_u16[i] = CAM;
        my.m128i_u16[i] = CBM;
        row+=2;
      }}

      {for(int i=0; i<4; i++){
        int CA = row[1+8];
        int CB = row[0+8];
        int CAM = CA+2048;
        int CBM = CB+2048;
        int px = CAM>>6;
        int py = CBM>>6;

        uint16* cm = p_color + px*color_map_size + py;
        c0.m128i_u16[i+4] = cm[0];
        c1.m128i_u16[i+4] = cm[1];
        c2.m128i_u16[i+4] = cm[color_map_size];
        c3.m128i_u16[i+4] = cm[color_map_size+1];
        mx.m128i_u16[i+4] = CAM;
        my.m128i_u16[i+4] = CBM;
        row+=2;
      }}

      {for(int i=0; i<4; i++){
        row[-8] = p_luma[row[-8]];
        row+=2;
      }}

      {for(int i=0; i<4; i++){
        row[0] = p_luma[row[0]];
        row+=2;
      }}

      my = _mm_slli_epi16(my,10);
      __m128i v1 = _mm_mulhi_epu16(c1,my);
      __m128i v3 = _mm_mulhi_epu16(c3,my);
      my = _mm_xor_si128(my,cr63);
      __m128i v0 = _mm_mulhi_epu16(c0,my);
      __m128i v2 = _mm_mulhi_epu16(c2,my);
      v0 = _mm_add_epi16(v0,v1);
      v2 = _mm_add_epi16(v2,v3);
      mx = _mm_slli_epi16(mx,10);
      v2 = _mm_mulhi_epu16(v2,mx);
      mx = _mm_xor_si128(mx,cr63);
      v0 = _mm_mulhi_epu16(v0,mx);
      v0 = _mm_add_epi16(v0,v2);

      __m128i m0 = _mm_unpacklo_epi16(v0,v0);
      __m128i ab0 = _mm_load_si128((__m128i*)(row-32));
      ab0 = _mm_min_epi16(ab0,cmax);
      ab0 = _mm_max_epi16(ab0,cmin);
      ab0 = _mm_slli_epi16(ab0,5);
      ab0 = _mm_mulhi_epi16(ab0,m0);
      _mm_store_si128((__m128i*)(row-32),ab0);
      __m128i m1 = _mm_unpackhi_epi16(v0,v0);
      __m128i ab1 = _mm_load_si128((__m128i*)(row-16));
      ab1 = _mm_min_epi16(ab1,cmax);
      ab1 = _mm_max_epi16(ab1,cmin);
      ab1 = _mm_slli_epi16(ab1,5);
      ab1 = _mm_mulhi_epi16(ab1,m1);
      _mm_store_si128((__m128i*)(row-16),ab1);
    }}

    row = row_start;
    unsigned char* d = (unsigned char*)dst.data + dst.pitch*y;

    const __m128i cr_ab = _mm_set_epi16(lab_to_rgb[1], lab_to_rgb[2], lab_to_rgb[1], lab_to_rgb[2], lab_to_rgb[1], lab_to_rgb[2], lab_to_rgb[1], lab_to_rgb[2]);
    const __m128i cg_ab = _mm_set_epi16(lab_to_rgb[4], lab_to_rgb[5], lab_to_rgb[4], lab_to_rgb[5], lab_to_rgb[4], lab_to_rgb[5], lab_to_rgb[4], lab_to_rgb[5]);
    const __m128i cb_ab = _mm_set_epi16(lab_to_rgb[7], lab_to_rgb[8], lab_to_rgb[7], lab_to_rgb[8], lab_to_rgb[7], lab_to_rgb[8], lab_to_rgb[7], lab_to_rgb[8]);
    const __m128i c_xl = _mm_set_epi16(0, lab_to_rgb[0], 0, lab_to_rgb[0], 0, lab_to_rgb[0], 0, lab_to_rgb[0]);
    const __m128i c05 = _mm_set1_epi32(0x2000);

    {for(int x=0; x<w; x++){
      __m128i abab = _mm_load_si128((__m128i*)(row));
      __m128i xlxl = _mm_load_si128((__m128i*)(row+8));
      row+=16;

      __m128i r0 = _mm_madd_epi16(abab,cr_ab);
      __m128i r1 = _mm_madd_epi16(xlxl,c_xl);
      r0 = _mm_add_epi32(r0,r1);
      r0 = _mm_add_epi32(r0,c05);
      r0 = _mm_srai_epi32(r0,14);
      __m128i g0 = _mm_madd_epi16(abab,cg_ab);
      __m128i g1 = _mm_madd_epi16(xlxl,c_xl);
      g0 = _mm_add_epi32(g0,g1);
      g0 = _mm_add_epi32(g0,c05);
      g0 = _mm_srai_epi32(g0,14);
      __m128i b0 = _mm_madd_epi16(abab,cb_ab);
      __m128i b1 = _mm_madd_epi16(xlxl,c_xl);
      b0 = _mm_add_epi32(b0,b1);
      b0 = _mm_add_epi32(b0,c05);
      b0 = _mm_srai_epi32(b0,14);

      r0 = _mm_packs_epi32(r0,r0);
      g0 = _mm_packs_epi32(g0,g0);
      b0 = _mm_packs_epi32(b0,b0);
      __m128i a0 = _mm_srli_epi32(xlxl,19);
      a0 = _mm_packs_epi32(a0,a0);

      __m128i bg = _mm_unpacklo_epi16(b0,g0);
      __m128i rx = _mm_unpacklo_epi16(r0,a0);
      bg = _mm_packus_epi16(bg,bg);
      rx = _mm_packus_epi16(rx,rx);
      __m128i bgrx = _mm_unpacklo_epi16(bg,rx);
      _mm_store_si128((__m128i*)d,bgrx);
      d+=16;
    }}
  }}
}

void LevelsFilter::render_ref()
{
  const VDXPixmap& src = *fa->src.mpPixmap;
  const VDXPixmap& dst = *fa->dst.mpPixmap;

  double gp = 0.5 / param.ci.v1;
  const int luma_map_size = 20;
  float luma[luma_map_size+1];
  {for(int i=0; i<luma_map_size+1; i++){
    double x = float(i)/luma_map_size;
    luma[i] = powf(x, gp);
  }}

  const int color_map_size = 64;
  float color[color_map_size][color_map_size];
  {for(int x=0; x<color_map_size; x++) for(int y=0; y<color_map_size; y++){
    float u = (float(x)/color_map_size-0.5)*2;
    float v = (float(y)/color_map_size-0.5)*2;

    float m = eval_color_level(u,v);
    color[x][y] = m;
  }}

  float A0 = param.d0.v0;
  float B0 = param.d1.v0;

  if(src.format==nsVDXPixmap::kPixFormat_XRGB8888){for(int y=0; y<src.h; y++){
    const unsigned char* s = (const unsigned char*)src.data + src.pitch*y;
    unsigned char* d = (unsigned char*)dst.data + dst.pitch*y;
    float scale_in = 1.0/(256*2048);
    float scale_out = 256.0/(2048);

    {for(int x=0; x<src.w; x++){
      int r = s[2];
      int g = s[1];
      int b = s[0];

      float CY = r*rgb_to_lab[0] + g*rgb_to_lab[1] + b*rgb_to_lab[2];
      float CA = r*rgb_to_lab[3] + g*rgb_to_lab[4] + b*rgb_to_lab[5];
      float CB = r*rgb_to_lab[6] + g*rgb_to_lab[7] + b*rgb_to_lab[8];
      CY = CY*scale_in;
      CA = CA*scale_in-A0*CY;
      CB = CB*scale_in-B0*CY;

      CY = (CY-param.ci.v0)/(param.ci.v2-param.ci.v0)*luma_map_size;
      if(CY<0){
        CY = 0;
      } else if(CY>luma_map_size){
        CY = 1;
      } else {
        int p = simple_ftoi(CY);
        float q = CY-p;
        CY = luma[p]*(1-q) + luma[p+1]*q;
      }

      const int cs = color_map_size/2;
      float CAM = CA*cs;
      float CBM = CB*cs;
      int px = simple_ftoi(CAM);
      float qx = CAM-px;
      int py = simple_ftoi(CBM);
      float qy = CBM-py;

      float m0 = color[px+cs][py+cs]*(1-qy) + color[px+cs][py+cs+1]*qy;
      float m1 = color[px+cs+1][py+cs]*(1-qy) + color[px+cs+1][py+cs+1]*qy;
      float m = m0*(1-qx) + m1*qx;
      //m = color[px+cs][py+cs];
      CA = CA*m;
      CB = CB*m;

      float R = CY*lab_to_rgb[0] + CA*lab_to_rgb[1] + CB*lab_to_rgb[2];
      float G = CY*lab_to_rgb[3] + CA*lab_to_rgb[4] + CB*lab_to_rgb[5];
      float B = CY*lab_to_rgb[6] + CA*lab_to_rgb[7] + CB*lab_to_rgb[8];

      R = R*scale_out+0.5;
      G = G*scale_out+0.5;
      B = B*scale_out+0.5;

      if(R<0) R=0; if(R>255) R=255;
      if(G<0) G=0; if(G>255) G=255;
      if(B<0) B=0; if(B>255) B=255;

      d[2] = R;
      d[1] = G;
      d[0] = B;
      d[3] = s[3];

      s+=4;
      d+=4;
    }}
  }}

  if(src.format==kPixFormat_XRGB64){for(int y=0; y<src.h; y++){
    const uint16* s = (const uint16*)src.data + src.pitch*y/2;
    uint16* d = (uint16*)dst.data + dst.pitch*y/2;
    FilterModPixmapInfo& src_info = *fma->fmpixmap->GetPixmapInfo(&src);
    float scale_in = 1.0/2048;
    float scale_out = 65536.0/(2048);

    {for(int x=0; x<src.w; x++){
      float r = float(s[2])/(src_info.ref_r+1);
      float g = float(s[1])/(src_info.ref_g+1);
      float b = float(s[0])/(src_info.ref_b+1);

      float CY = r*rgb_to_lab[0] + g*rgb_to_lab[1] + b*rgb_to_lab[2];
      float CA = r*rgb_to_lab[3] + g*rgb_to_lab[4] + b*rgb_to_lab[5];
      float CB = r*rgb_to_lab[6] + g*rgb_to_lab[7] + b*rgb_to_lab[8];
      CY = CY*scale_in;
      CA = CA*scale_in-A0*CY;
      CB = CB*scale_in-B0*CY;

      CY = (CY-param.ci.v0)/(param.ci.v2-param.ci.v0)*luma_map_size;
      if(CY<0){
        CY = 0;
      } else if(CY>luma_map_size){
        CY = 1;
      } else {
        int p = simple_ftoi(CY);
        float q = CY-p;
        CY = luma[p]*(1-q) + luma[p+1]*q;
      }

      const int cs = color_map_size/2;
      float CAM = CA*cs;
      float CBM = CB*cs;
      int px = simple_ftoi(CAM);
      float qx = CAM-px;
      int py = simple_ftoi(CBM);
      float qy = CBM-py;

      float m0 = color[px+cs][py+cs]*(1-qy) + color[px+cs][py+cs+1]*qy;
      float m1 = color[px+cs+1][py+cs]*(1-qy) + color[px+cs+1][py+cs+1]*qy;
      float m = m0*(1-qx) + m1*qx;
      CA = CA*m;
      CB = CB*m;

      float R = CY*lab_to_rgb[0] + CA*lab_to_rgb[1] + CB*lab_to_rgb[2];
      float G = CY*lab_to_rgb[3] + CA*lab_to_rgb[4] + CB*lab_to_rgb[5];
      float B = CY*lab_to_rgb[6] + CA*lab_to_rgb[7] + CB*lab_to_rgb[8];

      R = R*scale_out+0.5;
      G = G*scale_out+0.5;
      B = B*scale_out+0.5;

      if(R<0) R=0; if(R>65535) R=65535;
      if(G<0) G=0; if(G>65535) G=65535;
      if(B<0) B=0; if(B>65535) B=65535;

      d[2] = R;
      d[1] = G;
      d[0] = B;
      d[3] = s[3];

      s+=4;
      d+=4;
    }}
  }}
}
