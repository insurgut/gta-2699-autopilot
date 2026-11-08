#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"
cxx=${APIL_CXX:-x86_64-w64-mingw32-g++}
windres=${APIL_WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p bin dist
flags=(-std=c++17 -O2 -Wall -Wextra -Werror -static -s -municode -mwindows -DUNICODE -D_UNICODE)
"$cxx" "${flags[@]}" src/external_reader.cpp -o bin/DistrictTelemetry.exe -luser32
"$windres" src/apil.rc -O coff -o bin/apil-resource.o
"$cxx" "${flags[@]}" -Wno-cast-function-type src/controller.cpp bin/apil-resource.o -o dist/APIL.exe \
  -luser32 -lgdi32 -lshell32 -lole32 -lshlwapi -luuid -lwinhttp -lbcrypt
python3 - <<'PY'
from hashlib import sha256
from pathlib import Path
import re
root = Path('dist')
binary = (root / 'APIL.exe').read_bytes()
version = re.search(r'apilVersion\[\]="([0-9.]+)"', Path('src/apil_release.hpp').read_text()).group(1)
(root / 'update.txt').write_text(f'{version} {len(binary)} {sha256(binary).hexdigest()}\n', encoding='ascii')
PY
