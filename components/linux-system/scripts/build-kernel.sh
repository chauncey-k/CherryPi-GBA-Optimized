#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
mkdir -p build/kernel logs
# Prevent unaliased UART0 from claiming the UART1 console's ttyS0 slot.
if ! grep -Fq 'serial1 = &uart0;' sunxi_kernel/arch/arm/boot/dts/suniv-f1c100s-generic.dts; then
    git -C sunxi_kernel apply --check "$root/patches/kernel-cherrypi-serial-aliases.patch"
    git -C sunxi_kernel apply "$root/patches/kernel-cherrypi-serial-aliases.patch"
fi
# Apply the panel and display graph after the serial alias patch.
if ! grep -Fq 'cherrypi,rgb480272-prototype' sunxi_kernel/drivers/gpu/drm/panel/panel-simple.c; then
    git -C sunxi_kernel apply --check "$root/patches/kernel-cherrypi-lcd-prototype.patch"
    git -C sunxi_kernel apply "$root/patches/kernel-cherrypi-lcd-prototype.patch"
fi
# Backlight patch follows the LCD patch; default PWM duty is 20%.
if ! grep -Fq 'cherrypi_backlight: backlight' sunxi_kernel/arch/arm/boot/dts/suniv-f1c100s-generic.dts; then
    git -C sunxi_kernel apply --check "$root/patches/kernel-cherrypi-backlight.patch"
    git -C sunxi_kernel apply "$root/patches/kernel-cherrypi-backlight.patch"
fi
if ! grep -Fq 'label = "cherrypi-gamepad";' sunxi_kernel/arch/arm/boot/dts/suniv-f1c100s-generic.dts; then
    git -C sunxi_kernel apply --check "$root/patches/kernel-cherrypi-gamepad.patch"
    git -C sunxi_kernel apply "$root/patches/kernel-cherrypi-gamepad.patch"
fi
# Avoid strict pinmux/GPIO ownership conflict; request pull-ups via GPIO flags.
if grep -Fq 'pinctrl-0 = <&gamepad_pe_pins &gamepad_pa_pins>;' sunxi_kernel/arch/arm/boot/dts/suniv-f1c100s-generic.dts; then
    git -C sunxi_kernel apply --check "$root/patches/kernel-cherrypi-gamepad-gpio-ownership.patch"
    git -C sunxi_kernel apply "$root/patches/kernel-cherrypi-gamepad-gpio-ownership.patch"
fi
if ! grep -Fq 'Preserve bias flags for the fwnode GPIO consumer path' sunxi_kernel/drivers/gpio/gpiolib-of.c; then
    git -C sunxi_kernel apply --check "$root/patches/kernel-cherrypi-gpio-bias-flags.patch"
    git -C sunxi_kernel apply "$root/patches/kernel-cherrypi-gpio-bias-flags.patch"
fi
if [ ! -f build/kernel/.config ]; then
    cp configs/kernel-verified.config build/kernel/.config
fi
sunxi_kernel/scripts/config --file build/kernel/.config -e BACKLIGHT_CLASS_DEVICE -e BACKLIGHT_PWM -e PWM -e PWM_SUN4I -e INPUT -e INPUT_EVDEV -e INPUT_POLLDEV -e KEYBOARD_GPIO_POLLED
make -C sunxi_kernel O="$root/build/kernel" ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- olddefconfig
make -C sunxi_kernel O="$root/build/kernel" ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j"${JOBS:-4}" zImage suniv-f1c100s-generic.dtb modules 2>&1 | tee logs/kernel-build-latest.log
