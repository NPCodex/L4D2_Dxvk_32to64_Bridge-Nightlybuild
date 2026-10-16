"""Publish the complete validated L4N distribution with Bridge and concise notices."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
from archive_release import archive
from engine_payload import (staged_engine_files, read_regular, ORIGINAL_SHA256,
                            PATCHED_SHA256)
from l4n_payload import staged_l4n_files, manifest_metadata, MANIFEST_NAME, VERSION as L4N_VERSION

PLUGIN_SOURCE = "optional/L4N/L4D2BridgePlugin.dll"
PLUGIN_DESTINATION = "bin/neko/plugins/L4D2BridgePlugin.dll"
REQUIRED = ("bin/d3d9.dll", "bin/.l4d2bridge/L4D2Bridge64.exe",
            "bin/.l4d2bridge/L4D2Bridge32.exe", "bin/.l4d2bridge/d3d9vk_x86.dll",
            PLUGIN_SOURCE,
            "bin/.l4d2bridge/d3d9vk_x64.dll", "bin/.l4d2bridge/bridge.conf")
NOTICE_INPUTS = ("LICENSE", "THIRD_PARTY.md", "licenses/Bridge-MIT.txt",
                "licenses/Bridge-third-party.txt", "licenses/DXVK-LICENSE.txt",
                "licenses/DXVK-GPLALL-LICENSE.txt", "licenses/L4N-NOTICE.txt",
                "licenses/Valve-engine-NOTICE.txt", "readme_l4n.txt")
def select_l4n_runtime(files):
    # All original companion tools, SDK and instructions now ship. Metadata and
    # arbitrary staging files remain excluded by the exact approved manifest.
    metadata = manifest_metadata(files[MANIFEST_NAME])
    return {name: files[name] for name in metadata["files"]}


def combined_notices(source, verified):
    header = (
        "L4D2 Bridge / DXVK-GPLALL / L4N / Valve engine notices\n"
        "L4N original author: Starfelll (@Starfelll).\n\n"
        "The documents below retain their original text. The complete approved\n"
        "L4N distribution includes its tools, SDK and original instructions.\n"
        "Bridge repository documents and validation metadata remain in the repository.\n"
        "Installation of this integration package follows README.txt. The original\n"
        "L4N instructions below also describe standalone DXVK and developer tools.\n"
        "Repository: https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild\n"
    ).encode("utf-8")
    sections = [header]
    for name in NOTICE_INPUTS:
        data = verified[name] if name in verified else read_regular(source / name)
        if not data.strip():
            raise ValueError(f"Missing required notice text: {name}")
        # Keep each original license/readme byte sequence, including its line endings.
        sections.extend((f"\n\n===== {name} =====\n\n".encode("utf-8"), data))
    return b"".join(sections)


def runtime_archive(source, output):
    source, output = Path(source), Path(output)
    if output.exists():
        raise FileExistsError(f"Archive already exists: {output}")
    for name in REQUIRED:
        if not (source / name).is_file():
            raise ValueError(f"Missing runtime package file: {name}")
    # Validate complete source snapshots before selecting the exact shipped files.
    engine_files = staged_engine_files(source)
    l4n_files = staged_l4n_files(source)
    bridge_files = {name: read_regular(source / name) for name in REQUIRED}
    # Windows paths are case-insensitive. Reject collisions before any union or
    # plugin relocation can silently replace a supplied or generated file.
    occupied = {}
    for group in (engine_files, l4n_files, bridge_files,
                  (PLUGIN_DESTINATION, "README.txt", "THIRD-PARTY-NOTICES.txt")):
        for name in group:
            key = name.casefold()
            if key in occupied:
                raise ValueError(f"Package file collision: {occupied[key]} and {name}")
            occupied[key] = name
    verified = {**engine_files, **l4n_files, **bridge_files}
    hashes = json.loads(read_regular(source / "SHA256.json"))
    version = read_regular(source / "VERSION").decode("utf-8").strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", version):
        raise ValueError("Invalid source package version")
    # bridge.conf was historically outside the binary manifest. Every shipped
    # Client/Host/backend/plugin must still match the full package's receipt.
    for name in REQUIRED:
        if name.endswith((".dll", ".exe")):
            if hashes.get(name) != hashlib.sha256(verified[name]).hexdigest():
                raise ValueError(f"Runtime binary differs from the source package receipt: {name}")
    notices = combined_notices(source, verified)
    files = {**bridge_files, "bin/studiorender.dll": engine_files["bin/studiorender.dll"],
             **select_l4n_runtime(l4n_files)}
    # The original build receipt verifies the plugin at its staging path;
    # the player ZIP installs that same verified snapshot into L4N directly.
    files[PLUGIN_DESTINATION] = files.pop(PLUGIN_SOURCE)
    files["THIRD-PARTY-NOTICES.txt"] = notices
    files["README.txt"] = (
        f"L4D2 Bridge v{version} runtime package\n\n"
        "已包含 DXVK（GPLALL）、L4N 和桥接工具；退出游戏后备份原文件，将本包解压覆盖到游戏根目录即可安装。请勿与其他类似整合项目混装。\n"
        f"完整保留 L4N {L4N_VERSION} 的运行文件、bin/neko 工具与 SDK、to_l4n_survivor.exe、left4dead2 no-insecure.bat 和原作者 readme_l4n.txt；config_template.vdf 的更新已合并到实际 config.vdf，模板不单独附带。\n"
        "默认 x64、C8，静态映射关闭；Steam/Presenter 配套启用。当前版本的新增改动、验证及已知问题见对应 GitHub Release；局部微基准不代表游戏 FPS，本包不承诺全场景性能改善。\n"
        "IPC v2 协议标识为 0x00020004：bin/d3d9.dll 与 L4D2Bridge32.exe、L4D2Bridge64.exe 必须一起更新或回退，不能混用旧版。\n"
        "移除 -vulkan 启动参数，备份移走游戏根目录的 d3d9.dll；保留本包 bin/d3d9.dll 和 bin/.l4d2bridge 目录。\n"
        "升级时先解压到临时目录，保留原有 dxvk.conf、bin/.l4d2bridge/bridge.conf、left4dead2/neko/config.vdf、自定义后端、ReShade 和 retention DB，再合并覆盖。客户端与两个 Host 一并更新、回退。\n"
        "L4N 原作者：Starfelll（@Starfelll）。Bridge 设置菜单插件已放在 bin/neko/plugins/L4D2BridgePlugin.dll，随包安装；不需要菜单时退出游戏后移走该 DLL。\n"
        "已含 ThinFlex 修复，无需运行修复工具。覆盖前备份原始 bin/studiorender.dll；已修复玩家保留最初原始备份。只有下列原版或修复版身份匹配时才替换，未知游戏版本请从临时目录移除该 DLL。回退时恢复对应原版备份。\n"
        f"ThinFlex 原版 SHA-256：{ORIGINAL_SHA256}\n"
        f"ThinFlex 修复版 SHA-256：{PATCHED_SHA256}\n\n"
        "完整文档、配置说明与来源校验：\n"
        "https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild\n"
        "第三方许可及原作者说明见 THIRD-PARTY-NOTICES.txt。\n"
    ).encode("utf-8")
    with tempfile.TemporaryDirectory() as temporary:
        stage = Path(temporary) / "runtime"
        stage.mkdir()
        for name, data in files.items():
            destination = stage / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        temporary_zip = Path(temporary) / "runtime.zip"
        archive(stage, temporary_zip)
        created_output = False
        try:
            with output.open("xb") as target:
                created_output = True
                with temporary_zip.open("rb") as source_zip:
                    shutil.copyfileobj(source_zip, target)
        except BaseException:
            if created_output:
                output.unlink(missing_ok=True)
            raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    runtime_archive(args.source.resolve(), args.output.resolve())
