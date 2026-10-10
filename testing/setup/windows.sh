#!/bin/bash

set -o errexit

source "nzp_utility.sh"

APP_BIN="nzportable.exe"
TIMEOUT=180

testing_dir_path="${testing_dir_path:-}"
binary_path="${binary_path:-}"
working_dir="${working_dir:-}"

export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true

function obtain_nzportable()
{
	print_info "Obtaining NZ:P content.."
	cd "${working_dir}"
	rm -f nzportable-3ds.zip
	wget -q https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-3ds.zip
	unzip -oq nzportable-3ds.zip

	if [[ -z "${binary_path}" || ! -f "${binary_path}" ]]; then
		print_error "A Windows engine binary must be supplied with --binary." "1"
	fi

	cp "${binary_path}" "${working_dir}/nzportable/${APP_BIN}"
	chmod +x "${working_dir}/nzportable/${APP_BIN}"
}

function begin_setup()
{
	testing_dir_path="${1}"
	binary_path="${2}"
	working_dir="${3}"

	mkdir -p "${working_dir}"
	if ! command -v wine >/dev/null; then
		print_error "Toolbox Wine is unavailable!" "1"
	fi
	obtain_nzportable
	apply_content_overrides
	cd "${testing_dir_path}"
	print_info "Done setting up Windows testing!"
}

function run_nzportable()
{
    echo "env --chdir=${working_dir}/nzportable \
        WINEPREFIX=${working_dir}/wineprefix \
        SDL_AUDIODRIVER=dummy \
        LIBGL_ALWAYS_SOFTWARE=1 \
        xvfb-run -a timeout ${TIMEOUT} \
        wine ./${APP_BIN} \
        -basedir ${working_dir}/nzportable \
        -condebug \
        -nosound \
        -nocdaudio"
}

function capture_path()
{
	echo "${working_dir}/nzportable/capture.bmp"
}

function write_test_setup()
{
	local map_name="${1}"
	local test_mode="${2:-1}"
	printf '%s\n' "$(map_boot_arguments "${map_name}" "${test_mode}") +vid_width 320 +vid_height 240" > "${working_dir}/nzportable/setup.ini"
}
