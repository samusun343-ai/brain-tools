#!/bin/sh
# Build a Brainux kernel (zImage + imx28-pwsh6.dtb) with the drivers
# from this repository.
#
# The Brainux kernel is brain-hackers/linux-brain (Linux 6.1.70).
# This script clones it (or reuses $LINUX_BRAIN if set), applies the
# patches in ../patches, configures brain_defconfig and builds.
#
# Host packages needed (Debian/Ubuntu):
#   gcc-arm-linux-gnueabi bc flex bison device-tree-compiler mtools git make
#
# Output: $OUT/arch/arm/boot/zImage and
#         $OUT/arch/arm/boot/dts/imx28-pwsh6.dtb
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
SRC=${LINUX_BRAIN:-"$HERE/linux-brain"}
OUT=${OUT:-"$HERE/kbuild"}
JOBS=${JOBS:-3}

if [ ! -d "$SRC/.git" ]; then
	git clone --depth 1 https://github.com/brain-hackers/linux-brain.git "$SRC"
fi

cd "$SRC"
for p in "$HERE"/../patches/*.patch; do
	if git apply --check "$p" 2>/dev/null; then
		git apply "$p"
		echo "applied $p"
	else
		echo "skip (already applied?): $p"
	fi
done

export ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-
make O="$OUT" brain_defconfig
make O="$OUT" -j"$JOBS" zImage imx28-pwsh6.dtb

echo
echo "Built:"
echo "  $OUT/arch/arm/boot/zImage"
echo "  $OUT/arch/arm/boot/dts/imx28-pwsh6.dtb"
