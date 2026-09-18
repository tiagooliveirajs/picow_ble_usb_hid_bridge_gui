# Debian 13 XFCE/X11 — build, flash and debug the RP2350 BKB-3G Classic HID POC

This guide is for the branch:

`poc/rp2350-classic-hid-usb-cdc`

The POC target is Raspberry Pi Pico 2 W / RP2350. It exposes two USB functions at the same time:

- CDC ACM debug console -> `/dev/ttyACM*`
- standard USB HID keyboard -> Linux input subsystem

No UART adapter is required.

## 1. Install host packages

On Debian 13 (Trixie):

```sh
sudo apt update

sudo apt install -y \
  git \
  build-essential \
  cmake \
  ninja-build \
  gcc-arm-none-eabi \
  libnewlib-arm-none-eabi \
  libstdc++-arm-none-eabi-newlib \
  python3 \
  libusb-1.0-0-dev \
  picocom \
  usbutils
```

Raspberry Pi's C/C++ SDK documentation lists CMake, `gcc-arm-none-eabi`,
`libnewlib-arm-none-eabi` and `libstdc++-arm-none-eabi-newlib` as the Linux
cross-build prerequisites. Debian 13 provides GCC ARM Embedded 14.2.x, matching
the toolchain generation used by Pico SDK 2.2.0.

Check:

```sh
cmake --version
ninja --version
arm-none-eabi-gcc --version
git --version
```

## 2. Give your normal user access to CDC serial devices

Debian normally assigns `/dev/ttyACM*` to group `dialout`.

```sh
sudo usermod -aG dialout "$USER"
```

Log out of the XFCE session and log back in once after running this command.

Verify:

```sh
id
```

You should see `dialout` in the group list.

## 3. Clone Pico SDK 2.2.0

Keep the SDK outside the application repository.

```sh
mkdir -p "$HOME/pico"
cd "$HOME/pico"

git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git checkout 2.2.0
git submodule update --init --recursive
```

Export the SDK path:

```sh
export PICO_SDK_PATH="$HOME/pico/pico-sdk"
```

To persist it for future terminals:

```sh
printf '\nexport PICO_SDK_PATH="$HOME/pico/pico-sdk"\n' >> "$HOME/.bashrc"
source "$HOME/.bashrc"
```

Confirm:

```sh
git -C "$PICO_SDK_PATH" describe --tags --always
```

Expected tag: `2.2.0`.

## 4. Clone this repository and select the POC branch

Fresh clone:

```sh
cd "$HOME/pico"

git clone https://github.com/tiagooliveirajs/picow_ble_usb_hid_bridge_gui.git
cd picow_ble_usb_hid_bridge_gui

git fetch origin
git switch --track origin/poc/rp2350-classic-hid-usb-cdc
```

If you already have the repository:

```sh
cd /path/to/picow_ble_usb_hid_bridge_gui
git fetch origin
git switch poc/rp2350-classic-hid-usb-cdc
git pull --ff-only
```

Verify that you are not on `main`:

```sh
git branch --show-current
git status --short --branch
```

Expected branch:

```text
poc/rp2350-classic-hid-usb-cdc
```

## 5. Configure a Debug build for RP2350

```sh
cd "$HOME/pico/picow_ble_usb_hid_bridge_gui/src_fw/poc_bkb3g_classic_hid_cdc"

rm -rf build

cmake -S . -B build -G Ninja \
  -DPICO_BOARD=pico2_w \
  -DPICO_SDK_PATH="$PICO_SDK_PATH" \
  -DCMAKE_BUILD_TYPE=Debug
```

Important: this POC intentionally rejects any target other than `pico2_w`.

## 6. Compile locally

```sh
cmake --build build -j"$(nproc)"
```

Expected file:

```text
build/bkb3g_classic_hid_cdc.uf2
```

Also keep the ELF; it contains symbols and is useful for offline debugging:

```text
build/bkb3g_classic_hid_cdc.elf
```

Inspect sizes:

```sh
arm-none-eabi-size build/bkb3g_classic_hid_cdc.elf
sha256sum build/bkb3g_classic_hid_cdc.uf2
```

For disassembly with source lines:

```sh
arm-none-eabi-objdump -dS build/bkb3g_classic_hid_cdc.elf \
  > build/bkb3g_classic_hid_cdc.dis
```

## 7. Optional: install picotool 2.2.0

Pico SDK 2.x uses picotool for binary processing. If CMake does not already
obtain a usable picotool, install a matching version:

```sh
cd "$HOME/pico"

git clone https://github.com/raspberrypi/picotool.git
cd picotool
git checkout 2.2.0

cmake -S . -B build -G Ninja \
  -DPICO_SDK_PATH="$PICO_SDK_PATH" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j"$(nproc)"
sudo cmake --install build
```

Check:

```sh
picotool version
```

Inspect the locally built firmware before flashing:

```sh
picotool info -a \
  "$HOME/pico/picow_ble_usb_hid_bridge_gui/src_fw/poc_bkb3g_classic_hid_cdc/build/bkb3g_classic_hid_cdc.uf2"
```

## 8. Flash with BOOTSEL

1. Disconnect the Pico 2 W.
2. Hold BOOTSEL.
3. Connect USB while still holding BOOTSEL.
4. Release BOOTSEL after the RP2350 mass-storage volume appears.

Find the mount:

```sh
lsblk -o NAME,LABEL,FSTYPE,SIZE,MOUNTPOINTS
```

Under XFCE it is normally auto-mounted below:

```text
/media/$USER/RP2350
```

Copy the UF2:

```sh
cp build/bkb3g_classic_hid_cdc.uf2 "/media/$USER/RP2350/"
sync
```

The mass-storage volume disappears after the board reboots. That is normal.

## 9. Observe USB enumeration

Keep this running before flashing/reconnecting:

```sh
sudo journalctl -kf
```

Alternative:

```sh
sudo dmesg -w
```

The application should enumerate as one composite USB device with:

- CDC ACM serial interface
- HID keyboard interface

Useful commands:

```sh
lsusb
lsusb -t
ls -l /dev/ttyACM*
ls -l /dev/serial/by-id/ 2>/dev/null || true
```

The current descriptor uses VID:PID `cafe:4005`, so you can inspect it with:

```sh
lsusb -d cafe:4005
```

## 10. Open the CDC debug log

Prefer the stable `/dev/serial/by-id/...` path if Debian created one.
Otherwise use the actual `ttyACM` number.

Example:

```sh
picocom -b 115200 /dev/ttyACM0
```

The value `115200` is only a terminal setting here. CDC ACM is carried over USB,
not through a physical UART.

Exit picocom with:

```text
Ctrl-A  Ctrl-X
```

The firmware buffers early boot logs, so opening the terminal after USB
enumeration still shows recent startup diagnostics.

Expected early markers include:

```text
BOOT: RP2350 BKB-3G Classic HID -> USB HID POC
CORE1: starting CYW43 + BTstack Classic HID Host
BTSTACK: HCI_STATE_WORKING
SCAN: Bluetooth Classic inquiry started
```

## 11. Test Goldentec GT T1 / code 40062 pairing

This keyboard advertises as `BKB-3G` / `Bluetooth keyboard 3.0`.

For the cleanest test, first erase the keyboard's own saved Bluetooth memories:
hold **FN+ESC for 5 seconds**. This is the reset procedure documented for the GT T1.

Then:

1. Open the CDC console.
2. Put the keyboard into pairing mode with `FN+1`, `FN+2`, or `FN+3`
   until the white LED blinks.
3. Watch the CDC log.

Useful states:

```text
FOUND:
MATCH:
BOND:
SSP:
PAIRING:
CONNECT:
HID: CONNECTION OPEN
HID report descriptor
POC READY
```

The current POC intentionally requests **Security Level 2 / MITM not required** and
advertises local SSP capability as `NoInputNoOutput`. This matches the GT T1's
documented first-pairing flow, which does not instruct the user to enter a PIN or passkey.

The new `SSP:` lines also print the remote keyboard's IO capability and
authentication requirements so a remaining mismatch can be diagnosed precisely.

After `POC READY`, open Mousepad or another text editor and test:

```text
abc
ABC
space
Enter
Shift combinations
```

Each Classic input packet and each normalized USB keyboard state is also printed
in the CDC console.

## 12. Capture a full diagnostic session to a file

Terminal A:

```sh
sudo journalctl -kf | tee "$HOME/pico-usb-kernel.log"
```

Terminal B:

```sh
picocom -b 115200 /dev/ttyACM0 | tee "$HOME/bkb3g-poc.log"
```

If piping picocom is inconvenient, use `script`:

```sh
script -f "$HOME/bkb3g-poc.typescript" \
  -c 'picocom -b 115200 /dev/ttyACM0'
```

Those two logs together are the most useful output for the next debugging pass.

## 13. If /dev/ttyACM0 does not appear

First check whether the composite device enumerated:

```sh
lsusb -d cafe:4005
lsusb -t
sudo journalctl -k --since "5 minutes ago"
```

Load the Linux CDC ACM driver if necessary:

```sh
sudo modprobe cdc_acm
```

Then reconnect the Pico and check again:

```sh
ls -l /dev/ttyACM*
```

If HID works but CDC does not appear, save:

```sh
lsusb -v -d cafe:4005 > "$HOME/pico-lsusb-v.txt"
sudo journalctl -k --since "10 minutes ago" > "$HOME/pico-kernel.txt"
```

## 14. If CDC works but the keyboard is not discovered

Capture the log from power-on through at least two complete inquiry cycles.

Look for:

- whether `BTSTACK: HCI_STATE_WORKING` appears;
- whether any `FOUND:` devices appear;
- whether the keyboard name is present in EIR;
- whether `NAME:` remote-name resolution sees `Bluetooth keyboard 3.0` or
  `BKB-3G`.

Use a fresh keyboard pairing slot if possible.

## 15. If the keyboard is found but pairing fails

Keep the exact lines around:

```text
MATCH:
CONNECT:
PAIRING:
HID:
```

Do not repeatedly flash different firmware before saving the log. The status code
from the first failed attempt is valuable.

## 16. If POC READY appears but typing fails

Save:

- the `HID report descriptor` dump;
- one press and release of the `A` key;
- one press and release of Left Shift;
- one `Shift+A` sequence.

The expected normalized line for a simple `A` key is based on HID usage
`0x04`. Modifier usages `0xE0..0xE7` are converted to the modifier byte.

This POC intentionally ignores consumer/media usages until the basic keyboard
path is proven.

## 17. What "Debug" means without UART or an SWD probe

This build uses `CMAKE_BUILD_TYPE=Debug`, so the ELF retains debug symbols.
Without an SWD probe you cannot stop the RP2350 at breakpoints, but you still
have three useful diagnostic layers:

1. firmware event logs over USB CDC;
2. Linux kernel USB enumeration logs;
3. offline ELF inspection with `arm-none-eabi-objdump`, `nm`, and `addr2line`.

Examples:

```sh
arm-none-eabi-nm -n build/bkb3g_classic_hid_cdc.elf | less

arm-none-eabi-objdump -dS build/bkb3g_classic_hid_cdc.elf | less
```

If a future crash log gives a program counter such as `0x10001234`, map it to a
source line with:

```sh
arm-none-eabi-addr2line -e build/bkb3g_classic_hid_cdc.elf -f -C 0x10001234
```

A real breakpoint/single-step session later would require an SWD debug probe,
which is separate from this no-UART logging workflow.
