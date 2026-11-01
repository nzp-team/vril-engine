#!/bin/bash
set -o errexit

destination="${1:-/opt/pcsx2}"
source_directory="$(cd "$(dirname "$0")" && pwd)"
if [[ -x "${destination}/AppRun" ]]; then
	cc -shared -fPIC -O2 -Wall -Wextra -Werror "${source_directory}/pcsx2-usb-sync.c" -ldl -o "${destination}/usb-sync.so"
	exit 0
fi
apt-get update -qq
apt-get install -y --no-install-recommends mtools dosfstools libaio1t64
archive="$(mktemp)"
trap 'rm -f "${archive}"' EXIT
curl -fL --retry 3 -o "${archive}" https://github.com/PCSX2/pcsx2/releases/download/v2.4.0/pcsx2-v2.4.0-linux-appimage-x64-Qt.AppImage
echo "2d5061f4f2906f22befc719aac3acebd2699fb07c611bc90709780e28dea8c8e  ${archive}" | sha256sum -c -
offset="$(python3 - "${archive}" <<'PY'
import struct
import sys
with open(sys.argv[1], 'rb') as source:
    header = source.read(64)
assert header[:6] == b'\x7fELF\x02\x01'
print(struct.unpack_from('<Q', header, 40)[0] + struct.unpack_from('<H', header, 58)[0] * struct.unpack_from('<H', header, 60)[0])
PY
)"
unsquashfs -no-progress -offset "${offset}" -d "${destination}" "${archive}"

cc -shared -fPIC -O2 -Wall -Wextra -Werror "${source_directory}/pcsx2-usb-sync.c" -ldl -o "${destination}/usb-sync.so"
