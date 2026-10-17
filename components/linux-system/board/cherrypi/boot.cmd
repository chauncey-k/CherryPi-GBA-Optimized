# Serial prototype; Linux serial0 currently aliases UART1.
# Keep the compressed kernel above its decompressed destination.
setenv kernel_addr_r 0x81000000
setenv fdt_addr_r 0x81600000
setenv fdt_high 0x81700000
setenv bootm_size 0x01700000
setenv bootargs 'console=ttyS0,115200 earlycon=uart8250,mmio32,0x01c25400 ignore_loglevel root=/dev/mmcblk0p2 rootfstype=ext4 rootwait rw panic=10'
if load mmc 0:1 ${kernel_addr_r} zImage; then
    if load mmc 0:1 ${fdt_addr_r} suniv-f1c100s-generic.dtb; then
        bootz ${kernel_addr_r} - ${fdt_addr_r}
    fi
fi
echo 'Cherry Pi boot failed; inspect serial output and MMC files.'
