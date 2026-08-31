# RT-Thread boot script for the board-mounted eMMC (U-Boot mmc 1, FAT partition 1).
echo Loading RT-Thread from eMMC...
if mmc dev 1 && mmc rescan; then
    if fatload mmc 1:1 0x00200000 rtthread.bin; then
        dcache flush
        go 0x00200000
    fi
fi
echo Failed to load rtthread.bin from mmc 1:1
