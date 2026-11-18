#!/bin/sh
# Install Brainux boot files into the FAT (first) partition of a
# Brainux SD card image, using mtools (no mount needed).
#
# Usage:
#   install-sd.sh <sd.img> rtc
#       Install only the RTC-enabled device tree
#       (prebuilt/imx28-pwsh6-rtc-only.dtb).  Works with the stock
#       Brainux kernel: the stmp3xxx RTC driver is already built in,
#       only the device tree node was disabled.
#
#   install-sd.sh <sd.img> full <zImage> <imx28-pwsh6.dtb>
#       Install a kernel built by scripts/build-kernel.sh plus its
#       device tree (RTC + BU26154 sound).
#
# The first partition of a Brainux SD image starts at LBA 2048
# (byte offset 1048576).  Override with FAT_OFFSET if needed.
#
# WARNING: this writes to the image file given.  Work on a copy.
set -eu

IMG=$1
MODE=$2
OFFSET=${FAT_OFFSET:-1048576}
HERE=$(cd "$(dirname "$0")" && pwd)

case "$MODE" in
rtc)
	mcopy -o -i "$IMG@@$OFFSET" \
		"$HERE/../prebuilt/imx28-pwsh6-rtc-only.dtb" \
		::imx28-pwsh6.dtb
	echo "installed RTC-only DTB into $IMG"
	;;
full)
	ZIMAGE=$3
	DTB=$4
	mcopy -o -i "$IMG@@$OFFSET" "$ZIMAGE" ::zImage
	mcopy -o -i "$IMG@@$OFFSET" "$DTB" ::imx28-pwsh6.dtb
	echo "installed zImage + DTB into $IMG"
	;;
*)
	echo "usage: $0 <sd.img> rtc | full <zImage> <dtb>" >&2
	exit 1
	;;
esac
