"""Add focused reply regression dispatch to the existing production extractor."""
import argparse
from pathlib import Path
import shutil
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from generate_ipc_full_test import generate

parser = argparse.ArgumentParser()
parser.add_argument('--source', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
generate(args.source.resolve(), args.output.resolve())
shutil.copy2(ROOT / 'tests/optimization45_ipc_reply_cases.h', args.output / 'optimization45_ipc_reply_cases.h')
path = args.output / 'full_bridge_cases.cpp'
text = path.read_text()
text = text.replace('int wmain(', '#include "optimization45_ipc_reply_cases.h"\nint wmain(', 1)
needle = '    else {transfer<Device>();transfer<Module>();}'
assert needle in text
text = text.replace(needle, '''    else if(std::wstring(argv[2])==L"opt45-reply-delay") {optimization45Replies<Device>(false);}
    else if(std::wstring(argv[2])==L"opt45-reply-abandon") {optimization45Replies<Device>(true);}
''' + needle, 1)
path.write_text(text)
