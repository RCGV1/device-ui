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

process_mode="legacy"
nodes=100
seed=42
run_for_ms=0
while [[ $# -gt 0 ]]; do
	case "$1" in
	--process-mode)
		process_mode="${2:?missing value for --process-mode}"
		shift 2
		;;
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
		echo "usage: $0 [build-dir] [--process-mode legacy|virtual_candidate|pair] [--nodes N] [--seed N] [--run-for-ms N]" >&2
		exit 2
		;;
	esac
done

case "${process_mode}" in
legacy) virtual_gate=OFF ;;
virtual_candidate | pair) virtual_gate=ON ;;
*)
	echo "usage: $0 [build-dir] [--process-mode legacy|virtual_candidate|pair] [--nodes N] [--seed N] [--run-for-ms N]" >&2
	exit 2
	;;
esac

cmake -S . -B "${build_dir}" -DENABLE_MUI_X11_SIMULATOR=ON -DENABLE_DOCTESTS=ON -DENABLE_MUI_VIRTUAL_NODE_LIST="${virtual_gate}"
cmake --build "${build_dir}" --target mui_node_list_simulator

run_args=(--nodes "${nodes}" --seed "${seed}")
if [[ ${run_for_ms} != 0 ]]; then
	run_args+=(--run-for-ms "${run_for_ms}")
fi

child_pids=()
terminate_children() {
	for pid in "${child_pids[@]}"; do
		kill "${pid}" >/dev/null 2>&1 || true
	done
	for pid in "${child_pids[@]}"; do
		wait "${pid}" >/dev/null 2>&1 || true
	done
}
cleanup_children() {
	local status=$?
	trap - EXIT INT TERM
	terminate_children
	exit "${status}"
}
trap cleanup_children EXIT INT TERM

pid_is_running_job() {
	local pid="$1"
	local running_pid
	while IFS= read -r running_pid; do
		[[ ${running_pid} == "${pid}" ]] && return 0
	done < <(jobs -pr)
	return 1
}

wait_for_pair_children() {
	local remaining=${#child_pids[@]}
	local status=0
	local child_status=0
	local index
	local pid
	while [[ ${remaining} -gt 0 ]]; do
		for index in "${!child_pids[@]}"; do
			pid="${child_pids[index]}"
			[[ -z ${pid} ]] && continue
			if pid_is_running_job "${pid}"; then
				continue
			fi
			if wait "${pid}"; then
				child_status=0
			else
				child_status=$?
			fi
			child_pids[index]=
			remaining=$((remaining - 1))
			if [[ ${child_status} -ne 0 ]]; then
				status=${child_status}
				break 2
			fi
		done
		if [[ ${remaining} -gt 0 ]]; then
			sleep 0.05
		fi
	done
	return "${status}"
}

case "${process_mode}" in
legacy)
	"${build_dir}/bin/mui_node_list_simulator" --implementation legacy "${run_args[@]}"
	;;
virtual_candidate)
	"${build_dir}/bin/mui_node_list_simulator" --implementation virtual_candidate "${run_args[@]}"
	;;
pair)
	"${build_dir}/bin/mui_node_list_simulator" --implementation legacy "${run_args[@]}" &
	child_pids+=("$!")
	"${build_dir}/bin/mui_node_list_simulator" --implementation virtual_candidate "${run_args[@]}" &
	child_pids+=("$!")
	status=0
	wait_for_pair_children || status=$?
	if [[ ${status} -ne 0 ]]; then
		trap - EXIT INT TERM
		terminate_children
		exit "${status}"
	fi
	child_pids=()
	exit 0
	;;
esac
