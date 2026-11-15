#!/bin/bash
#
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
#
set -o errexit
source "nzp_utility.sh"

EMULATOR_BIN="${RPCS3_BIN:-/opt/rpcs3/AppRun}"
TIMEOUT=600

function begin_setup()
{
    testing_dir_path="$1"
    binary_path="$(realpath "$2")"
    working_dir="$3"
    local extractor="${PS3DEV:-/usr/local/ps3dev}/bin/pkg"
    local install="${working_dir}/rpcs3/dev_hdd0/game/NZPT00001"
    local package

    [[ -x "${EMULATOR_BIN}" ]] || print_error 'Use a Toolbox image with RPCS3 support.' 1
    [[ -f "${binary_path}" ]] || print_error 'Supply the PS3 package or EBOOT.BIN with --binary.' 1

    mkdir -p "${working_dir}"
    cd "${working_dir}"

    if [[ ! -f nzportable-3ds.zip ]]; then
        curl -fL --retry 3 --connect-timeout 15 --max-time 300 \
            https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-3ds.zip \
            -o nzportable-3ds.zip
    fi

    unzip -oq nzportable-3ds.zip
    apply_content_overrides
    compgen -G "${working_dir}/nzportable/nzp/maps/*.bsp" > /dev/null

    mkdir -p "${install}/USRDIR"
    rm -rf "${install}/USRDIR/nzportable"

    if [[ "${binary_path##*.}" == pkg ]]; then
        [[ -x "${extractor}" ]] || print_error 'The Toolbox PS3 package tool is unavailable.' 1
        rm -rf "${working_dir}/ps3-package"
        mkdir -p "${working_dir}/ps3-package"
        cd "${working_dir}/ps3-package"
        "${extractor}" --extract "${binary_path}"
        package="$(find . -type f -path '*/USRDIR/EBOOT.BIN' -print -quit)"
        [[ -n "${package}" ]] || print_error 'The package contains no PS3 executable.' 1
        cp -a "$(dirname "$(dirname "${package}")")/." "${install}/"
    else
        cp "${binary_path}" "${install}/USRDIR/EBOOT.BIN"
        if [[ -f "$(dirname "${binary_path}")/../PARAM.SFO" ]]; then
            cp "$(dirname "${binary_path}")/../PARAM.SFO" "${install}/PARAM.SFO"
        fi
        cp -a "$(dirname "${binary_path}")/shaders" "${install}/USRDIR/"
    fi

    [[ -s "${install}/USRDIR/shaders/hyena.vpo" ]] || print_error 'The PS3 shaders are missing.' 1
    rm -rf "${install}/USRDIR/nzportable"
    ln -s "${working_dir}/nzportable" "${install}/USRDIR/nzportable"

    printf '\nshow_fps "0"\nbrightness "0.5"\nr_vsync "0"\n' >> "${working_dir}/nzportable/nzp/config.cfg"
    python3 "${testing_dir_path}/setup/utils/run-rpcs3.py" prepare "${TIMEOUT}" "${working_dir}" "${EMULATOR_BIN}"
    cd "${testing_dir_path}"
}

function write_test_setup()
{
    printf '%s\n' "$(map_boot_arguments "$1" "${2:-1}") -nosound -nocdaudio" > "$(test_game_path)/setup.ini"
}

function run_nzportable()
{
    echo "python3 ${testing_dir_path}/setup/utils/run-rpcs3.py run ${TIMEOUT} ${working_dir} ${EMULATOR_BIN}"
}

function capture_path()
{
    echo "$(test_game_path)/capture.bmp"
}
