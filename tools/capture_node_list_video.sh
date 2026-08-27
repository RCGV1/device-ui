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
production_dir="$output_dir/production"

mkdir -p "$production_dir"
"$video_executable" --implementation production --nodes "$nodes" --seed "$seed" --output-frames "$frames" --output-dir "$production_dir"

production_count=$(find "$production_dir" -name 'frame_*.ppm' -type f | wc -l | tr -d ' ')
if [ "$production_count" -ne "$frames" ]; then
	echo "frame count mismatch: production=$production_count expected=$frames" >&2
	exit 1
fi

ffmpeg -y -framerate 30 -i "$production_dir/frame_%04d.ppm" -c:v libx264 -pix_fmt yuv420p "$output_mp4"
