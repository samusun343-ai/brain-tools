# brainux-driver

Drivers and device-tree fixes that make the SHARP Brain (PW-SH6) hardware
work under [Brainux](https://github.com/brain-hackers/buildbrain)
(Debian 13, Linux 6.1.70, DT `imx28-pwsh6`):

| Item | State in stock Brainux | This repo |
| --- | --- | --- |
| i.MX28 RTC | DT node `status="disabled"`, no `/dev/rtc0` | DT enabled + wakeup-source; driver wake-IRQ fix |
| Audio (ROHM BU26154MUV codec) | DT names an EVK sgtl5000 that is not on the board; no driver exists | New ASoC codec driver + MXS machine driver + DT glue |
| Suspend wake from RTC alarm | RTC not registered as a wakeup source; alarm IRQ is masked during suspend | `wakeup-source` DT property honoured by `rtc-stmp3xxx` |

## Legal basis / information sources

Everything here was written **only** from:

- the public ROHM BU26154MUV datasheet (Rev.002),
- mainline Linux source code,
- the Brainux distribution and its kernel source
  ([brain-hackers/linux-brain](https://github.com/brain-hackers/linux-brain), GPL),
- black-box observation of a running Brainux system (boot logs, dmesg,
  commands run inside the guest, audio captured from the emulator).

No WinCE/eMMC image contents were extracted, disassembled or otherwise
analysed for this work.

## Contents

- `patches/` — the three changes as patches against
  brain-hackers/linux-brain @ Linux 6.1.70:
  1. `0001` — DTS: enable the RTC (`status="okay"`, `wakeup-source`).
  2. `0002` — ASoC: BU26154 codec driver (`sound/soc/codecs/bu26154.c`),
     MXS machine driver (`sound/soc/mxs/mxs-bu26154.c`), Kconfig/defconfig
     wiring, and the DT change from sgtl5000 to the BU26154.
  3. `0003` — `rtc-stmp3xxx`: honour `wakeup-source`
     (`device_init_wakeup()` + `dev_pm_set_wake_irq()`), so the RTC alarm
     can wake the system from suspend.
- `sound/` — the two new driver sources, for direct copying into a kernel
  tree (identical to the copies in patch 0002).
- `prebuilt/imx28-pwsh6-rtc-only.dtb` — the stock DTB with *only* the RTC
  node enabled; works with the stock kernel (the stock kernel already has
  `CONFIG_RTC_DRV_STMP=y`). Enough for M1 (RTC) without rebuilding a kernel.
- `prebuilt/imx28-pwsh6.dtb` — DTB for the rebuilt kernel (RTC + BU26154).
  (The prebuilt DTBs are binary and are not mirrored in the GitHub copy of
  this repo; regenerate them with `scripts/build-kernel.sh`, which builds
  the full DTB, or apply the `wakeup-source` edit from patch 0001 to the
  stock DTB with `dtc` for the RTC-only variant.)
- `scripts/build-kernel.sh` — clone linux-brain, apply the patches, build
  `zImage` + DTB.
- `scripts/install-sd.sh` — install a DTB (RTC-only fix) or a full
  kernel+DTB onto a Brainux SD image (FAT boot partition, via mtools).

## Why a full kernel rebuild for audio?

The Brainux rootfs ships **no `/lib/modules` directory** and no
`Module.symvers` for the stock kernel, so out-of-tree modules cannot be
built or loaded against it in practice (no matching symbol CRCs). The
practical route — and the one verified here — is to rebuild the whole
kernel from the distribution's own source with the new drivers built-in
(`CONFIG_SND_SOC_BU26154=y`, `CONFIG_SND_SOC_MXS_BU26154=y`).

## Toolchain

Any ARMv5-compatible cross toolchain works. Verified with:

- Ubuntu packages: `gcc-arm-linux-gnueabi device-tree-compiler bc flex bison mtools`
  (`sudo apt install gcc-arm-linux-gnueabi device-tree-compiler bc flex bison mtools`), or
- the relocatable Bootlin toolchain
  `armv5-eabi--glibc--stable-2024.05-1` (GCC 13.3,
  prefix `arm-buildroot-linux-gnueabi-`) plus a `bc` on `PATH`
  (the kernel build needs `bc` for `timeconst.h`).

Set `CROSS_COMPILE` accordingly; `scripts/build-kernel.sh` defaults to
`arm-linux-gnueabi-`.

## Build

```sh
git clone https://github.com/brain-hackers/linux-brain
cd linux-brain
for p in /path/to/brainux-driver/patches/*.patch; do git am "$p"; done
# or: ./scripts/build-kernel.sh /path/to/linux-brain
```

`scripts/build-kernel.sh` runs `brain_defconfig` and builds
`arch/arm/boot/zImage` and `arch/arm/boot/dts/imx28-pwsh6.dtb` into an
out-of-tree build directory (`O=../kbuild` by default).

## Install onto an SD image

Work on a **copy** of the Brainux SD image. The boot partition is the
first (FAT) partition at byte offset 1048576; the files are `zImage` and
`imx28-pwsh6.dtb`.

```sh
cp sd.img sd-work.img
# RTC only (stock kernel):
scripts/install-sd.sh sd-work.img rtc
# Full (rebuilt kernel + DTB):
scripts/install-sd.sh sd-work.img full /path/to/zImage /path/to/imx28-pwsh6.dtb
```

## Verify

Boot the image and log in (`user` / `brain`).

RTC:

```sh
ls /dev/rtc0
cat /sys/class/rtc/rtc0/name          # stmp3xxx-rtc 80056000.rtc
cat /sys/class/rtc/rtc0/device/power/wakeup   # enabled (rebuilt kernel)
sudo hwclock -r                        # or read /proc/driver/rtc
```

Audio:

```sh
cat /proc/asound/cards                 # mxs_bu26154
aplay -l
amixer sget 'Master'
speaker-test -D hw:0,0 -t sine -f 440 -c 2 -r 44100 -l 1
```

Suspend wake (rebuilt kernel): set an RTC alarm ~1 minute ahead
(e.g. via `/sys/class/rtc/rtc0/wakealarm` or the `RTC_ALM_SET` +
`RTC_AIE_ON` ioctls), then `echo mem > /sys/power/state`; the system
resumes by itself when the alarm fires.

## Verification status (2026-11-18, on the SHARP Brain QEMU)

- **RTC (stock kernel + `prebuilt/imx28-pwsh6-rtc-only.dtb`)**: `stmp3xxx-rtc
  80056000.rtc: registered as rtc0`; `/dev/rtc0` present; `RTC_SET_TIME` to
  2026-11-18 14:25:30 read back exactly via `RTC_RD_TIME`.
- **Audio (rebuilt kernel + `prebuilt/imx28-pwsh6.dtb`)**: dmesg shows
  `bu26154 0-001a: BU26154 codec found` and card `#0: mxs_bu26154`;
  `/proc/asound/pcm` lists `HiFi Playback` and `HiFi Capture`. Mixer:
  `amixer controls` lists Master Playback/Capture Volume, Speaker Playback
  Volume and Master Playback Switch; `sset 'Master' 200` reads back
  `200 [78%] [-27.50dB]` (the PDATT dB mapping is exact). Playback of a
  440 Hz sine (speaker-test, 44.1 kHz stereo) captured through the
  emulator's wav backend: ≈50 % non-zero samples (speaker-test drives one
  channel at a time) and 441 zero crossings in the first 0.5 s — the
  correct 440 Hz tone.
- **Suspend wake (guest side)**: with patch 0003,
  `/sys/class/rtc/rtc0/device/power/wakeup` reads `enabled` and the
  `wakealarm` interface exists; an alarm can be armed and the system
  enters `mem` suspend cleanly.

## Known issues

- The distribution's `user` account is not in the `audio` group, while
  `/dev/snd/*` is `root:audio 0660` — run ALSA tools as root (or add the
  user to the group). This is distribution setup, not a driver problem.
- On the SHARP Brain QEMU, an open playback stream triggers an
  interrupt/softirq flood in the SoC DMA completion path (the guest logs
  RCU stalls and crawls while the stream is open; the audio data itself
  is correct). The codec driver uses no interrupt; the flood is in the
  mxs DMA/SAIF platform path and how the emulator paces it, and is
  tracked on the emulator side.
- RTC-alarm wake from suspend cannot be observed on that emulator: its
  RTC model stores the ALARM register but never compares it against the
  seconds counter, so the alarm interrupt is never raised. The guest-side
  wiring (this repo) is the standard mainline path and is complete.

## Hardware notes (from the public datasheet)

The BU26154 is at I2C address `0x1a` on i2c0. Its register interface is
unusual: the first byte of every transaction is an INDEX; an even INDEX
reads and an odd INDEX writes the canonical even-numbered register, and
the data pointer auto-increments by 2. The driver therefore implements
the codec read/write callbacks directly instead of using regmap. The
part is clocked from the SAIF (it is the I2S slave; the SoC is master),
with `CLKSEL` choosing the MCLKI divider so that MCLKI = 256/512/1024 × fs.
