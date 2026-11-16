#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""GitHub API 推送工具（git push 走不了时用）。
用法: python api_push.py "<commit message 沿用本地 HEAD 提交>" 或直接无参（默认沿用 HEAD 提交信息）
原理: 本地最新提交的变更文件 → blobs → trees(base_tree=远端head) → commit(parent=远端head) → 更新 ref。
前提: 本地每个提交与远端一一对应（每次只推最新一个提交）。
"""
import json, base64, subprocess, sys, urllib.request

PROXY = "http://127.0.0.1:8811"
OWNER, REPO = "coolcoolmanman-design", "MotoNav"
BRANCH = "cursor/firmware-musica17-sixslot"
REPO_DIR = r"D:\workbuddy\2026-10-26-08-12-36\wt-iosfix"

TOKEN = subprocess.run(["git","credential","fill"], input="protocol=https\nhost=github.com\n",
                       capture_output=True, text=True).stdout
TOKEN = [l.split("=",1)[1] for l in TOKEN.splitlines() if l.startswith("password=")][0]
opener = urllib.request.build_opener(urllib.request.ProxyHandler({"http": PROXY, "https": PROXY}))
def api(method, path, payload=None):
    req = urllib.request.Request(f"https://api.github.com/repos/{OWNER}/{REPO}/{path}",
        method=method, data=json.dumps(payload).encode() if payload is not None else None,
        headers={"Authorization": f"token {TOKEN}", "Accept": "application/vnd.github+json",
                 "Content-Type": "application/json", "User-Agent": "api-push"})
    with opener.open(req, timeout=60) as r:
        body = r.read()
        return json.loads(body.decode()) if body.strip() else {}

BASE = api("GET", f"git/ref/heads/{BRANCH}")["object"]["sha"]
files = subprocess.run(["git","diff","--name-only","HEAD~1","HEAD"], capture_output=True, text=True).stdout.split()
if not files:
    sys.exit("HEAD~1..HEAD 无变更文件")
tree_items = []
TEXT_EXT = (".h", ".hpp", ".c", ".cpp", ".ino", ".swift", ".md", ".py", ".sh", ".yml", ".yaml",
            ".json", ".txt", ".xml", ".plist", ".pbxproj")
for f in files:
    data = open(f, "rb").read()
    # Windows 工作区文件是 CRLF；api 直推会绕过 git 的 autocrlf 归一化，
    # 曾把 RELEASE_VERSION 推成 "0.16.2.21\r\n" 导致 iOS 打包 test -f 失败。文本一律转 LF。
    if f.endswith(TEXT_EXT) and b"\r\n" in data:
        data = data.replace(b"\r\n", b"\n")
    blob = api("POST", "git/blobs", {"content": base64.b64encode(data).decode(), "encoding": "base64"})
    tree_items.append({"path": f, "mode": "100644", "type": "blob", "sha": blob["sha"]})
base_tree = api("GET", f"git/commits/{BASE}")["tree"]["sha"]
tree = api("POST", "git/trees", {"base_tree": base_tree, "tree": tree_items})
msg = subprocess.run(["git","log","--format=%B","-1","HEAD"], capture_output=True, text=True).stdout
commit = api("POST", "git/commits", {"message": msg, "tree": tree["sha"], "parents": [BASE]})
api("PATCH", f"git/refs/heads/{BRANCH}", {"sha": commit["sha"], "force": False})
print(f"PUSHED OK {len(files)} files -> {commit['sha'][:7]}")
