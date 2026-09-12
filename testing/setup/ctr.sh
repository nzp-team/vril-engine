#!/bin/bash
#
# Nazi Zombies: Portable
# Nintendo 3DS test code init
# ----
# Prepares a testing environment targeting
# Nintendo 3DS builds.
#
# This is intended to be used via a Docker 
# container running ubuntu:24.04.
#
set -o errexit

source "nzp_utility.sh"

# Read by our test scripts.
EMULATOR_BIN="azahar/AppRun"
APP_BIN="nzportable.3dsx"

# How many seconds to wait before time out
TIMEOUT=240

# The Azahar release we download and use.
azahar_version="2126.0"

testing_dir_path="${testing_dir_path:-}"
binary_path="${binary_path:-}"
working_dir="${working_dir:-}"

# tzdata will try to display an interactive install prompt by
# default, so make sure we define our system as non-interactive.
export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true

function install_dependencies
{
	print_info "Installing Nintendo 3DS dependencies.."
	apt-get update -y
	apt-get install -y ffmpeg libegl1 libfontconfig1 libgl1 libgl1-mesa-dri \
		libglu1-mesa libgtk-3-0 libxcb-cursor0 libxkbcommon-x11-0 \
		python3 squashfs-tools unzip wget xauth xvfb
}

function download_azahar
{
	local image_path="${working_dir}/azahar.AppImage"
	local image_offset

	if [[ ! -f "${image_path}" ]]; then
		print_info "Downloading Azahar [${azahar_version}].."
		wget -q -O "${image_path}.tmp" \
			"https://github.com/azahar-emu/azahar/releases/download/${azahar_version}/azahar.AppImage"
		mv "${image_path}.tmp" "${image_path}"
	fi

	# Evil AppImage binary extraction hack, Python edition
	image_offset=$(python3 - "${image_path}" <<-'PYTHON'
		import struct
		import sys
		with open(sys.argv[1], "rb") as image:
		    header = image.read(64)
		assert header[:6] == b"\x7fELF\x02\x01", "Expected a 64-bit little-endian AppImage"
		print(struct.unpack_from("<Q", header, 40)[0] +
		      struct.unpack_from("<H", header, 58)[0] * struct.unpack_from("<H", header, 60)[0])
	PYTHON
	)
	unsquashfs -no-progress -offset "${image_offset}" -d "${working_dir}/azahar" "${image_path}"
}

function obtain_nzportable
{
	print_info "Obtaining latest Nazi Zombies: Portable 3DS release.."
	cd "${working_dir}"
	rm -f nzportable-3ds.zip
	wget -q https://github.com/nzp-team/nzportable/releases/download/nightly/nzportable-3ds.zip
	unzip -oq nzportable-3ds.zip

	if [ -n "${binary_path}" ]; then
		if [[ ! -f "${binary_path}" ]]; then
			print_error "3DS binary does not exist: [${binary_path}]" "1"
		fi
		print_info "Replacing nightly binary with user-specified: [${binary_path}]"
		cp "${binary_path}" "${working_dir}/nzportable/${APP_BIN}"
	fi
}

function begin_setup()
{
	testing_dir_path="${1}"
	binary_path="${2}"
	working_dir="${3}"

	# Create our working directory.
	if [[ ! -d "${working_dir}" ]]; then
		mkdir -p "${working_dir}"
	fi

	MODE="$(ctr_mode "${MODE}")"
	cd "${working_dir}"

	install_dependencies;
	
	# Check if we have the emulator already
	if [[ -f "${working_dir}/${EMULATOR_BIN}" ]]; then
		print_info "Emulator binary found in [${working_dir}], skipping download.."
	else
		download_azahar;
	fi

	obtain_nzportable;
	configure_azahar "${MODE}"

	print_info "Done setting up for Nintendo 3DS testing!"
}

function ctr_mode()
{
	case "${1:-new}" in
		new) echo "new" ;;
		old) echo "old" ;;
		*)
			print_error "3DS mode must be old or new (received: ${1})." "1"
			;;
	esac
}

function configure_azahar()
{
	local mode
	mode="$(ctr_mode "${1}")"
	local is_new_3ds=false
	if [[ "${mode}" == "new" ]]; then
		is_new_3ds=true
	fi

	export XDG_CONFIG_HOME="${working_dir}/azahar-${mode}/config"
	export XDG_DATA_HOME="${working_dir}/azahar-${mode}/data"
	mkdir -p "${XDG_CONFIG_HOME}/azahar-emu" "$(runtime_path "${mode}")"
	printf '%s\n' \
		'[System]' \
		"is_new_3ds=${is_new_3ds}" \
		'is_new_3ds/default=false' \
		'' \
		'[Renderer]' \
		'graphics_api=1' \
		'graphics_api/default=false' \
		'async_shader_compilation=false' \
		'async_shader_compilation/default=false' \
		'use_hw_shader=true' \
		'use_hw_shader/default=false' \
		'' \
		'[Miscellaneous]' \
		'check_for_update_on_start=false' \
		'check_for_update_on_start/default=false' \
		> "${XDG_CONFIG_HOME}/azahar-emu/qt-config.ini"

	cp -a "${working_dir}/nzportable/." "$(runtime_path "${mode}")/"
}

function runtime_path()
{
	local mode
	mode="$(ctr_mode "${1:-${MODE:-new}}")"
	echo "${working_dir}/azahar-${mode}/data/azahar-emu/sdmc/3ds/nzportable"
}

function test_game_path()
{
	runtime_path "${MODE:-new}"
}

#
# run_nzportable
# ---
# Returns command used to run game via emulator.
#
function run_nzportable()
{
	echo "env XDG_CONFIG_HOME=${XDG_CONFIG_HOME} XDG_DATA_HOME=${XDG_DATA_HOME} SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 bash ${testing_dir_path}/setup/utils/run-azahar.sh ${TIMEOUT} $(test_game_path)/nzp/condebug.log ${working_dir}/nzportable ${working_dir}/${EMULATOR_BIN} ${APP_BIN}"
}

function write_test_setup()
{
	local map_name="${1}"
	local test_mode="${2:-1}"
	printf '%s\n' "$(map_boot_arguments "${map_name}" "${test_mode}") -nosound -nocdaudio" > "$(test_game_path)/setup.ini"
}

function capture_path()
{
	local mode
	mode="$(ctr_mode "${MODE:-new}")"
	echo "$(runtime_path "${mode}")/capture-${mode}.bmp"
}
