#!/bin/bash
# MotoNav iOS：iPhone Duo 折叠屏适配效果图
# 场景：① iPhone Air 默认底部横条 ② 强制右侧竖栏（模拟外屏）③ iPad 宽屏（模拟展开态）
set -e
IOS=/Users/coolmanmani/WorkBuddy/2026-10-17-23-53-58/MotoNav/ios
OUT=/Users/coolmanmani/WorkBuddy/2026-10-17-23-53-58/MotoNav/output/ios/shots
mkdir -p "$OUT"
export TMPDIR=$IOS/.tmp
export PATH="/usr/bin:/bin:/usr/sbin:/sbin"
BUNDLE=com.coolmanman.motonav.dd
SDKROOT=/Applications/Xcode.app/Contents/Developer/Platforms
MACRO="$SDKROOT/iPhoneSimulator.platform/Developer/usr/lib/swift/host/plugins/libSwiftUIMacros.dylib"

# ---- 1. 找 iOS runtime / 机型 ----
RT=$(xcrun simctl list runtimes -j | /usr/bin/python3 -c "import json,sys
rs=[r for r in json.load(sys.stdin) if r['name'].startswith('iOS') and r.get('isAvailable')]
print(rs[0]['identifier'] if rs else '')")
[ -z "$RT" ] && { echo "没有可用的 iOS runtime，先 xcodebuild -downloadPlatform iOS"; exit 1; }
AIR_TYPE=$(xcrun simctl list devicetypes -j | /usr/bin/python3 -c "import json,sys
ts=[t['identifier'] for t in json.load(sys.stdin) if 'iPhone-Air' in t['identifier']]
print(ts[0] if ts else '')")
IPAD_TYPE=$(xcrun simctl list devicetypes -j | /usr/bin/python3 -c "import json,sys
ts=[t['identifier'] for t in json.load(sys.stdin) if 'iPad-mini' in t['identifier']]
print(ts[0] if ts else '')")
echo "runtime=$RT air=$AIR_TYPE ipad=$IPAD_TYPE"

mk() { xcrun simctl create "$1" "$2" "$RT" 2>/dev/null || xcrun simctl list devices -j | /usr/bin/python3 -c "
import json,sys
d=json.load(sys.stdin)
for r in d['devices'].values():
    for x in r:
        if x['name']=='$1': print(x['udid']); break
else: sys.exit(1)"; }

AIR=$(mk "MotoNav-Air" "$AIR_TYPE")
IPAD=$(mk "MotoNav-Pad" "$IPAD_TYPE")

# ---- 2. 构建（模拟器版）----
build() { # $1=udid  $2=插件路径
  xcodebuild -workspace "$IOS/MotoNav.xcworkspace" -scheme MotoNav -configuration Debug \
    -destination "platform=iOS Simulator,id=$1" -derivedDataPath "$IOS/.tmp/dd" \
    CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
    OTHER_SWIFT_FLAGS="-load-plugin-library $2" build >"$IOS/.tmp/build.log" 2>&1
}
build "$AIR" "$MACRO"
APP="$IOS/.tmp/dd/Build/Products/Debug-iphonesimulator/MotoNav.app"
[ -d "$APP" ] || { echo "构建失败，看 .tmp/build.log"; tail -20 "$IOS/.tmp/build.log"; exit 1; }
echo "构建 OK：$APP"

# ---- 3. 截图 ----
shot() { # $1=udid $2=tab_placement $3=main_tab $4=appearance $5=文件名
  local U="$1" TP="$2" MT="$3" AP="$4" F="$5"
  xcrun simctl boot "$U" 2>/dev/null || true
  xcrun simctl bootstatus "$U" -b >/dev/null 2>&1 || sleep 15
  xcrun simctl install "$U" "$APP"
  xcrun simctl terminate "$U" "$BUNDLE" 2>/dev/null || true
  # 预置默认值：假 key（避免弹未填 key 提示）、导航位置、起始页、外观
  xcrun simctl spawn "$U" defaults write "$BUNDLE" key_amap -string "demo0000demo0000" 2>/dev/null || true
  xcrun simctl spawn "$U" defaults write "$BUNDLE" key_tdt  -string "demo0000demo0000" 2>/dev/null || true
  xcrun simctl spawn "$U" defaults write "$BUNDLE" tab_placement -int "$TP"
  xcrun simctl spawn "$U" defaults write "$BUNDLE" main_tab -int "$MT"
  xcrun simctl spawn "$U" defaults write "$BUNDLE" appearance -int "$AP"
  xcrun simctl privacy "$U" grant location "$BUNDLE" 2>/dev/null || true   # 别弹定位授权挡画面
  xcrun simctl ui "$U" appearance $([ "$AP" = 2 ] && echo dark || echo light) >/dev/null 2>&1 || true
  xcrun simctl launch "$U" "$BUNDLE" >/dev/null
  sleep 4
  xcrun simctl io "$U" screenshot "$OUT/$F"
  echo "截图：$OUT/$F"
  xcrun simctl shutdown "$U" 2>/dev/null || true
}

# ① iPhone Air · 浅色 · 默认（底部横条）
shot "$AIR" 0 0 0 "air-1-底部横条-浅色.png"
# ② iPhone Air · 浅色 · 强制右侧竖栏（模拟外屏）
shot "$AIR" 2 0 0 "duo-2-外屏右侧竖栏-浅色.png"
# ③ iPhone Air · 浅色 · 右侧竖栏 + 圆屏设置页（看「折叠屏适配」卡片）
shot "$AIR" 2 3 0 "duo-3-外屏右侧竖栏-设置页-浅色.png"
# ④ iPhone Air · 深色 · 右侧竖栏
shot "$AIR" 2 0 2 "duo-4-外屏右侧竖栏-深色.png"
# ⑤ iPhone Air · 深色 · 底部横条
shot "$AIR" 0 0 2 "air-5-底部横条-深色.png"
# ⑥ iPad 宽屏（模拟展开态：底部横条 + 内容限宽）
build "$IPAD" "$MACRO" && shot "$IPAD" 0 0 0 "duo-6-展开态宽屏-底部横条-浅色.png"

xcrun simctl delete "$AIR" 2>/dev/null || true
xcrun simctl delete "$IPAD" 2>/dev/null || true
echo "全部完成 → $OUT"
