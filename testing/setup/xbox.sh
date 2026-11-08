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


set -o errexit
source "nzp_utility.sh"

EMULATOR_BIN="${XEMU_BIN:-/opt/xemu/AppRun}"
TIMEOUT=900
XEMU_FILES="${XEMU_FILES:-/workspace/config/xemu}"

function begin_setup()
{
    testing_dir_path="$1"
    binary_path="$2"
    working_dir="$3"
    [[ -f "$binary_path" ]] || print_error 'Supply an Xbox XBE with --binary.' 1
    [[ -x "$EMULATOR_BIN" ]] || print_error "xemu is unavailable at [$EMULATOR_BIN]. Prepare Toolbox's Xbox dependencies first." 1
    if [[ -f "${XEMU_FILES}/bios.rar" ]]; then
        if ! command -v bsdtar >/dev/null 2>&1; then
            apt-get update
            apt-get install -y --no-install-recommends libarchive-tools
        fi
        bsdtar -xOf "${XEMU_FILES}/bios.rar" 'Xbox BIOS/Flash ROM (BIOS)/Complex_4627v1.03.bin' > "${XEMU_FILES}/bios.bin"
        bsdtar -xOf "${XEMU_FILES}/bios.rar" 'Xbox BIOS/MCPX Boot ROM/mcpx_1.0.bin' > "${XEMU_FILES}/mcpx_1.0.bin"
        bsdtar -xOf "${XEMU_FILES}/bios.rar" 'Xbox BIOS/Hard Disk/xbox_hdd.qcow2' > "${XEMU_FILES}/xbox_hdd.qcow2"
        rm -f "${XEMU_FILES}/bios.rar"
    fi
    for required in bios.bin mcpx_1.0.bin xbox_hdd.qcow2; do
        [[ -f "${XEMU_FILES}/${required}" ]] || print_error "Missing xemu input [${XEMU_FILES}/${required}]." 1
    done
    mkdir -p "${working_dir}/nzportable"
    cd "${working_dir}"
    if [[ -z "${ASSETS_OVERRIDE_DIRECTORY:-}" ]]; then
        curl -fL --retry 3 -o nzportable-3ds.zip \
            https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-3ds.zip
        unzip -oq nzportable-3ds.zip
    fi
    apply_content_overrides
    cp "$binary_path" nzportable/default.xbe
    mkdir -p nzportable/nzp/gfx/controller_glyphs
    cp /workspace/repos/assets/common/gfx/controller_glyphs/xbox.tga nzportable/nzp/gfx/controller_glyphs/
    mkdir -p xbox-installer
    cp "${testing_dir_path}/setup/utils/xbox-install.c" xbox-installer/main.c
    printf 'XBE_TITLE = Test Installer\nSRCS = %s/xbox-installer/main.c\nOUTPUT_DIR = %s/xbox-installer/disc\ninclude %s/Makefile\n' \
        "$working_dir" "$working_dir" "$NXDK_DIR" > xbox-installer/Makefile
    make -C xbox-installer -j4
    # Refresh the disc assets once per test run, including content overrides.
    rm -rf xbox-installer/disc/nzp
    cp -a nzportable/nzp xbox-installer/disc/nzp
    rm -f eeprom.bin
    cd "$testing_dir_path"
}

function write_test_setup()
{
    printf '%s\n' "-nosound -nocdaudio $(map_boot_arguments "$1" "${2:-1}")" > "$(test_game_path)/setup.ini"
}

function run_nzportable()
{
    echo "bash ${testing_dir_path}/setup/utils/run-xemu.sh ${TIMEOUT} ${working_dir} ${EMULATOR_BIN} ${XEMU_FILES} $1"
}

function capture_path()
{
    echo "$(test_game_path)/capture.bmp"
}
