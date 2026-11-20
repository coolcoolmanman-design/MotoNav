#pragma once
// Shared by both boards. Draws only into the existing 466 x 466 canvas.
// Stable brand IDs are also used by BootBrand.kt. Frame pixels stay in the caller fb;
// musicA.47: tinfl state is a static heap inflater (loopTask stack is only 8 KiB).
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <esp_system.h>              // esp_random：每次开机闪电形态随机
#include "resources/BrandLogos.h"
#include "resources/ZeldaBootBg.h"   // 塞尔达40周年特别版：整幅背景 + 旗框边点
#include "resources/Gta6BootFrames.h" // musicA.48：GTA VI 14 帧 zlib RGB565
#include "resources/ShenhuaBootFrames.h" // musicA.95：申花 1 张静态徽章帧，动效全部实时绘制（雷电版）
#include <miniz.h>

namespace BootAnimation {
static const int Size = 466;
static const uint32_t Duration = 1900;
struct Brand { const char *name; uint32_t color; };
static const Brand Brands[] = {
  {"MOTONAV",0x38E6C1}, {"HONDA",0xFF354D}, {"YAMAHA",0x5688FF},
  {"SUZUKI",0x4D9EFF}, {"KAWASAKI",0x79E52B}, {"BMW",0x63B8FF},
  {"DUCATI",0xF43C45}, {"KTM",0xFF841F}, {"TRIUMPH",0xE1E8EF},
  {"HARLEY-DAVIDSON",0xFF972F}, {"APRILIA",0xFF4148},
  {"CFMOTO",0x36D4D9}, {"QJMOTOR",0xFF424E}, {"VOGE",0x44B5FF},
  {"ZONTES",0x58D4F5}, {"BENELLI",0x55CD88}, {"ROYAL ENFIELD",0xEFBC67},
  {"ZELDA40",0xFFD86E},
  {"GTA6",0xFF2EA8},                                        // musicA.46：18 = GTA VI 开机
  {"SHENHUA",0x1E6FFF}                                      // musicA.80：19 = 申花开机
};
static const int Count = sizeof(Brands) / sizeof(Brands[0]);
static const int Zelda40Id = 17;                            // 塞尔达40周年特别版开机动画
static const int Gta6Id = 18;                               // GTA VI 14 帧开机 (~3 s)
static const int ShenhuaId = 19;                            // 申花 9 帧开机 (~2.7 s)
static const int Off = Count;                               // 20 = 关闭
inline constexpr int valid(int id) { return id >= 0 && id <= Off ? id : 0; }
inline constexpr int parse(const char *s) {
  if (!s || !*s) return -1;
  int id = 0;
  for (int i = 0; s[i]; ++i) {
    if (i >= 2 || s[i] < '0' || s[i] > '9') return -1;
    id = id * 10 + s[i] - '0';
  }
  return id <= Off ? id : -1;
}
static_assert(Count == 20 && Zelda40Id == 17 && Gta6Id == 18 && ShenhuaId == 19 && Off == 20, "Stable boot brand IDs");
static_assert(valid(-1)==0 && valid(999)==0 && valid(Off)==Off,"Invalid stored brand must fall back");
static_assert(parse("0")==0 && parse("16")==16 && parse("17")==17 && parse("18")==18 && parse("19")==ShenhuaId && parse("20")==Off,"Stable protocol IDs");
static_assert(parse("")==-1 && parse("-1")==-1 && parse("21")==-1 && parse("1x")==-1 &&
              parse("999")==-1 && parse("1,2")==-1,"Reject malformed settings without a write");
// 每品牌时长：塞尔达 / GTA VI 3.0s，申花 ~2.7s，其余 1.9s
inline uint32_t duration(int id) {
  id = valid(id);
  if (id == Zelda40Id) return ZELDA_BOOT_TOTAL_MS;
  if (id == Gta6Id) return GTA6_BOOT_TOTAL_MS;
  if (id == ShenhuaId) return SHENHUA_BOOT_TOTAL_MS;
  return Duration;
}
inline uint16_t rgb(uint32_t c, float gain=1.f) {
  int r=((c>>16)&255)*gain, g=((c>>8)&255)*gain, b=(c&255)*gain;
  return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}
inline float clamp(float n) { return n < 0 ? 0 : n > 1 ? 1 : n; }
inline float ease(float n) { n=clamp(n); return n*n*(3-2*n); }

class Painter {
  uint16_t *fb;
public:
  explicit Painter(uint16_t *p):fb(p){}
  void pixel(int x,int y,uint16_t c) {
    if ((unsigned)x < Size && (unsigned)y < Size) fb[y*Size+x]=c;
  }
  void light(int x,int y,uint16_t c) {
    if ((unsigned)x >= Size || (unsigned)y >= Size) return;
    // Preserve brighter strokes at joins; AA segment endpoints must not cut dark seams.
    uint16_t old=fb[y*Size+x];
    uint16_t r=(old&0xF800)>(c&0xF800)?old&0xF800:c&0xF800;
    uint16_t g=(old&0x07E0)>(c&0x07E0)?old&0x07E0:c&0x07E0;
    uint16_t b=(old&0x001F)>(c&0x001F)?old&0x001F:c&0x001F;
    fb[y*Size+x]=r|g|b;
  }
  void rect(int x,int y,int w,int h,uint16_t c) {
    for(int yy=y;yy<y+h;++yy) for(int xx=x;xx<x+w;++xx) pixel(xx,yy,c);
  }
  void line(float ax,float ay,float bx,float by,float width,uint32_t color,float gain=1.f) {
    float vx=bx-ax,vy=by-ay,den=vx*vx+vy*vy;
    float r=width*.5f;
    int x0=(int)floorf(fminf(ax,bx)-r-1),x1=(int)ceilf(fmaxf(ax,bx)+r+1);
    int y0=(int)floorf(fminf(ay,by)-r-1),y1=(int)ceilf(fmaxf(ay,by)+r+1);
    for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
      float t=den>0 ? clamp(((x-ax)*vx+(y-ay)*vy)/den):0;
      float dx=x-ax-t*vx,dy=y-ay-t*vy;
      float a=clamp(r+.5f-sqrtf(dx*dx+dy*dy));
      if(a>0) light(x,y,rgb(color,gain*a));
    }
  }
  void arc(float cx,float cy,float radius,float start,float end,float width,uint32_t color,float gain=1.f) {
    const float rad=.01745329252f;
    for(float a=start;a<end;a+=3) {
      float b=fminf(a+3,end);
      line(cx+cosf(a*rad)*radius,cy+sinf(a*rad)*radius,
           cx+cosf(b*rad)*radius,cy+sinf(b*rad)*radius,width,color,gain);
    }
  }
  // 5 x 7 display lettering. Uppercase A-Z, space and hyphen only.
  void text(const char *s,int y,int scale,uint32_t color,float gain=1.f,int spacing=1) {
    static const uint8_t font[26][5]={
      {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
      {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
      {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
      {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
      {0x41,0x41,0x7F,0x41,0x41},{0x20,0x40,0x41,0x3F,0x01},
      {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
      {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
      {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
      {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
      {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
      {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
      {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
    };
    int n=strlen(s),advance=5*scale+spacing;
    int x=(Size-(n*advance-spacing))/2;
    for(int i=0;i<n;++i,x+=advance) {
      for(int col=0;col<5;++col) {
        uint8_t bits=s[i]>='A'&&s[i]<='Z'?font[s[i]-'A'][col]:s[i]=='-'?8:0;
        for(int row=0;row<7;++row) if(bits&(1<<row))
          rect(x+col*scale,y+row*scale,scale,scale,rgb(color,gain));
      }
    }
  }
};


inline void drawBadge(Painter &p,int id,float gain,uint32_t ms) {
  if(id > 0 && id <= BrandLogos::Count) {
    const BrandLogos::Logo &logo=BrandLogos::Logos[id-1];
    const int x0=(Size-logo.w)/2, y0=184-logo.h/2;
    uint32_t position=0;
    for(unsigned i=0;i<logo.pairs;++i) {
      uint16_t count=logo.data[i*2], color=logo.data[i*2+1];
      uint16_t faded=((int)(((color>>11)&31)*gain)<<11) |
                     ((int)(((color>>5)&63)*gain)<<5) | (int)((color&31)*gain);
      if(faded) {
        for(unsigned j=0;j<count;++j) {
          uint32_t offset=position+j;
          p.pixel(x0+offset%logo.w,y0+offset/logo.w,faded);
        }
      }
      position+=count;
    }
    return;
  }
  uint32_t color=Brands[0].color;
  // Keep the original default MotoNav motorcycle animation.
  p.arc(168,202,28,0,360,3,0xD7E4ED,gain);
  p.arc(300,202,28,0,360,3,0xD7E4ED,gain);
  p.arc(168,202,19,ms*.15f,ms*.15f+220,1,color,gain*.8f);
  p.arc(300,202,19,ms*.15f,ms*.15f+220,1,color,gain*.8f);
  p.line(168,202,205,157,3,color,gain);
  p.line(205,157,238,202,3,color,gain);
  p.line(168,202,238,202,3,color,gain);
  p.line(238,202,274,158,3,color,gain);
  p.line(274,158,300,202,4,0xEDF6FF,gain);
  p.line(274,158,264,139,3,0xEDF6FF,gain);
  p.line(264,139,248,139,3,0xEDF6FF,gain);
  p.line(202,155,227,147,5,0xEDF6FF,gain);
  p.line(227,147,254,155,5,0xEDF6FF,gain);
  p.line(179,151,202,155,5,0xEDF6FF,gain);
  p.line(177,151,164,141,3,color,gain);
  p.line(210,172,237,172,4,0xEDF6FF,gain);
  p.line(214,180,235,180,3,0xEDF6FF,gain);
  p.line(218,188,232,188,3,0xEDF6FF,gain);
  p.line(285,153,295,153,4,color,gain);
}

// ================= 塞尔达40周年特别版开机动画（id 17） =================
// 效果与 motonav-effect/boot_anim3.py 定稿一致：
//   0~3.8s   亮度波：以"40"字样中心为原点，整幅从暗到亮向外扩散（150px 软过渡，无硬光圈边）
//   3.8~6.8s 旗框滚动流光：白热光头带金色拖尾沿旗框巡游（角度参数化，与效果图同参数）
inline uint16_t zeldaScale(uint16_t c, uint32_t lq) {        // lq = 0..256 亮度
  uint32_t r = ((c >> 11) & 0x1F) * lq >> 8;
  uint32_t g = ((c >> 5) & 0x3F) * lq >> 8;
  uint32_t b = (c & 0x1F) * lq >> 8;
  return (uint16_t)((r << 11) | (g << 5) | b);
}
inline uint16_t zeldaAdd(uint16_t dst, uint32_t add) {       // 加法混色并饱和
  uint32_t r = ((dst >> 11) & 0x1F) + ((add >> 11) & 0x1F);
  uint32_t g = ((dst >> 5) & 0x3F) + ((add >> 5) & 0x3F);
  uint32_t b = (dst & 0x1F) + (add & 0x1F);
  if (r > 31) r = 31; if (g > 63) g = 63; if (b > 31) b = 31;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

inline void renderZelda(uint16_t *fb, uint32_t ms) {
  static uint32_t dx2[Size];                                 // (x-cx)^2 表
  static bool dxInit = false;
  if (!dxInit) {
    for (int x = 0; x < Size; ++x) {
      float d = x - ZELDA_BOOT_CX; dx2[x] = (uint32_t)(d * d);
    }
    dxInit = true;
  }
  // zelda40.4：等价提速（画面像素级不变）。原实现每帧对 21.7 万像素做 sqrtf、对 3288 个
  // 边框点做 atan2f，是帧率的主要瓶颈；并在淡入结束后仍每帧重算整屏背景。现改为开机首次
  // 调用时预建查表 / 缓存，之后每帧只做查表与加法。PSRAM 分配失败时自动回退到原算法。
  // 1) 距离查表：di = dx2[x] + (y-cy)^2，value = sqrt(di)*8（半精度足够，误差 < 1/8 px）
  static uint16_t *distLUT = nullptr; static uint32_t distMax = 0; static bool distInit = false;
  if (!distInit) {
    distMax = 0;
    for (int x = 0; x < Size; ++x) {
      uint32_t dx = dx2[x];
      for (int y = 0; y < Size; ++y) { float dy = y - ZELDA_BOOT_CY; uint32_t di = dx + (uint32_t)(dy * dy); if (di > distMax) distMax = di; }
    }
    distLUT = (uint16_t *)heap_caps_malloc((distMax + 1) * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (distLUT) for (uint32_t i = 0; i <= distMax; ++i) distLUT[i] = (uint16_t)(sqrtf((float)i) * 8.0f);
    distInit = true;                                          // 分配失败 → 回退逐像素 sqrtf
  }
  // 2) 边框点角度查表（atan2f 只需算一次）
  static float bordAng[ZELDA_BORDER_N]; static bool baInit = false;
  if (!baInit) {
    for (int i = 0; i < ZELDA_BORDER_N; ++i)
      bordAng[i] = atan2f(ZeldaBootBorderY[i] - ZELDA_BORDER_CY, ZeldaBootBorderX[i] - ZELDA_BORDER_CX);
    baInit = true;
  }
  // 3) 淡入完成后的“全亮背景”缓存：流光阶段直接 memcpy，不再每帧重算整屏
  static uint16_t *fadedBg = nullptr; static bool fadedReady = false;
  if (!fadedBg) fadedBg = (uint16_t *)heap_caps_malloc((uint32_t)Size * Size * sizeof(uint16_t), MALLOC_CAP_SPIRAM);

  const float BAND = ZELDA_BOOT_BAND;
  float r;
  bool full;
  if (ms >= ZELDA_BOOT_FADE_MS) { r = 1e9f; full = true; }
  else {
    float p = clamp(ms / (float)ZELDA_BOOT_FADE_MS);
    r = ease(p) * (ZELDA_BOOT_RMAX + BAND);                  // 波前半径（含整个过渡带）
    full = false;
  }
  if (full) {
    // 背景已全亮：首次构建缓存，之后每帧直接拷贝，再叠加流光
    if (fadedBg && !fadedReady) {
      for (uint32_t i = 0; i < (uint32_t)Size * Size; ++i) fadedBg[i] = zeldaScale(ZeldaBootBg[i], 256);
      fadedReady = true;
    }
    if (fadedBg) memcpy(fb, fadedBg, (uint32_t)Size * Size * sizeof(uint16_t));
    else for (int y = 0; y < Size; ++y) { uint16_t *row = fb + y * Size; const uint16_t *bg = ZeldaBootBg + y * Size; for (int x = 0; x < Size; ++x) row[x] = zeldaScale(bg[x], 256); }
  } else {
    for (int y = 0; y < Size; ++y) {
      float dy = y - ZELDA_BOOT_CY;
      float dy2 = dy * dy;
      uint16_t *row = fb + y * Size;
      const uint16_t *bg = ZeldaBootBg + y * Size;
      for (int x = 0; x < Size; ++x) {
        float d = distLUT ? (distLUT[dx2[x] + (uint32_t)dy2] * (1.0f / 8.0f)) : sqrtf((float)dx2[x] + dy2);
        float l = clamp((r - d) / BAND);
        l = l * l * (3 - 2 * l);                             // smoothstep 软过渡
        row[x] = zeldaScale(bg[x], (uint32_t)(l * 256.0f));
      }
    }
  }
  if (!full) return;
  // ---- 旗框滚动流光 ----
  float t01 = (ms - ZELDA_BOOT_FADE_MS) / (float)(ZELDA_BOOT_TOTAL_MS - ZELDA_BOOT_FADE_MS);
  float p = clamp(t01 / 1.15f);
  float rise = fminf(1.0f, 0.25f + p * 3.0f);
  const float TAU = ZELDA_BORDER_TAU;
  float head = fmodf(t01, 1.0f) * 6.2831853f;
  const uint32_t HOT = 0xFFF4C8;                             // 白热 (255,244,200)
  float best = 1e9f; int bxi = 0, byi = 0;
  for (int i = 0; i < ZELDA_BORDER_N; ++i) {
    float ang = bordAng[i];
    float d = fmodf(ang - head + 9.4247779f, 6.2831853f);    // 光头之后的角距
    // 光头亮核吸附：角度最近的轮廓点
    float da = fabsf(d < 3.1415927f ? d : d - 6.2831853f);
    if (da < best) { best = da; bxi = ZeldaBootBorderX[i]; byi = ZeldaBootBorderY[i]; }
    if (d >= TAU) continue;
    float w = powf(1.0f - d / TAU, 1.15f);
    if (w < 0.02f) continue;
    uint16_t add = rgb(HOT, w * rise);                       // rgb() 输出 RGB565
    int x = ZeldaBootBorderX[i], y = ZeldaBootBorderY[i];
    fb[y * Size + x] = zeldaAdd(fb[y * Size + x], add);
  }
  // 光头亮核：高斯光斑
  const float spotR = 15.0f;
  int x0 = bxi - 45, x1 = bxi + 45, y0 = byi - 45, y1 = byi + 45;
  if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > Size) x1 = Size; if (y1 > Size) y1 = Size;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      float dx = x - bxi, dy = y - byi;
      float g = expf(-(dx * dx + dy * dy) / (2 * spotR * spotR));
      uint16_t add = rgb(HOT, g * rise * 0.85f);
      if (add) fb[y * Size + x] = zeldaAdd(fb[y * Size + x], add);
    }
  }
}

// musicA.48：GTA VI 开机 — 14 帧 zlib（~214 ms/帧，总 3 s；同 A.46 相位，更密）
// musicA.47：tinfl_decompressor ≈11 KiB；不可放 loopTask 栈（默认 8 KiB）→ 与 RasterDecoder 同走 heap。
inline bool inflateZlibRgb565(const uint8_t *z, size_t zlen, uint16_t *dst, size_t need) {
  if (!z || !dst || !zlen || !need) return false;
  tinfl_decompressor *decomp = motonavSharedInflater();   // musicA.114 共享解压器
  if (!decomp) return false;
  tinfl_init(decomp);
  size_t inBytes = zlen, outBytes = need;
  tinfl_status st = tinfl_decompress(decomp, z, &inBytes,
      (uint8_t *)dst, (uint8_t *)dst, &outBytes,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  return st == TINFL_STATUS_DONE && outBytes == need;
}

inline void renderGta6(uint16_t *fb, uint32_t ms) {
  int fi = (int)(ms / GTA6_BOOT_FRAME_MS);
  if (fi < 0) fi = 0;
  if (fi >= GTA6_BOOT_COUNT) fi = GTA6_BOOT_COUNT - 1;
  if (!inflateZlibRgb565(GTA6_BOOT_ZDATA + GTA6_BOOT_ZOFF[fi], GTA6_BOOT_ZLEN[fi],
                         fb, (size_t)Size * Size * 2)) {
    memset(fb, 0, (size_t)Size * Size * 2);
  }
}

// musicA.95：申花开机 — 雷电版实时绘制（v7）。资源只有 1 张静态徽章帧(px3/40c)，
// 其余全部代码绘制：徽章 1s 从纯黑淡入(轻微缩放沉降)；14 轮斜劈闪电(锯齿主干+分叉+
// 闪烁，从画布边缘劈到盾徽轮廓)；周缘爬行电弧(66ms 重生成)；主雷间隙背景微闪电填充；
// 命中火花+辉光；大雷轮次整屏加法泛光。每次开机形态随机(esp_random 种子)。总 3.2s。
inline uint32_t shRng(uint32_t &st) { st = st*1664525u + 1013904223u; return st; }
inline float shRnd(uint32_t &st) { return (shRng(st) >> 8) * (1.f/16777216.f); }

// 中点位移法锯齿折线（闪电主干）。depth 5 → 33 点，depth 3 → 9 点。
inline void shJaggy(float *xs, float *ys, int &n, float ax, float ay,
                    float bx, float by, uint32_t &st, float rough, int depth) {
  xs[0]=ax; ys[0]=ay; xs[1]=bx; ys[1]=by; n=2;
  float len=sqrtf((bx-ax)*(bx-ax)+(by-ay)*(by-ay));
  for (int d=0; d<depth; ++d) {
    float amp=powf(rough,(float)d)*0.5f*len;
    for (int i=n-1; i>0; --i) {
      float mx=(xs[i-1]+xs[i])*0.5f, my=(ys[i-1]+ys[i])*0.5f;
      float dx=ys[i]-ys[i-1], dy=xs[i-1]-xs[i];
      float L=sqrtf(dx*dx+dy*dy); if (L<1e-3f) L=1;
      float off=(shRnd(st)-0.5f)*2.f*amp;
      for (int j=n; j>i; --j) { xs[j]=xs[j-1]; ys[j]=ys[j-1]; }
      xs[i]=mx+dx/L*off; ys[i]=my+dy/L*off; ++n;
    }
  }
}

// 闪电：辉光/中亮/白核三通道 + 1-2 根短分叉
inline void shBolt(Painter &p, float ax, float ay, float bx, float by,
                   uint32_t &st, bool main) {
  float xs[34], ys[34]; int n;
  shJaggy(xs,ys,n,ax,ay,bx,by,st, main?0.20f:0.30f, 5);
  if (main) {
    for (int i=0;i+1<n;++i) p.line(xs[i],ys[i],xs[i+1],ys[i+1],8,0x5A8CFF,0.30f);
    for (int i=0;i+1<n;++i) p.line(xs[i],ys[i],xs[i+1],ys[i+1],4,0x96C3FF,0.55f);
    for (int i=0;i+1<n;++i) p.line(xs[i],ys[i],xs[i+1],ys[i+1],3,0xEBF5FF,0.92f);
  } else {
    for (int i=0;i+1<n;++i) p.line(xs[i],ys[i],xs[i+1],ys[i+1],2,0x96C3FF,0.45f);
    for (int i=0;i+1<n;++i) p.line(xs[i],ys[i],xs[i+1],ys[i+1],1,0xEBF5FF,0.65f);
  }
  int nf=main?2:1;
  for (int k=0;k<nf;++k) {
    int i=2+(int)(shRng(st)% (uint32_t)(n-4));
    float ang=shRnd(st)*6.2831853f, ln=14.f+shRnd(st)*20.f;
    float fx=xs[i]+cosf(ang)*ln, fy=ys[i]+sinf(ang)*ln;
    float fxs[8], fys[8]; int fn;
    shJaggy(fxs,fys,fn,xs[i],ys[i],fx,fy,st,0.45f,3);
    for (int j=0;j+1<fn;++j) p.line(fxs[j],fys[j],fxs[j+1],fys[j+1],1,0x96C3FF,0.42f);
  }
}

// 命中点辉光（加法小光斑）
inline void shHitGlow(uint16_t *fb, float hx, float hy, float pw) {
  int x0=(int)hx-6, x1=(int)hx+7, y0=(int)hy-6, y1=(int)hy+7;
  if (x0<0)x0=0; if (y0<0)y0=0; if (x1>Size)x1=Size; if (y1>Size)y1=Size;
  for (int y=y0;y<y1;++y) for (int x=x0;x<x1;++x) {
    float dx=x-hx, dy=y-hy;
    float g=expf(-(dx*dx+dy*dy)/18.f)*pw;
    if (g<0.03f) continue;
    uint32_t col=((uint32_t)200<<16)|((uint32_t)230<<8)|255;
    fb[y*Size+x]=zeldaAdd(fb[y*Size+x], rgb(col, g));
  }
}

inline void renderShenhua(uint16_t *fb, uint32_t ms) {
  static_assert(SHENHUA_BOOT_W == Size && SHENHUA_BOOT_H == Size, "Shenhua boot must be 466x466");
  static uint16_t *badge = nullptr;               // 缓存全亮静态帧
  if (!badge) {
    badge = (uint16_t *)heap_caps_malloc((size_t)Size * Size * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (badge && !inflateZlibRgb565(SHENHUA_BOOT_ZDATA, SHENHUA_BOOT_ZLEN[0],
                                    badge, (size_t)Size * Size * 2)) {
      free(badge); badge = nullptr;
    }
  }
  static uint32_t seed = 0; static bool seeded = false;
  if (!seeded) { seed = (uint32_t)esp_random(); seeded = true; }
  const uint32_t FADE0 = 3600;
  float outg = 1.f - ease((ms - FADE0) / float(SHENHUA_BOOT_TOTAL_MS - FADE0));
  if (outg <= 0.f) { memset(fb, 0, (size_t)Size * Size * sizeof(uint16_t)); return; }

  // Badge: full brightness from t=0 (no fade-in); only a slight settle over the first 500ms.
  float settle = 0.80f + 0.20f * ease(clamp(ms / 500.f));
  int lq = (int)(outg * 256.f); if (lq > 256) lq = 256;
  if (lq > 0) {
    if (badge) {
      if (settle >= 0.999f) {
        for (uint32_t i = 0; i < (uint32_t)Size * Size; ++i) fb[i] = zeldaScale(badge[i], (uint32_t)lq);
      } else {
        // 缩放沉降：中心采样最近邻
        int cxw = (int)(Size * settle);
        for (int y = 0; y < Size; ++y) {
          int sy = (int)((y - Size/2) / settle) + Size/2;
          if ((unsigned)sy >= (unsigned)Size) continue;
          const uint16_t *src = badge + sy * Size;
          uint16_t *row = fb + y * Size;
          for (int x = 0; x < Size; ++x) {
            int sx = (int)((x - Size/2) / settle) + Size/2;
            if ((unsigned)sx < (unsigned)Size) row[x] = zeldaScale(src[sx], (uint32_t)lq);
          }
        }
      }
    } else memset(fb, 0, (size_t)Size * Size * sizeof(uint16_t));
  } else memset(fb, 0, (size_t)Size * Size * sizeof(uint16_t));

  // 盾徽轮廓查表：96 个角度，从中心向外走亮度阈值(背景 9,11,22 亮度≈13，盾徽银边≈100+)
  static int16_t rimX[96], rimY[96]; static bool rimInit = false;
  if (!rimInit) {
    for (int i = 0; i < 96; ++i) {
      float a = i * 6.2831853f / 96.f, ca = cosf(a), sa = sinf(a);
      int last = -1;
      for (int r = 30; r < 233; ++r) {
        int x = 233 + (int)(ca * r), y = 233 + (int)(sa * r);
        if ((unsigned)x >= (unsigned)Size || (unsigned)y >= (unsigned)Size) break;
        uint16_t c = badge ? badge[y * Size + x] : 0;
        int R = ((c >> 11) & 31) * 255 / 31, G = ((c >> 5) & 63) * 255 / 63, B = (c & 31) * 255 / 31;
        float lum = 0.299f * R + 0.587f * G + 0.114f * B;
        if (lum > 30.f) last = r;
        else if (last >= 0 && r > last + 3) break;
      }
      if (last < 0) last = 168;
      rimX[i] = (int16_t)(233 + ca * last); rimY[i] = (int16_t)(233 + sa * last);
    }
    rimInit = true;
  }
  auto rimHit = [&](float ang, float &hx, float &hy) {
    if (ang < 0) ang += 6.2831853f; if (ang >= 6.2831853f) ang -= 6.2831853f;
    float f = ang * 96.f / 6.2831853f;
    int i0 = (int)f % 96, i1 = (i0 + 1) % 96;
    float fr = f - (int)f;
    hx = rimX[i0] + (rimX[i1] - rimX[i0]) * fr;
    hy = rimY[i0] + (rimY[i1] - rimY[i0]) * fr;
  };
  auto edgePt = [&](float ang, float &ex, float &ey) {
    float ca = cosf(ang), sa = sinf(ang), t = 1e9f;
    if (ca > 1e-6f) t = fminf(t, (Size - 1 - 233) / ca);
    if (ca < -1e-6f) t = fminf(t, (0 - 233) / ca);
    if (sa > 1e-6f) t = fminf(t, (Size - 1 - 233) / sa);
    if (sa < -1e-6f) t = fminf(t, (0 - 233) / sa);
    ex = 233 + ca * t; ey = 233 + sa * t;
  };

  Painter p(fb);

  // 主雷 14 轮：(起始ms, 道数, 大雷)
  static const uint16_t STR[14][3] = {
    {200,1,0},{400,1,0},{630,2,0},{830,1,0},{1030,2,0},{1250,3,1},{1450,1,0},
    {1650,2,0},{1850,1,0},{2050,2,0},{2280,3,1},{2480,1,0},{2670,2,0},{2840,1,0}};
  float flash = 0.f;
  for (int si = 0; si < 14; ++si) {
    uint32_t s0 = STR[si][0];
    if (ms < s0 || ms >= s0 + 100) continue;
    int nb = STR[si][1]; bool big = STR[si][2] != 0;
    uint32_t dt = ms - s0;
    int ph = (int)(dt / 33); if (ph > 2) ph = 2;
    bool on = (ph == 0 || ph == 2) || (big && ph == 1);   // 闪烁：亮-灭-亮，大雷少灭一次
    if (on) {
      uint32_t st = seed ^ (s0 * 2654435761u) ^ ((uint32_t)ph * 97u);
      float base = shRnd(st) * 6.2831853f;
      for (int k = 0; k < nb; ++k) {
        float aHit = base + k * 6.2831853f / nb + (shRnd(st) - 0.5f) * 0.9f;
        float a0 = aHit + (shRnd(st) - 0.5f) * 2.2f;      // 入射方向偏离 → 斜劈长闪电
        float ex, ey, hx, hy;
        edgePt(a0, ex, ey); rimHit(aHit, hx, hy);
        shBolt(p, ex, ey, hx, hy, st, k == 0 || big);
        if (ph < 2) shHitGlow(fb, hx, hy, 0.55f);
      }
    }
    float f = dt < 35 ? (big ? 0.62f : 0.38f) : (big ? 0.26f : 0.14f);
    if (f > flash) flash = f;
    // 命中火花（雷后 100ms 内）
    if (dt >= 33 && dt < 133) {
      float life = (dt - 33) / 100.f;
      uint32_t st = seed ^ (s0 * 31u);
      for (int k = 0; k < nb; ++k) {
        float aHit = shRnd(st) * 6.2831853f;
        float hx, hy; rimHit(aHit, hx, hy);
        shHitGlow(fb, hx, hy, 0.35f * (1 - life));
        for (int j = 0; j < 12; ++j) {
          float a2 = shRnd(st) * 6.2831853f;
          float r0 = (4 + shRnd(st) * 22) * (0.4f + life);
          float px = hx + cosf(a2) * r0, py = hy + sinf(a2) * r0;
          float al = (1 - life) * (0.4f + shRnd(st) * 0.6f) * 0.8f;
          if (al < 0.05f) continue;
          uint32_t col = ((uint32_t)190 << 16) | ((uint32_t)225 << 8) | 255;
          p.rect((int)px, (int)py, 2, 2, rgb(col, al));
        }
      }
    }
  }

  // 周缘爬行电弧（每 66ms 重生成，4 段贴轮廓折线）
  if (ms > 330) {
    uint32_t slot = ms / 66;
    uint32_t st = seed ^ (slot * 799u);
    for (int k = 0; k < 4; ++k) {
      float a0 = shRnd(st) * 6.2831853f, span = 0.35f + shRnd(st) * 0.45f;
      float ax[8], ay[8];
      for (int j = 0; j < 7; ++j) {
        float a = a0 + span * j / 6.f;
        float rr = 1.f + (shRnd(st) - 0.5f) * 0.10f;
        float hx, hy; rimHit(a, hx, hy);
        ax[j] = 233 + (hx - 233) * rr; ay[j] = 233 + (hy - 233) * rr;
      }
      for (int j = 0; j + 1 < 7; ++j) {
        p.line(ax[j], ay[j], ax[j+1], ay[j+1], 2, 0x96C3FF, 0.45f);
        p.line(ax[j], ay[j], ax[j+1], ay[j+1], 1, 0xEBF5FF, 0.62f);
      }
    }
    // 主雷间隙背景微闪电（每 33ms 槽 55% 概率 1-2 道，细且暗）
    uint32_t mslot = ms / 33;
    uint32_t mst = seed ^ (mslot * 761u);
    for (int k = 0; k < 2; ++k) {
      if (shRnd(mst) < 0.55f) {
        float aHit = shRnd(mst) * 6.2831853f;
        float a0 = aHit + (shRnd(mst) - 0.5f) * 1.8f;
        float ex, ey, hx, hy;
        edgePt(a0, ex, ey); rimHit(aHit, hx, hy);
        shBolt(p, ex, ey, hx, hy, mst, false);
      }
    }
  }

  // 大雷整屏加法泛光（偏蓝，非灰白）
  if (flash > 0.f) {
    uint16_t add = rgb(0x4664D2, flash);
    for (uint32_t i = 0; i < (uint32_t)Size * Size; ++i) fb[i] = zeldaAdd(fb[i], add);
  }
}

inline void render(uint16_t *fb,uint32_t ms,int id) {
  if(!fb) return;
  id=valid(id);
  if(id==Off) { memset(fb,0,Size*Size*sizeof(uint16_t)); return; }
  if(id==Zelda40Id) { memset(fb,0,Size*Size*sizeof(uint16_t)); renderZelda(fb, ms); return; }
  if(id==Gta6Id) { renderGta6(fb, ms); return; }
  if(id==ShenhuaId) { renderShenhua(fb, ms); return; }
  memset(fb,0,Size*Size*sizeof(uint16_t));
  Painter p(fb);
  uint32_t color=Brands[id].color;
  float intro=ease(ms/420.f), fade=1-ease((ms-1670.f)/230.f);
  float gain=intro*fade;
  float progress=ease(ms/1300.f);
  // Instrument bezel, moving sweep and sparse index marks.
  p.arc(233,233,211,-90,270,1,0x32414E,gain*.7f);
  p.arc(233,233,211,-90,-90+360*progress,3,color,gain*.85f);
  float head=-90+360*progress;
  p.arc(233,233,203,head-22,head,2,0xF3FAFF,gain*.8f);
  for(int a=0;a<360;a+=15) {
    float r=a*.01745329252f;
    float inner=(a%45==0)?191:196;
    p.line(233+cosf(r)*inner,233+sinf(r)*inner,
           233+cosf(r)*199,233+sinf(r)*199,1,0x667583,gain*.65f);
  }
  p.text("MOTONAV",89,1,0x92A4B4,gain,4);
  float bike=ease((ms-120.f)/500.f)*fade;
  drawBadge(p,id,bike,ms);
  float title=ease((ms-320.f)/500.f)*fade;
  int scale=strlen(Brands[id].name)>12?4:strlen(Brands[id].name)>8?5:6;
  p.text(Brands[id].name,263+(int)((1-title)*9),scale,0xEFF7FC,title,scale);
  p.line(205,324,261,324,2,color,title);
  p.text("RIDE YOUR WAY",348,1,0x8D9BA7,title,3);
}
} // namespace BootAnimation
