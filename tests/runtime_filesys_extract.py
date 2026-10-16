"""Extract the actual filesystem and startup/config functions for native tests."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from native_source import function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variant', choices=('real', 'no-root-routing', 'no-fallback', 'false-success'), default='real')
    args = parser.parse_args()
    names = ['bridge/src/util/util_filesys.h', 'bridge/src/util/util_filesys.cpp',
             'bridge/src/util/util_process.h', 'bridge/src/util/runtime_path.h',
             'bridge/src/client/d3d9_lss.cpp', 'bridge/src/util/config/config.cpp']
    contents = {name: (args.source / name).read_text(encoding='utf-8') for name in names}
    header = contents[names[0]]
    header = header[header.index('namespace dxvk::util {'):]
    impl = contents[names[1]]
    impl = impl[impl.index('namespace dxvk::util {'):]
    arch = contents[names[2]]
    arch = arch[arch.index('  static const char* RTX_ARCH_UNKNOWN'):arch.rindex('}')]
    client = function(contents[names[4]], 'bool initFileSys(const HMODULE hModule)')
    config = function(contents[names[5]], 'Config Config::getUserConfig(const App app)')
    if args.variant == 'no-root-routing':
        before = 'l4d2_paths::runtimeBase(hModulePath.parent_path(), ".l4d2bridge")'
        assert client.count(before) == 1
        client = client.replace(before, 'hModulePath.parent_path()')
    elif args.variant == 'no-fallback':
        before = 's_remixPath / runtimeDirName(bridge_util::RTX_ARCH_X64)'
        assert impl.count(before) == 1
        impl = impl.replace(before, 's_remixPath / runtimeDirName(getNativeArchString())')
    elif args.variant == 'false-success':
        before = 'return dxvk::util::RtxFileSys::isInitialized();'
        assert client.count(before) == 1
        client = client.replace(before, 'return true;')
    # Only the OS architecture/path query and logging are supplied by the fixture.
    # Runtime directory selection, initialization and config resolution are copied whole.
    generated = ('namespace bridge_util {\n' + arch + '\n}\nusing namespace bridge_util;\n'
                 + contents[names[3]] + '\n' + header + '\n' + impl + '\n'
                 + client + '\nnamespace bridge_util {\n' + config + '\n}\n')
    args.output.mkdir(parents=True, exist_ok=True)
    out = args.output / 'runtime_filesys_generated.h'
    out.write_text(generated, encoding='utf-8')
    receipt = {'variant': args.variant, 'source': str(args.source.resolve()),
               'source_sha256': {name: hashlib.sha256((args.source / name).read_bytes()).hexdigest() for name in names},
               'generated_sha256': hashlib.sha256(out.read_bytes()).hexdigest()}
    (args.output / 'extraction.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
