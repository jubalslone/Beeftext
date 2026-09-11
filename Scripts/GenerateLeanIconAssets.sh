#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/.." && pwd)
icon_root="$repo_root/Beeftext/Resources/Icons"
app_dir="$icon_root/App"
tray_dir="$icon_root/Tray"
source_dir="$icon_root/Sources"

master_source="$source_dir/LeanBeeftextMaster.png"
tray_32_source="$source_dir/LeanBeeftextTray32Source.png"
tray_16_source="$source_dir/LeanBeeftextTray16Source.png"

command -v magick >/dev/null
mkdir -p "$app_dir" "$tray_dir"

render_full_icon() {
	local size=$1
	local output="$app_dir/LeanBeeftextApp-${size}.png"

	magick "$master_source" \
		-strip -alpha on -colorspace sRGB \
		-filter Lanczos -resize "${size}x${size}!" \
		-define png:color-type=6 \
		-define png:compression-level=9 \
		-define png:exclude-chunk=date,time \
		"$output"
}

render_tray_32_icon() {
	magick "$tray_32_source" \
		-strip -alpha on -colorspace sRGB \
		-trim +repage -filter Lanczos -resize '32x32' \
		-gravity center -background none -extent '32x32' \
		-define png:color-type=6 \
		-define png:compression-level=9 \
		-define png:exclude-chunk=date,time \
		"$tray_dir/LeanBeeftextTray-32.png"
}

render_tray_16_icon() {
	magick "$tray_16_source" \
		-strip -alpha on -colorspace sRGB \
		-filter Lanczos -resize '16x16!' \
		-define png:color-type=6 \
		-define png:compression-level=9 \
		-define png:exclude-chunk=date,time \
		"$tray_dir/LeanBeeftextTray-16.png"
}

render_paused_icon() {
	local input=$1
	local output=$2

	magick "$input" \
		-strip -alpha on -colorspace Gray -colorspace sRGB \
		-define png:color-type=6 \
		-define png:compression-level=9 \
		-define png:exclude-chunk=date,time \
		"$output"
}

app_sizes=(20 24 32 48 64 128 256)
for size in "${app_sizes[@]}"; do
	render_full_icon "$size"
	render_paused_icon \
		"$app_dir/LeanBeeftextApp-${size}.png" \
		"$app_dir/LeanBeeftextAppPaused-${size}.png"
done

render_tray_32_icon
render_tray_16_icon
render_paused_icon \
	"$tray_dir/LeanBeeftextTray-32.png" \
	"$tray_dir/LeanBeeftextTrayPaused-32.png"
render_paused_icon \
	"$tray_dir/LeanBeeftextTray-16.png" \
	"$tray_dir/LeanBeeftextTrayPaused-16.png"

magick -quiet \
	"$tray_dir/LeanBeeftextTray-16.png" \
	"$app_dir/LeanBeeftextApp-20.png" \
	"$app_dir/LeanBeeftextApp-24.png" \
	"$app_dir/LeanBeeftextApp-32.png" \
	"$app_dir/LeanBeeftextApp-48.png" \
	"$app_dir/LeanBeeftextApp-64.png" \
	"$app_dir/LeanBeeftextApp-128.png" \
	"$app_dir/LeanBeeftextApp-256.png" \
	"$icon_root/LeanBeeftextApp.ico"

magick -quiet \
	"$tray_dir/LeanBeeftextTrayPaused-16.png" \
	"$app_dir/LeanBeeftextAppPaused-20.png" \
	"$app_dir/LeanBeeftextAppPaused-24.png" \
	"$app_dir/LeanBeeftextAppPaused-32.png" \
	"$app_dir/LeanBeeftextAppPaused-48.png" \
	"$app_dir/LeanBeeftextAppPaused-64.png" \
	"$app_dir/LeanBeeftextAppPaused-128.png" \
	"$app_dir/LeanBeeftextAppPaused-256.png" \
	"$icon_root/LeanBeeftextAppPaused.ico"

magick -quiet \
	"$tray_dir/LeanBeeftextTray-16.png" \
	"$tray_dir/LeanBeeftextTray-32.png" \
	"$icon_root/LeanBeeftextTray.ico"

magick -quiet \
	"$tray_dir/LeanBeeftextTrayPaused-16.png" \
	"$tray_dir/LeanBeeftextTrayPaused-32.png" \
	"$icon_root/LeanBeeftextTrayPaused.ico"

asset_files=(
	ASSET_MANIFEST.md
	Sources/LeanBeeftextMaster.png
	Sources/LeanBeeftextTray16Source.png
	Sources/LeanBeeftextTray32Source.png
	App/LeanBeeftextApp-20.png
	App/LeanBeeftextApp-24.png
	App/LeanBeeftextApp-32.png
	App/LeanBeeftextApp-48.png
	App/LeanBeeftextApp-64.png
	App/LeanBeeftextApp-128.png
	App/LeanBeeftextApp-256.png
	App/LeanBeeftextAppPaused-20.png
	App/LeanBeeftextAppPaused-24.png
	App/LeanBeeftextAppPaused-32.png
	App/LeanBeeftextAppPaused-48.png
	App/LeanBeeftextAppPaused-64.png
	App/LeanBeeftextAppPaused-128.png
	App/LeanBeeftextAppPaused-256.png
	Tray/LeanBeeftextTray-16.png
	Tray/LeanBeeftextTray-32.png
	Tray/LeanBeeftextTrayPaused-16.png
	Tray/LeanBeeftextTrayPaused-32.png
	LeanBeeftextApp.ico
	LeanBeeftextAppPaused.ico
	LeanBeeftextTray.ico
	LeanBeeftextTrayPaused.ico
)

(
	cd "$icon_root"
	sha256sum "${asset_files[@]}" > SHA256SUMS.txt
)

verify_png() {
	local file=$1
	local expected_size=$2
	local actual_size
	local opaque

	actual_size=$(identify -format '%wx%h' "$file")
	opaque=$(identify -format '%[opaque]' "$file")
	if [[ "$actual_size" != "${expected_size}x${expected_size}" ]]; then
		printf 'Unexpected PNG dimensions for %s: %s\n' "$file" "$actual_size" >&2
		exit 1
	fi
	if [[ "$opaque" != 'False' ]]; then
		printf 'PNG does not retain transparency: %s\n' "$file" >&2
		exit 1
	fi
}

for size in "${app_sizes[@]}"; do
	verify_png "$app_dir/LeanBeeftextApp-${size}.png" "$size"
	verify_png "$app_dir/LeanBeeftextAppPaused-${size}.png" "$size"
done
verify_png "$tray_dir/LeanBeeftextTray-32.png" 32
verify_png "$tray_dir/LeanBeeftextTray-16.png" 16
verify_png "$tray_dir/LeanBeeftextTrayPaused-32.png" 32
verify_png "$tray_dir/LeanBeeftextTrayPaused-16.png" 16

verify_ico_frames() {
	local file=$1
	local expected=$2
	local actual

	actual=$(identify -format '%wx%h\n' "$file")
	if [[ "$actual" != "$expected" ]]; then
		printf 'Unexpected ICO frames for %s:\n%s\n' "$file" "$actual" >&2
		exit 1
	fi
}

verify_ico_frames "$icon_root/LeanBeeftextApp.ico" $'16x16\n20x20\n24x24\n32x32\n48x48\n64x64\n128x128\n256x256'
verify_ico_frames "$icon_root/LeanBeeftextAppPaused.ico" $'16x16\n20x20\n24x24\n32x32\n48x48\n64x64\n128x128\n256x256'
verify_ico_frames "$icon_root/LeanBeeftextTray.ico" $'16x16\n32x32'
verify_ico_frames "$icon_root/LeanBeeftextTrayPaused.ico" $'16x16\n32x32'

(
	cd "$icon_root"
	sha256sum -c SHA256SUMS.txt
)
