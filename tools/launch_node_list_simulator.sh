#!/usr/bin/env bash
set -euo pipefail

if [[ -z ${DISPLAY:-} ]]; then
	echo "DISPLAY is required to launch the legacy X11 simulator" >&2
	exit 1
fi

build_dir="${1:-build-task1-x11-simulator}"
cmake -S . -B "${build_dir}" -DENABLE_MUI_X11_SIMULATOR=ON -DENABLE_DOCTESTS=ON
cmake --build "${build_dir}" --target mui_node_list_simulator
"${build_dir}/bin/mui_node_list_simulator" --implementation legacy
