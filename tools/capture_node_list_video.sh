#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
	echo "usage: $0 NODE_LIST_VIDEO OUTPUT_DIR NODES SEED FRAMES OUTPUT_MP4" >&2
	exit 2
fi

video_executable=$1
output_dir=$2
nodes=$3
seed=$4
frames=$5
output_mp4=$6
legacy_dir="$output_dir/legacy"
candidate_dir="$output_dir/virtual_candidate"

mkdir -p "$legacy_dir" "$candidate_dir"
"$video_executable" --implementation legacy --nodes "$nodes" --seed "$seed" --output-frames "$frames" --output-dir "$legacy_dir"
"$video_executable" --implementation virtual_candidate --nodes "$nodes" --seed "$seed" --output-frames "$frames" --output-dir "$candidate_dir"

legacy_count=$(find "$legacy_dir" -name 'frame_*.ppm' -type f | wc -l | tr -d ' ')
candidate_count=$(find "$candidate_dir" -name 'frame_*.ppm' -type f | wc -l | tr -d ' ')
if [ "$legacy_count" -ne "$candidate_count" ] || [ "$legacy_count" -ne "$frames" ]; then
	echo "frame count mismatch: legacy=$legacy_count virtual_candidate=$candidate_count expected=$frames" >&2
	exit 1
fi

ffmpeg -y -framerate 30 -i "$legacy_dir/frame_%04d.ppm" -framerate 30 -i "$candidate_dir/frame_%04d.ppm" \
	-filter_complex hstack=inputs=2 -c:v libx264 -pix_fmt yuv420p "$output_mp4"
