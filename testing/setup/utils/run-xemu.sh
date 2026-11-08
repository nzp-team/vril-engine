#!/bin/bash
# Copyright (C) 2026 NZ:P Team
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


set -euo pipefail

timeout_seconds="$1"
working_dir="$2"
emulator="$3"
roms="$4"
require_capture="${5:-1}"

if [[ "${6:-}" != --display ]]; then
    exec timeout --kill-after=10 "$((timeout_seconds + 90))" \
        xvfb-run -a -e "${working_dir}/xvfb.log" -s '-screen 0 1280x960x24 -nolisten tcp' \
        bash "$0" "$timeout_seconds" "$working_dir" "$emulator" "$roms" "$require_capture" --display
fi

cp "${roms}/xbox_hdd.qcow2" "${working_dir}/xbox_hdd.qcow2"
game="${working_dir}/nzportable"
disc="${working_dir}/xbox-installer/disc"
rm -f "${game}/capture.bmp" "${game}/nzp/condebug.log" "${working_dir}/xbox-test.iso"
cp "${game}/default.xbe" "${disc}/vril.xbe"
cp "${game}/setup.ini" "${disc}/setup.ini"
"${NXDK_DIR}/tools/extract-xiso/build/extract-xiso" -c "$disc" "${working_dir}/xbox-test.iso"
export SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1
export LP_NUM_THREADS=2
python3 -B "$(dirname "$0")/xemu-test.py" "$timeout_seconds" "$working_dir" "$emulator" "$roms" "$require_capture"
