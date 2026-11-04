import os, time, urllib.request, json

TOK = os.environ["GH_TOKEN"]
PROXY = "http://127.0.0.1:7890"
BRANCH = "ios-v0.16.2.12-slotfix"
WF = "release-ios.yml"
API = "https://api.github.com/repos/coolcoolmanman-design/MotoNav"

def get(url):
    req = urllib.request.Request(url, headers={
        "Authorization": "Bearer " + TOK,
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
    })
    proxy = urllib.request.ProxyHandler({"https": PROXY})
    opener = urllib.request.build_opener(proxy)
    with opener.open(req, timeout=30) as r:
        return json.loads(r.read().decode())

def find_run():
    d = get(f"{API}/actions/workflows/{WF}/runs?branch={BRANCH}&per_page=5")
    for r in d.get("workflow_runs", []):
        if r.get("event") in ("workflow_dispatch", "push"):
            return r
    return None

start = time.time()
last = None
while time.time() - start < 3600:
    run = find_run()
    if run:
        st = run.get("status")
        con = run.get("conclusion")
        rid = run.get("id")
        line = f"{int(time.time()-start)}s status={st} conclusion={con} run={rid}"
        if line != last:
            print(line, flush=True)
            last = line
        if st == "completed":
            rel = run.get("html_url", "")
            # get release tag
            tag = None
            try:
                rr = get(f"{API}/actions/runs/{rid}")
                # no direct release link; query releases for ios-v0.16.2.12
                rels = get(f"{API}/releases?per_page=20")
                for x in rels:
                    if x.get("tag_name","").startswith("ios-v0.16.2"):
                        tag = x.get("html_url")
                        break
            except Exception as e:
                tag = f"(release lookup failed: {e})"
            print(f"DONE conclusion={con} run={rel}", flush=True)
            if tag:
                print(f"RELEASE={tag}", flush=True)
            break
    time.sleep(20)
else:
    print("TIMEOUT", flush=True)
