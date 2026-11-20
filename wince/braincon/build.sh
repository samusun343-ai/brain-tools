#!/bin/sh
# Build BrainCon.dll (console stream driver) and P47Term.exe
# (terminal + launcher) for the SHARP Brain WinCE guest.
#
# Requirements: clang (ARM backend) and python3. No SDK or libraries
# are needed: the C sources are freestanding and declare the coredll
# surface they use; pe-link47.py links the objects into WinCE PEs
# that import COREDLL.dll by ordinal.
#
# Outputs: BrainCon.dll (~80 KB, mostly zero-filled ring storage)
#          P47Term.exe  (~28 KB)
# plus <out>.imports.json / <out>.map.json next to each binary.
set -e
cd "$(dirname "$0")"

CLANG=${CLANG:-clang}
CFLAGS="-target armv5te-none-eabi -marm -Os -ffreestanding -fno-builtin -fshort-wchar"

# shellcheck disable=SC2086
$CLANG $CFLAGS -c braincon.c -o braincon.o
# shellcheck disable=SC2086
$CLANG $CFLAGS -c p47term.c -o p47term.o

python3 pe-link47.py braincon.o BrainCon.dll --dll --entry DllMain \
    --exports CON_Init,CON_Deinit,CON_Open,CON_Close,CON_Read,CON_Write,CON_Seek,CON_IOControl,CON_PowerUp,CON_PowerDown \
    --base 0x10000000
python3 pe-link47.py p47term.o P47Term.exe

echo "built BrainCon.dll and P47Term.exe"
