"""Observe Remix HEAD while building only an approved or manually selected SHA."""
import json
import os
import re
import time
import urllib.error
import urllib.parse
import urllib.request

from release_names import classify
from pathlib import Path
from build_identity import recipe_digest

UPSTREAM = "repos/NVIDIAGameWorks/dxvk-remix"
ROOT = Path(__file__).resolve().parents[1]
APPROVAL_PATH = ROOT / "config/nvidia-upstream.json"


def read_original_project(root=None):
    root = ROOT if root is None else root
    original = json.loads((root / "config/original-project.json").read_text(encoding="utf-8"))
    if not isinstance(original, dict) or original.get("integration") not in ("full", "partial"):
        raise ValueError("L4D2 integration must be explicitly full or partial")
    for field in ("repository", "version"):
        if not isinstance(original.get(field), str) or not original[field].strip():
            raise ValueError(f"L4D2 provenance requires {field}")
    commits = ["commit", "main_baseline"]
    if original["integration"] == "partial":
        commits.extend(("reviewed_commit", "partial_commit"))
        if not isinstance(original.get("partial_version"), str) or not original["partial_version"].strip():
            raise ValueError("Partial L4D2 integration requires partial_version")
    for field in commits:
        if not isinstance(original.get(field), str) or not re.fullmatch(r"[0-9a-f]{40}", original[field]):
            raise ValueError(f"L4D2 provenance requires a full {field} SHA")
    return original


def read_approval():
    approval = json.loads(APPROVAL_PATH.read_text(encoding="utf-8"))
    if (not isinstance(approval, dict)
            or approval.get("repository") != "https://github.com/NVIDIAGameWorks/dxvk-remix"
            or not isinstance(approval.get("branch"), str)
            or not approval["branch"].strip()
            or not isinstance(approval.get("approved_commit"), str)
            or not re.fullmatch(r"[0-9a-fA-F]{40}", approval["approved_commit"])):
        raise ValueError("NVIDIA approval must identify the repository, branch and full approved commit SHA")
    return approval["branch"], approval["approved_commit"].lower()


def api(path):
    request = urllib.request.Request("https://api.github.com/" + path, headers={
        "Authorization": "Bearer " + os.environ["GH_TOKEN"],
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
    })
    for attempt in range(3):
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            if error.code not in (429, 500, 502, 503, 504) or attempt == 2:
                raise
            time.sleep(2 ** attempt)


def pending():
    force = os.environ.get("FORCE_REBUILD", "false").lower() == "true"
    thinflex = os.environ.get("THINFLEX_TEST", "false").lower() == "true"
    channel = "thinflex-test" if thinflex else "nightly"
    digest_label = "Experimental recipe digest" if thinflex else "Recipe digest"
    manual = os.environ.get("UPSTREAM_COMMIT", "").strip()
    if manual:
        if not re.fullmatch(r"[0-9a-fA-F]{40}", manual):
            raise ValueError("Manual builds require a full 40-character commit SHA")
        ref, branch = manual.lower(), "manual"
    else:
        branch, ref = read_approval()
        observed_branch = api(UPSTREAM)["default_branch"]
        observed = api(f"{UPSTREAM}/commits/{urllib.parse.quote(observed_branch, safe='')}")["sha"]
        if not isinstance(observed, str) or not re.fullmatch(r"[0-9a-f]{40}", observed):
            raise ValueError("Invalid observed upstream SHA")
        print(f"Observed NVIDIA {observed_branch}: {observed}; approved source: {ref}")
        if observed != ref:
            print("NVIDIA HEAD awaits review; retaining the approved build source")
    info = api(f"{UPSTREAM}/commits/{urllib.parse.quote(ref, safe='')}")
    commit = info["sha"]
    if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-f]{40}", commit) or commit != ref:
        raise ValueError("Resolved upstream SHA differs from the approved or manual commit")
    recipe = recipe_digest()
    if not force:
        page = 1
        while True:
            releases = api(f"repos/{os.environ['GITHUB_REPOSITORY']}/releases?per_page=100&page={page}")
            for release in releases:
                lines = (release.get("body") or "").splitlines()
                release_channel = next((line.split(": ", 1)[1] for line in lines
                                        if line.startswith("Release channel: ")), "nightly")
                if (not release["draft"] and f"Upstream commit: {commit}" in lines
                        and release_channel == channel
                        and f"{digest_label}: {recipe}" in lines):
                    print(f"Already published upstream and recipe as {release['tag_name']}")
                    return []
            if len(releases) < 100:
                break
            page += 1
    else:
        print(f"Force rebuild enabled for upstream {branch}: {commit}")
    names = classify(commit, info["commit"]["committer"]["date"], recipe,
                     os.environ["GITHUB_RUN_ID"], os.environ["GITHUB_RUN_ATTEMPT"])
    version = (ROOT / "VERSION").read_text().strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", version):
        raise ValueError("Invalid project version")
    identifier = "v" + version + ("-thinflex-test-" if thinflex else "-") + names["release_tag"]
    original = read_original_project()
    upstream_title = ("上游 " + original["partial_version"] + " 选择性更新"
                      if original["integration"] == "partial"
                      else "完整同步上游 " + original["version"])
    title = "v" + version + " L4N 整合包 · " + upstream_title + (" + ThinFlex 测试版" if thinflex else "")
    names.update(release_tag=identifier, title=title,
                 archive="l4d2-bridge-" + identifier + ".zip")
    return [{"tag": names["group"], "commit": commit, "branch": branch,
             "recipe_digest": recipe, **names}]


if __name__ == "__main__":
    rows = pending()
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        output.write("matrix=" + json.dumps({"include": rows}) + "\n")
        output.write("pending=" + str(bool(rows)).lower() + "\n")
    print(json.dumps(rows, indent=2))


