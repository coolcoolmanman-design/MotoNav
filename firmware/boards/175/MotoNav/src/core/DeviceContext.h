#pragma once
// 依赖、板级对象、布局常量和共享运行状态。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
#include "Arduino_GFX_Library.h"
#include "../../pin_config.h"
#include <Wire.h>
#include "TouchDrvCSTXXX.hpp"     // v5.2 触摸屏（SensorLib 库，微雪示例里带的）
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/rtc_io.h>
#include "../../IdlePowerOff.h"
#include <driver/spi_master.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include "../../HidMediaKeys.h"
#include "../../AppleMediaClient.h"
#include "../../ScreenRotationCommand.h"
#include <BLESecurity.h>       // v6.5 配对验证
#include <miniz.h>            // 罪恶都市轮廓：仅接收地图时解压
#include "../../BootAnimation.h"
#include "../../DragonRadar.h"
int bootBrand = 0;
String pendingBootAck;
int faceId = 0;                 // v7.12.10 待机页可换表盘：当前表盘编号（0 = 默认）
String pendingFaceAck;
bool faceDirty = false;         // 表盘切换后请求待机页整页重绘
// v7.12.10-musicA.8 二期C：表盘「背景 + 组件槽」——App 经 #FC 下发槽配置，秒切
// 颜色 RGB565（0 = 自动默认）；slotMask：bit0 时间 bit1 电量 bit2 连接态 bit3 中央动画（musicA.17 语义）
uint16_t slotTimeColor = 0, slotBattColor = 0, slotConnColor = 0, slotTpmsColor = 0;
uint8_t  slotMask = 0x0F;       // 默认全开；musicA.40 起 bit1 电量可由 App 开关（不再强制）
// musicA.17 六槽表盘：左上/右上/下方三槽功能可切（0 速度 1 G值 2 胎压 3 天气）；
// 时间槽/中央动画槽/连接态槽固定但可显隐调色；中央动画载具 0 塞尔达 1 像素摩托 2 自行车(隐藏) 3 GTA VI 4 组合
uint8_t  faceSlotFunc[3] = {0, 2, 3};   // 出厂默认：速度 / 胎压 / 天气
uint16_t slotTLColor = 0, slotTRColor = 0, slotBTColor = 0, slotVehColor = 0;
uint8_t  vehId = 0;
// musicA.55：中央组合精灵已烧进固件（16 组合 × 2 态），App 只发 #CS,<g>,<v> 索引，
// 开机即有、切换零 BLE bulk。comboUseBuiltin=false 时回退旧 CSP 自定义通路。
uint8_t  comboGender = 0;        // 0 = 男生 m, 1 = 女生 f
uint8_t  comboVehicle = 3;       // 0..7：sedan suv supercar motorcycle scooter sportbike bicycle foldingBike
bool     comboUseBuiltin = true;
// musicA.17/46 自定义开机背景（bulk type 8，'BGR' zlib RGB565 466×466）：
// musicA.46 起成功收包后写 ffat；开机装载后发 BGR,cached,1，App 仅变更时再推。
#define BGR_HDR 8
#define BGR_MAX (BGR_HDR + 466 * 466 * 2 + 64)
uint16_t *bgCustom = nullptr;   // PSRAM：最近一次收到的自定义背景
bool bgCustomOn = false;        // true = homeBg 切到 bgCustom（zbgRect 生效）
bool bgClearReq = false;        // #BGD 请求：主循环里清掉并恢复 faceId 背景
bool mediaFlashLoading = false; // 开机从 ffat 装载时跳过回写
bool bgFlashCached = false;     // 开机已从 Flash 装回壁纸
String pendingFaceCfgAck;
String pendingVehAck;
String pendingBgCfgAck;
String pendingBgCachedAck;      // musicA.46：BGR,cached,1
String pendingBadgeCachedAck;   // musicA.46：BDG,cached,<mask>
String pendingCoverSpinAck;               // v7.12.10 音乐封面旋转开关回执 RC,<0/1>
String pendingLpAck;                      // musicA.50：LP / LPA / PBO 回执
String pendingCspCachedAck;               // musicA.50：CSP,cached,1
inline uint16_t slotColor(uint16_t def, uint16_t user) { return user ? user : def; }   // 0 = 自动默认（各页共用，StatusWidgets 早于 HomePage 包含）

// ===== musicA.50/52 中央槽组合精灵（bulk type 11，'CSP'；App 预制 push）=====
// ver=2：'CSP' 2 role frameCount w_le16 h_le16 + 每帧 u32 zlen + zlib(RGB565 466²)；chroma 0。
// role 1=sleep(/center/sleep.csp) 2=chase(/center/chase.csp)；w=h=0 清除该 role。
#define CSP_HDR 10
#define CSP_MAX_FRAMES 12
#define CSP_MAX (256 * 1024)
#define CSP_ROLE_SLEEP 1
#define CSP_ROLE_CHASE 2
struct CspPack {
  uint8_t *wire = nullptr;
  uint32_t wireLen = 0;
  uint8_t frameCount = 0;
  uint32_t zoff[CSP_MAX_FRAMES];
  uint32_t zlen[CSP_MAX_FRAMES];
  bool ok = false;
};
CspPack cspSleep;
CspPack cspChase;
bool cspFlashCached = false;              // 至少一套 sleep/chase 已从 ffat 装回
inline bool cspReady() { return cspSleep.ok || cspChase.ok; }

// ===== musicA.50 电量管理 / 省电 =====
bool lpModeOn = false;                    // 省电模式（冻帧 + 降亮 + 关 dynBg）
bool lpAutoOn = true;                     // ≤10% 自动开
bool pwrBatOffAllow = false;              // 电池供电允许短按息屏（默认关）
int  lpBrightSaved = -1;                  // 进入省电前亮度；-1=未记账
#define LP_BRIGHT_CAP 80
#define LP_AUTO_THR 10

// ===== musicA.37 六槽改造 + 预设动态背景（2026-11-06 正式功能） =====
// 上方槽 / 中央槽 / 时间样式由 App 的 #FC3 下发；旧 #FC2 一字不改（老固件忽略新命令即兼容）。
uint8_t  topFunc    = 0;      // 上方槽：0 时间 1..4 兼容旧速度/G/胎压/天气 5 隐藏（musicA.40 App 只发 0/5）
uint8_t  centerFunc = 0;      // 中央槽：0 动画 1 时间 2 全屏指针表盘 3 隐藏（musicA.40）
uint8_t  clockStyle = 0;      // 时间样式 0..8（5..8 同时是全屏指针的四种）
uint8_t  fullFaceAB = 0;      // 全屏指针样式：0 A 纯净（默认） 1 B 近中心黑圆外环 2 C Infograph 四弧
uint8_t  dynBgStyle = 0;      // 动态背景：0 现有静态 1 呼吸金纹 2 流动极光 3 星尘粒子 4 斜向光扫 5 同心涟漪 6 像素夜景
bool     dynBgOn    = false;   // 总开关（开机等 App 同步，避免刷机后未连 App 就改变待机观感）
uint8_t  dynBgSpeed = 18;     // 漂移倍率 ×10（默认 18 = ×1.8，只缩放时间不改幅度）
bool     dynBgAuto  = false;   // musicA.40：废弃昼夜自动；保留字段兼容 NVS/协议，渲染始终夜色
uint16_t *dynBgBuf  = nullptr; // PSRAM：466×466 当前帧背景（zbgRect 从它恢复）
// 上传自定义壁纸 > 动态背景：上传成功时暂停动态帧但保留已选风格，#BGD 恢复默认后自动续上。
// musicA.50：省电模式强制关动态背景帧。
inline bool dynBgActive() { return dynBgOn && dynBgStyle > 0 && !bgCustomOn && !lpModeOn; }
// 秒针刻度 / 红点分离规则（用户拍板）：hasTimeFace 管 60 格刻度，dotOn 严格只管每秒红点。
// musicA.41：全屏指针删除最外圈秒刻度，不再把 centerFunc==2 算进 hasTimeFace。
inline bool hasTimeFace() { return topFunc == 0 || centerFunc == 1; }
inline bool dotOn()       { return (slotMask & 0x01) != 0; }   // bit0 由 App 的 top==0 推导
#include <esp_mac.h>           // v7.0 读蓝牙地址，拼蓝牙名
#if defined(CONFIG_NIMBLE_ENABLED)
#include "host/ble_gap.h"     // v4.5 蓝牙 2M 模式
#include "host/ble_store.h"   // v6.5 记住的手机
#endif

// ===== 屏幕（和官方 HelloWorld 示例一样的初始化方式）=====
// v2.6 第 7 个参数 true = 总线可共享（快速刷屏要和它轮流使用数据线）
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3, true);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
// 先画在内存里的“画布”上，画完再一次性推到屏幕（需要 PSRAM：OPI PSRAM）
// v2.2：可以切换画布内存的画布，用来做双缓冲
class SwapCanvas : public Arduino_Canvas {
public:
  SwapCanvas(int16_t w, int16_t h, Arduino_G *out) : Arduino_Canvas(w, h, out) {}
  void setFramebuffer(uint16_t *f) { _framebuffer = f; }
};
SwapCanvas *gfx = new SwapCanvas(LCD_WIDTH, LCD_HEIGHT, panel);
bool needFlush = false;
bool frameIsMap = false;          // v2.2 这一帧是小地图（整帧重画，可以异步刷屏）
int qspiMHz = 40;                 // v2.3 屏幕数据线速度
bool fastFlushOn = true;          // v2.6 快速刷屏

// ===== musicA.23 待机屏保 =====
// 待机页静置 60 秒后进入：程序化眼睛动画（呼吸/眨眼/扫视），只刷眼睛脏矩形。
// 这里只放前向声明：实现要在 HomePage.h（drawWaiting）之后，touchPoll 又要先看到 saverActive。
bool saverActive = false;         // 屏保是否正在显示
void saverWake();                 // 触摸唤醒（touchPoll 里调）
void saverEnter();                // 进入屏保
void saverExit(bool eatTouch);    // 退出并重画当前页


// ===== 颜色（RGB565）=====
#define C_BLACK  0x0000
#define C_WHITE  0xFFFF
#define C_GREEN  0x07E0
#define C_GREY   0x7BEF
#define C_CYAN   0x07FF
#define C_RED    0xF800
#define C_DIM    0x6B4D   // #6B6B6B 预告、非推荐车道用的灰色
#define C_TRACK  0x2945   // #2A2A2A 进度环底色
#define C_YELLOW 0xF623   // #F5C518
#define C_ORANGE 0xFBC3   // #FF7A1A
#define C_SEP    0x31A6   // #333333 车道分隔线
#define C_SIGNRED 0xE104  // #E02020 限速牌红圈

// ===== 屏幕几何：466x466，圆心 (233, 233) =====
#define CX 233
#define RING_R 222

// ===== 布局（y 坐标）=====
#define SPEED_Y      50    // 车速数字顶部
#define ICON_CY      188   // 没有车道时，图标中心（v1.8.2 起只作初始位置，最终按可用区域自动摆放）
#define ICON_CY_LANE 172   // 有车道时，图标中心
#define ICON_TOP     120   // v1.8.2 图标可用区域上沿（车速 km/h 下方）
#define ICON_MAX_W   250   // 图标最大宽度
#define LANES_Y      238   // 车道行顶部
#define DIST_Y       270   // 没有车道时，距离数字顶部（7 号字，高 56）
#define DIST_Y_LANE  278   // 有车道时，距离数字顶部（6 号字，高 48）
#define ROAD_Y       334   // 没有车道时，路名图片顶部
#define ROAD_Y_LANE  334   // 有车道时，路名图片顶部
#define BOTTOM_Y     372   // 底部信息图片顶部（常驻显示）
#define TEXT_R       200   // 文字必须在这个半径以内（圆环内沿约 215）

// ===== 蓝牙：手机端必须用同样的名字和 UUID =====
#define DEVICE_NAME  "MotoNav"
String bleName = DEVICE_NAME;        // v7.0 实际蓝牙名 "MotoNav-XXXX"（XXXX = 蓝牙地址最后两个字节），开机时算好
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"   // v2.0 圆屏 → 手机通知
BLECharacteristic *txChar = nullptr;

volatile bool bleConnected = false;

// ===== 接收队列：蓝牙线程收到的每个数据包原样放进队列，主循环再取出处理 =====
#define RXQ_SLOTS 48   // v2.0 加大，接收地图大块数据时不丢包
#define RXQ_MAX   512
struct RxMsg { uint16_t len; uint8_t data[RXQ_MAX]; };
RxMsg rxq[RXQ_SLOTS];
volatile int rxHead = 0, rxTail = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ===== 导航与行车状态 =====
volatile bool txSubscribed = false;   // ui.8 手机订阅了 TX 通知 = App 真正在场。
                                      // 圆屏是绑定过的 HID 设备，iOS 系统会在 App 断开后自动重连
                                      // （链路通且已认证），只看 bleConnected 会一直显示 ready；
                                      // 系统级 HID 重连不会订阅我们的 TX 特征，以此区分。
volatile bool appBye = false;         // zelda40.2 App 点断开时先发 "#OFF"：系统级 HID 重连会恢复 CCCD 订阅，
                                      // 单看 txSubscribed 挡不住，收到 #OFF 后强制离线，收到任何新消息解除。
bool shownConn = false;       // 屏幕上反映的连接状态
bool inNav = false;           // 是否处于导航中
String navDir = "";           // 当前方向
int navDist = 0;              // 当前距离（米）
float navAngle = NAN;         // v1.8 路口实际转弯角度；NAN = 没有，用标准角度
unsigned long lastRxMs = 0;   // 最近一次收到任何消息的时间
bool staleShown = false;      // 是否正在显示“数据中断”提示
bool navDirty = false;        // 导航画面需要重画

int speedKmh = -1;            // 车速，-1 表示还没有
int camLimit = 0, camDist = 0;
bool hasCam = false;          // 前方是否有测速
int laneN = 0;                // 车道数，0 表示不显示
// v3.1 小地图底部的行程信息（"#e,剩余米,到达时间HHMM,红绿灯数"）和前方红绿灯（"#g,距离"）
bool tripOn = false;
bool cruiseOn = false;        // v4.8 巡航模式（没有目的地，只显示地图、车速、电子眼、安全提示）
IdlePowerOffTimer idlePowerOffTimer = {0, false};
IdlePowerSupply idlePowerSupply = {0, false, false};
volatile bool powerOffPending = false;
bool cleanOn = false;         // v4.9 清爽模式：地图照常，去掉红绿灯、电子眼、安全提示、测速牌和行程信息（"#w,1/0"）
int tripRemainM = 0, tripEta = 0, tripLights = 0;
// v7.3 路程进度
#include "../../TripProgress.h"
TripProgress tripProgress;
bool progOn = true;
long progDone = 0;                   // 这趟已经走掉的里程（米）
int progLastRm = -1;                 // 上一次收到的剩余里程
unsigned long progLastAt = 0;
#define progReset() do { progDone = 0; progLastRm = -1; progLastAt = 0; tripProgress.reset(); } while (0)   // 宏：放这么靠前写成函数会打乱 Arduino 自动生成的声明
int lightDist = -1;           // 前方红绿灯距离（米），-1 = 不显示
// v3.5 小地图上的电子眼（手机发 "#c,坐标系,x,y,限速,类型,..."，最多 6 个）
#define MCAM_MAX 10           // v4.8 6 → 10（电子眼 + 道路安全提示）
int mapCamN = 0, mapCamFrame = -1;
int32_t mapCamX[MCAM_MAX], mapCamY[MCAM_MAX];
int mapCamSpd[MCAM_MAX];
int mapCamTyp[MCAM_MAX];      // v3.8 图标编号（手机换算好的，0~15）
int8_t mapCamPref[MCAM_MAX];  // v3.8 图标放好了没有（-1 = 还没放过）
float mapCamPrefA[MCAM_MAX];  // v4.1 类型图标在小摄像头的哪个方向（屏幕角度）
int8_t mapCamPrefK[MCAM_MAX]; // v4.3 放图标时这一处有几个类型图标（个数变了就重新放）
float mapCamLockH[MCAM_MAX];  // v4.0 放图标时的地图朝向：之后固定不动，转弯后（朝向变了且已经转稳）才重新放
uint8_t laneBg[8], laneFr[8]; // 每条车道的背景方向码、推荐方向码（0xF = 不推荐）

// ===== 文字图片（手机渲染好的 4 位灰度图）=====
#define IMG_SLOTS 13           // 0 = 路名，1 = 底部信息，2 = 来电人名字，3/4 = 天气地名和风湿度，5/6 = 音乐歌名和歌手
#define IMG_MAX   8192        // 每张图最多字节数
struct TextImg {
  uint8_t *show;              // 正在显示的图
  uint16_t showW; uint8_t showH;
  uint8_t *recv;              // 正在接收的图
  int recvId; uint16_t recvW; uint8_t recvH; uint8_t recvParts; uint32_t recvMask;
};
TextImg imgs[IMG_SLOTS];
AppleMediaClient appleMedia;
volatile uint32_t mediaConnectionEpoch = 0;
uint32_t lyricImageGeneration = 0; uint16_t lyricImageTitleRevision = 0, lyricImageArtistRevision = 0;
bool musicLyrics = false;                  // #ML,0/1; never persist an enabled default
bool musicDirty = true;
bool musicHave = false, musicPlaying = false;
// ===== v7.12.10-cover / zelda40.9-musicA：音乐封面 bulk type 5（手机 CVR）=====
// musicA.5：仍 128 协议；载入一次双线性+轻锐化；旋帧定点最近邻。
// musicA.10：纯圆裁封面（无黑胶环）+ 封面取色氛围底 + 动态唱针；COVER_HI≈盘径。
#define COVER_PX 128
#define COVER_DISC_R 112                               // 纯圆封面半径（居中偏上）
#define COVER_DISC_CY 120                              // 封面圆心 y；底缘≈232，贴近 chrome
#define COVER_HI 256                                   // ≈2R；旋时 scale≈1，省满屏 466²
#define COVER_MAX (6 + COVER_PX * COVER_PX * 2 + 64)   // 'CVR' 头(6) + RGB565 方图 + 余量
#define MUSIC_CHROME_Y0 220                            // 歌名/键所在下半区
uint16_t *coverImg = nullptr;              // PSRAM：COVER_PX*COVER_PX RGB565（解码原图）
uint16_t *coverHi = nullptr;               // PSRAM：COVER_HI*COVER_HI 双线性放大缓存
int coverW = 0, coverH = 0;
bool coverValid = false;
bool coverHiReady = false;
// musicA.39 封面轮播邻封：上一首封面历史（换歌时拷贝）；下一首 AMS 不提供 → 用占位/色偏变体
uint16_t *coverPrevImg = nullptr;
int coverPrevW = 0, coverPrevH = 0;
bool coverPrevValid = false;
float coverAngle = 0.0f;                   // 弧度；播放中慢旋，暂停停转
unsigned long coverSpinAt = 0;
bool coverSpinEnabled = true;              // v7.12.10 音乐封面是否旋转（App 经 #RC 下发，NVS 持久化）
uint16_t coverAmbient = 0x1082;            // 封面主色氛围底（载入时采样）
float tonearmT = 0.0f;                     // 0=暂停抬离，1=播放落边
bool musicChromeReady = false;
uint16_t *musicChrome = nullptr;           // PSRAM：底区 overlay；0=透明
// musicA.38 / FO-218 音乐样式：0 原版·冷雾银 / 1 封面轮播 / 2 柔光立体 / 3 歌词主导 / 4 极简仪表 / 5 全屏转动封面 / 6 黑胶唱片
// App 经 #MS,<id> 下发，NVS 持久化；切换后 musicDirty 整页重画。
uint8_t musicStyle = 0;
// ui.6 音乐封面风格化背景：0 关闭 / 1 放大虚化 / 2 色块马赛克 / 3 双色海报。
// App 经 #MBG,<mode> 下发，NVS "mbg"；换封面或换档位才重建一次整屏缓存。
uint8_t  musicBgMode  = 1;              // 默认放大虚化（刷机后放音乐即可见）
bool     musicBgDirty = true;           // 换封面 / 换档位置位 → 下一次画背景时重建
uint16_t *musicBgBuf  = nullptr;        // PSRAM：466×466 RGB565
bool     musicBgFailed = false;         // 分配失败 → 永久退回 coverAmbient 纯色
// PR5：手机 #Z 可选附带 duration/elapsed（毫秒），固件本地推演进度条。
uint32_t musicDurMs = 0, musicElapsedMs = 0;
unsigned long musicProgAt = 0;
// ===== 电子吧唧（bulk type 7，466×466；避开上游安卓 TYPE_COVER=6）=====
// ver=1：'BDG' 1 w_le16 h_le16 + zlib → slot 0；ver=2：多槽轮播（头 10B）。
#define BADGE_PX 466
#define BADGE_SLOTS 3
#define BADGE_HDR_V1 8
#define BADGE_HDR_V2 10
#define BADGE_HDR BADGE_HDR_V2
#define BADGE_MAX (BADGE_HDR + BADGE_PX * BADGE_PX * 2 + 64)
#define BADGE_CAROUSEL_MS 3000UL
uint16_t *badgeSlots[BADGE_SLOTS] = {nullptr, nullptr, nullptr};
int badgeSlotW[BADGE_SLOTS] = {0, 0, 0};
int badgeSlotH[BADGE_SLOTS] = {0, 0, 0};
bool badgeSlotOk[BADGE_SLOTS] = {false, false, false};
int badgeCount = 0;
int badgeIndex = 0;
unsigned long badgeCarouselAt = 0;
bool badgeDirty = false;
bool pendingBadgeCapability = false;      // CAP,BDG,2；ACK 仍 K,7,id
// musicA.18 吧唧文字滚动：#BT,<mode>（0 图片 / 1 横向滚动）。文本由 App 渲染成
// 宽位图（w≤2048，需 w*h ≤ 466²）经 type 7 推送；固件按列回绕横滚显示。
int badgeMode = 0;                        // NVS bt_mode
unsigned long badgeScrollAt = 0;
inline uint16_t *badgeCurrentImg() {
  return (badgeIndex >= 0 && badgeIndex < BADGE_SLOTS) ? badgeSlots[badgeIndex] : nullptr;
}
inline int badgeCurrentW() {
  return (badgeIndex >= 0 && badgeIndex < BADGE_SLOTS) ? badgeSlotW[badgeIndex] : 0;
}
inline int badgeCurrentH() {
  return (badgeIndex >= 0 && badgeIndex < BADGE_SLOTS) ? badgeSlotH[badgeIndex] : 0;
}
inline bool badgeHasCurrent() {
  return badgeCount > 0 && badgeIndex >= 0 && badgeIndex < BADGE_SLOTS && badgeSlotOk[badgeIndex];
}
int musicActions = 0;
int musicPressed = -1, musicAck = 0;
unsigned long musicAckUntil = 0;
// HID adds a second GATT service to the existing authenticated navigation connection.
bool hidMusicMode = false;                  // persisted by #HM,0/1; Android explicitly selects 0
BLEHIDDevice *mediaHid = nullptr;
BLECharacteristic *mediaInput = nullptr;
volatile bool hidSubscribed = false, hidSuspended = false, hidReportMode = true;
volatile uint16_t hidSubscriber = 0xFFFF;
volatile bool hidNotifyAccepted = false;
volatile uint32_t hidSessionEpoch = 0;
uint32_t hidSeenEpoch = 0;
bool hidReadyShown = false, hidReleaseOnReady = true;
String pendingHidStatus, pendingLyricsStatus, pendingMediaStatus;
unsigned long hidInitialReleaseAt = 0;
HidMediaKeys::Pulses hidPulses;
unsigned long hidTapAt = 0;
void hidMediaTick();
bool hidMediaReady();
bool wxDirty = false;                 // v5.3 天气页要重画

// ================= v6.5 蓝牙配对验证 =================
#define PAIR_WINDOW_MS  120000UL      // 开机 2 分钟内可以配对新手机
#define AUTH_TIMEOUT_MS 60000UL       // 连上 60 秒还没通过验证就断开
volatile bool bleAuthed = false;      // 当前连接已加密 + 通过配对码验证
volatile uint16_t bleConnHandle = 0xFFFF;
volatile unsigned long bleConnAt = 0, bleAuthAt = 0;
volatile uint32_t pairCode = 0;       // 要显示的配对码
volatile bool pairReq = false;        // 有新的配对请求（蓝牙线程 → 主循环）
volatile bool pairLocked = false;     // 不在允许配对的时间：不显示配对码
volatile bool pairDone = false;       // 配对结束（成功或失败）
volatile bool pairOk = false;
int authNotifies = 0;                 // "U,1" 已经发了几次
