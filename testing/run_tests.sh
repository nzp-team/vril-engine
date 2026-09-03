#!/bin/bash
#
# Nazi Zombies: Portable
# Main test runner.
# ----
# Kicks off setup, test, then validation scripts.
#
# This is intended to be used via a Docker 
# container running ubuntu:24.04.
#
set -o errexit

source "nzp_utility.sh"

#
# print_help
# ---
# Shows how to invoke test script.
#
function print_help()
{
    echo "Usage: run_tests.sh --platform psp --test map-boot --content /path/to/validate/"
    echo "-p (--platform) : Platform to run tests for. Looks in setup directory."
    echo "-t (--test)     : Test to run. Use \"all\" to run all tests. Looks in tests directory."
    echo "-c (--content)  : Path to validate directory for content comparison."
    echo "-b (--binary)   : Optional path to Vril engine binary to test under instead of nightly."
    echo "-g (--generate) : Generates master bitmaps of all maps to compare against."
    echo "-w (--working-dir) : Working directory for testing (persistent if mounted). Defaults to /working."
    echo "-m (--mode)     : Extra mode flag to pass into test scripts."
    echo "-h (--help)     : Displays this message."
}

while true; do
    case "$1" in
        -p | --platform ) PLATFORM="$2"; shift 2 ;;
        -t | --test ) TEST="$2"; shift 2 ;;
        -b | --binary ) BINARY_PATH="$2"; shift 2 ;;
        -g | --generate ) TEST="generate-source"; shift 1 ;;
        -c | --content ) CONTENT_DIR="$2"; shift 2 ;;
        -w | --working-dir ) WORKING_DIR="$2"; shift 2 ;;
        -r | --reference-platform ) REFERENCE_PLATFORM="$2"; shift 2 ;;
        -m | --mode ) MODE="$2"; shift 2 ;;
        -h | --help ) print_help; exit 0 ;;
        -- ) shift; break ;;
        * ) break ;;
    esac
done

#
# validate_arguments
# ---
# Verifies arguments were set and
# the requirements are satisfiable.
#
function validate_arguments()
{
    # Check if --platform was set
    if [[ -z "${PLATFORM}" ]]; then
        print_error "Platform was not specified!" "1"
    fi

    # Check if platform is valid
    if [[ ! -f "./setup/${PLATFORM}.sh" ]]; then
        print_error "Could not find setup script for platform [${PLATFORM}]!" "1"
    fi

    # Check if --test was set
    if [[ -z "${TEST}" ]]; then
        print_error "Test to run was not specified!" "1"
    fi

    # Check if test is valid
    if [[ ! -f "./tests/${TEST}.sh" ]] && [[ "${TEST}" != "all" ]]; then
        print_error "Could not find test script [${TEST}]!" "1"
    fi
}

#
# load_setup_script
# ---
# Loads specified platform's setup script.
#
function load_setup_script()
{
    source "./setup/${PLATFORM}.sh"
}

#
# run_test
# ---
# Executes desired (or all) tests in package.
#
function run_test()
{
    if [[ "${TEST}" == "all" ]]; then
        local any_test_failed=0
        local combined_summary="${WORKING_DIR}/fail/summary-all.md"
        local summary="${WORKING_DIR}/fail/summary.md"

        rm -f "${combined_summary}"
        for test_script in tests/*.sh; do
            local pretty_sh=$(basename ${test_script} .sh) 

            # Source generating script should not be ran automatically.
            if [[ "${test_script}" == "tests/generate-source.sh" ]]; then
                continue
            fi

            rm -f "${summary}"
            echo "[TEST]: Running ${pretty_sh}"

            set +o errexit
            (
                set -o errexit
                source "./tests/${pretty_sh}.sh" "${PLATFORM}" "${CONTENT_DIR}" "${MODE}" "${WORKING_DIR}" "${REFERENCE_PLATFORM}"
            )
            local test_status=$?
            set -o errexit

            if [[ "${test_status}" -ne 0 ]]; then
                any_test_failed=1
                mkdir -p "${WORKING_DIR}/fail"
                if [[ -f "${summary}" ]]; then
                    cat "${summary}" >> "${combined_summary}"
                    printf '\n' >> "${combined_summary}"
                else
                    printf '## %s\n\n- Test exited with status %i.\n\n' \
                        "${pretty_sh}" "${test_status}" >> "${combined_summary}"
                fi
            fi
        done

        if [[ "${any_test_failed}" -ne 0 ]]; then
            mv "${combined_summary}" "${summary}"
            return 1
        fi

        rm -f "${combined_summary}" "${summary}"
    else
        echo "$(pwd)"
        source "./tests/${TEST}.sh" "${PLATFORM}" "${CONTENT_DIR}" "${MODE}" "${WORKING_DIR}" "${REFERENCE_PLATFORM}"
    fi
}

function write_failure_summary()
{
    local test_exit=$?
    trap - EXIT

    if (( test_exit != 0 )) && [[ ! -f "${WORKING_DIR}/fail/summary.md" ]]; then
        mkdir -p "${WORKING_DIR}/fail"
        printf "## Test harness failure\n\nThe test run exited before detailed test results were available. Check the failed step log for the underlying error.\n" > "${WORKING_DIR}/fail/summary.md"
    fi

    exit "${test_exit}"
}

function main()
{
    # Confirm everything is in order.
    validate_arguments;

    print_info "Preparing environment for testing platform [${PLATFORM}].."

    # Begin prepping our environment.
    local dir=$(pwd)

    # Default validation content if not set
    if [[ -z "${CONTENT_DIR}" ]]; then
        CONTENT_DIR="${dir}/validate"
    fi

    # Default reference platform to execute platform
    if [[ -z "${REFERENCE_PLATFORM}" ]]; then
        REFERENCE_PLATFORM="${PLATFORM}"
    fi
    
    # Default working dir if not set
    if [[ -z "${WORKING_DIR}" ]]; then
        WORKING_DIR="/working"
    fi

    # Ensure WORKING_DIR is absolute
    if [[ "${WORKING_DIR}" != /* ]]; then
        WORKING_DIR="$(pwd)/${WORKING_DIR}"
    fi

    # Remove trailing slash
    WORKING_DIR="${WORKING_DIR%/}"

    # Ensure every failed test run leaves a summary that CI can publish, even
    # when setup fails before an individual test can report its own details.
    trap write_failure_summary EXIT

    load_setup_script;
    begin_setup "${dir}" "${BINARY_PATH}" "${WORKING_DIR}";
    cd "${dir}"

    # Run our tests.
    run_test;
}

main;
