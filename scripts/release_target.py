"""Read-only identity checks for explicitly authorized existing-release rebuilds."""
import json
import re
from urllib.parse import quote


def read_release_target(root, repository, version):
    path = root / "config/release-target.json"
    if not path.exists():
        return None
    target = json.loads(path.read_text(encoding="utf-8"))
    if (not isinstance(target, dict) or type(target.get("schema")) is not int or target.get("schema") != 1
            or target.get("mode") != "replace-existing"
            or target.get("repository") != repository
            or target.get("version") != version
            or type(target.get("release_id")) is not int or target["release_id"] <= 0):
        raise ValueError("Existing-release target must match this repository and VERSION")
    tag = target.get("tag")
    if (not isinstance(tag, str)
            or not re.fullmatch(r"v" + re.escape(version)
                                + r"-nightly-\d{8}-[0-9a-f]{8}-r[0-9a-f]{12}-b[1-9]\d*\.[1-9]\d*", tag)
            or target.get("archive") != "l4d2-bridge-" + tag + ".zip"):
        raise ValueError("Existing-release target must retain a safe version-matched tag and archive name")
    return target


def resolve_release_target(target, api):
    """Fail closed before a build; this function never creates or updates GitHub data."""
    base = "repos/" + target["repository"]
    release = api(base + "/releases/" + str(target["release_id"]))
    if (not isinstance(release, dict) or release.get("id") != target["release_id"] or release.get("tag_name") != target["tag"]
            or release.get("draft") is not False or release.get("prerelease") is not False):
        raise ValueError("Existing-release identity or published stable status changed")
    names = {target["archive"], target["archive"] + ".sha256"}
    assets = release.get("assets", [])
    if (not isinstance(assets, list) or len(assets) != 2
            or any(not isinstance(asset, dict) for asset in assets)
            or {asset.get("name") for asset in assets} != names
            or any(asset.get("state") != "uploaded" or type(asset.get("size")) is not int
                   or asset["size"] <= 0 for asset in assets)):
        raise ValueError("Existing-release assets differ from the authorized full ZIP and checksum")
    latest = api(base + "/releases/latest")
    if not isinstance(latest, dict) or latest.get("id") != target["release_id"]:
        raise ValueError("Existing-release target is no longer Latest")
    reference = api(base + "/git/ref/tags/" + quote(target["tag"], safe=""))
    if not isinstance(reference, dict) or not isinstance(reference.get("object"), dict):
        raise ValueError("Existing-release tag must already exist")
    obj = reference["object"]
    if (reference.get("ref") != "refs/tags/" + target["tag"]
            or obj.get("type") not in ("commit", "tag")
            or not isinstance(obj.get("sha"), str)
            or not re.fullmatch(r"[0-9a-f]{40}", obj["sha"])):
        raise ValueError("Existing-release tag must already exist")
    return {"release_id": target["release_id"], "release_tag": target["tag"],
            "archive": target["archive"], "delivery_mode": "replace-existing",
            "publish_allowed": False}
