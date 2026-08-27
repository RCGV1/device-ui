#!/usr/bin/env bash
set -euo pipefail

if [[ -z ${DISPLAY:-} ]]; then
	echo "DISPLAY is required to launch the X11 simulator" >&2
	exit 1
fi

build_dir="${1:-build-task1-x11-simulator}"
if [[ $# -gt 0 && $1 != --* ]]; then
	shift
fi

nodes=100
seed=42
run_for_ms=0
while [[ $# -gt 0 ]]; do
	case "$1" in
	--nodes)
		nodes="${2:?missing value for --nodes}"
		shift 2
		;;
	--seed)
		seed="${2:?missing value for --seed}"
		shift 2
		;;
	--run-for-ms)
		run_for_ms="${2:?missing value for --run-for-ms}"
		shift 2
		;;
	*)
		echo "usage: $0 [build-dir] [--nodes N] [--seed N] [--run-for-ms N]" >&2
		exit 2
		;;
	esac
done

cmake -S . -B "${build_dir}" -DENABLE_MUI_X11_SIMULATOR=ON -DENABLE_DOCTESTS=ON
cmake --build "${build_dir}" --target mui_node_list_simulator

run_args=(--nodes "${nodes}" --seed "${seed}")
if [[ ${run_for_ms} != 0 ]]; then
	run_args+=(--run-for-ms "${run_for_ms}")
fi

"${build_dir}/bin/mui_node_list_simulator" --implementation production "${run_args[@]}"
