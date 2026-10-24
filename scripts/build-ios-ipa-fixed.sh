#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DERIVED="${MOTONAV_DERIVED_DATA:-$(mktemp -d /tmp/MotoNavRelease.XXXXXX)}"
OUT="$ROOT/output/ios"
mkdir -p "$OUT"

# Xcode 27.1 下 @State 等 SwiftUI 属性是宏，本机嵌套沙箱会让 swift-plugin-server 崩
# (sandbox_apply EPERM)，必须进程内直载宏插件 dylib。
MACRO_DYLIB="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/lib/swift/host/plugins/libSwiftUIMacros.dylib"

xcodebuild -workspace "$ROOT/ios/MotoNav.xcworkspace" -scheme MotoNav \
  -destination 'generic/platform=iOS' -configuration Release \
  -derivedDataPath "$DERIVED" CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO DEVELOPMENT_TEAM= \
  OTHER_SWIFT_FLAGS="-load-plugin-library $MACRO_DYLIB" \
  build

APP="$DERIVED/Build/Products/Release-iphoneos/MotoNav.app"
VERSION=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/Info.plist")
EXPECTED=$(tr -d '[:space:]' < "$ROOT/RELEASE_VERSION")
EXT_VERSION=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/PlugIns/MotoNavShare.appex/Info.plist")
if [[ "$VERSION" != "$EXPECTED" || "$EXT_VERSION" != "$EXPECTED" ]]; then
  echo "Version mismatch: expected=$EXPECTED app=$VERSION extension=$EXT_VERSION" >&2
  exit 1
fi

STAGE=$(mktemp -d /tmp/MotoNavPayload.XXXXXX)
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/Payload"
ditto "$APP" "$STAGE/Payload/MotoNav.app"
find "$STAGE" -name _CodeSignature -type d -prune -exec rm -rf {} +
find "$STAGE" -name embedded.mobileprovision -delete
xcrun strip -S "$STAGE/Payload/MotoNav.app/MotoNav"
xcrun strip -S "$STAGE/Payload/MotoNav.app/PlugIns/MotoNavShare.appex/MotoNavShare"
IPA="$OUT/MotoNav-$VERSION-unsigned.ipa"
rm -f "$IPA"
(cd "$STAGE" && /usr/bin/zip -qry "$IPA" Payload)
(cd "$OUT" && shasum -a 256 "$(basename "$IPA")" > SHA256SUMS.txt)
echo "$IPA"
