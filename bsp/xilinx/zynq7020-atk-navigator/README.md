# ZYNQ7020 ATK Navigator BSP

## Introduction

This BSP supports the ALIENTEK Navigator board based on the Xilinx Zynq-7020
SoC. It is an independent, community-maintained port and is not an official
ALIENTEK product.

The current BSP provides:

- U-Boot loading and entry at `0x00200000`
- Cortex-A9 dual-core SMP
- ARM GICv2 interrupt controller
- MMU and cache
- 1 GiB PS DDR heap
- PS UART0 console
- PS UART1 device (EMIO controller; current PL design does not export pins)
- PS GPIO pin device with interrupt support
- GPIO software I2C bus for the touch controller
- VFP/NEON context management on both Cortex-A9 cores
- PS watchdog device
- TTC0 timer0 clock-timer device
- PS CAN0 device
- PS GEM0 Ethernet with the external RTL8211E PHY
- PS SD1 with the board-mounted 8 GB eMMC
- PS QSPI0 with the 32 MiB W25Q256 flash
- PS USB0 host through the ULPI PHY and onboard USB 2.0 hub
- 1024x600 RGB888 LCD through AXI VDMA and VTC
- FT5246 capacitive touch device on the software I2C bus
- Cortex-A9 private timer system tick

PS GPIO pin numbers 0-53 address MIO0-MIO53. Pin numbers 54-117 address
EMIO0-EMIO63. Pull-up and pull-down settings belong to the Zynq MIO pin-control
registers and are not changed by the GPIO direction API.

AXI GPIO0 is mapped at `0x41200000`. Its two output-only bits are appended to
the same RT-Thread `pin` device. Pin 118 (`AXI_GPIO0_0` or `PLLED0`) drives H15,
and pin 119 (`AXI_GPIO0_1` or `PLLED1`) drives L15:

```text
msh />pin mode 118 output
msh />pin write 118 1
msh />pin read 118
```

The touch connector software I2C bus is registered as `swi2c0`. SCL uses
GPIO64 (EMIO10) and SDA uses GPIO62 (EMIO8). Both pins use open-drain GPIO
semantics and therefore require the pull-ups present on the board. Hardware
scanning detects the FT5246 touch controller at 7-bit address `0x38`.

The watchdog is registered as `wdt`. It supports setting a timeout in whole
seconds, start, stop, and keepalive through the standard RT-Thread watchdog
device controls.

TTC0 timer0 is registered as `timer0` through RT-Thread's current
`rt_clock_timer` interface. Its fixed counter frequency is 976562 Hz (125 MHz
divided by 128), with a 16-bit interval counter and interrupt-driven one-shot
or periodic operation.

PS CAN0 is registered as `can0`. The driver supports standard and extended
data or remote frames, interrupt-driven receive and transmit, normal, listen,
and internal loopback modes, and 125 kbit/s, 250 kbit/s, 500 kbit/s, or
1 Mbit/s operation. The CAN reference clock is 100 MHz for the hardware design
used by this BSP.

CAN0 is routed through EMIO: RX is connected to FPGA pin L16 and TX to J14,
both using 3.3 V I/O. These signals are controller-level CAN RX/TX and require
an external 3.3 V CAN transceiver before they can be connected to a CAN bus.

GEM0 is registered as `e0`. Its RTL8211E PHY uses MDIO address 7 and RGMII-ID.
The default configuration uses DHCP and enables a larger TCP window, direct
TCP/IP-core-locked input, and zero-copy receive buffers. The top 2 MiB of DDR
is reserved for non-cacheable GEM descriptor and packet storage; it is not
included in the system heap.

Hardware validation used a direct gigabit link to a Linux host. ICMP passed
without packet loss, and an iperf2 TCP host-to-board test sustained about
850 Mbit/s for 15 seconds. The conservative copy-based board-to-host path
sustained about 250 Mbit/s. Higher transmit rates require asynchronous TX
descriptor batching or reference-counted zero-copy TX; neither is enabled
because the buffer lifetime must remain correct under SMP.

## Toolchain

Install an Arm bare-metal GCC toolchain and make sure `arm-none-eabi-gcc` is in
`PATH`. Alternatively, set `RTT_EXEC_PATH` to the toolchain's `bin` directory.

## Build

Run the following commands in this BSP directory:

```sh
scons --pyconfig-silent
scons -j8
```

The build creates `rtthread.elf` and `rtthread.bin`.

## Boot from U-Boot

Copy `rtthread.bin` to the TFTP server and use these commands at the U-Boot
prompt (replace the server address and file path when needed):

```text
setenv serverip 192.168.1.10
setenv ipaddr 192.168.1.30
tftpboot 0x00200000 rtthread.bin
dcache flush
go 0x00200000
```

### Boot from eMMC

The board-mounted eMMC is U-Boot device `mmc 1`. Its first partition is FAT
and can hold RT-Thread beside the existing Linux boot files. Generate the
U-Boot script without committing the generated `rtthread.scr` binary:

```sh
mkimage -A arm -O rtos -T script -C none \
    -n "RT-Thread Zynq7020" -d rtthread.cmd rtthread.scr
```

Copy `rtthread.bin` and `rtthread.scr` to the root of eMMC partition 1. They
can also be downloaded and written from the U-Boot prompt:

```text
mmc dev 1
mmc rescan
tftpboot ${loadaddr} rtthread.bin
fatwrite mmc 1:1 ${loadaddr} rtthread.bin ${filesize}
tftpboot ${loadaddr} rtthread.scr
fatwrite mmc 1:1 ${loadaddr} rtthread.scr ${filesize}
```

Test the eMMC copy before changing the persistent boot command:

```text
fatload mmc 1:1 ${loadaddr} rtthread.scr
source ${loadaddr}
```

After the test succeeds, select RT-Thread at power-on:

```text
setenv bootcmd 'mmc dev 1; mmc rescan; fatload mmc 1:1 ${loadaddr} rtthread.scr; source ${loadaddr}'
saveenv
```

The script is named `rtthread.scr`, rather than `boot.scr`, so the existing
Linux boot script remains available. Restore the previous `bootcmd` to return
to Linux.

When `BSP_USING_SD1_EMMC` is enabled, the same eMMC is registered through the
RT-Thread MMC/SD stack. The first implementation uses the Zynq SD1 Arasan
SDHCI controller at `0xe0101000`, interrupt 79, a 4-bit bus and a maximum card
clock of 50 MHz. The device is non-removable and does not use a card-detect or
write-protect signal. Elm-FAT is enabled so an existing FAT partition can be
mounted without formatting it. For example, the first detected partition can
be inspected with:

```text
mount sd0p0 / elm
ls /
umount /
```

The root mount point is used here because the default root filesystem is
devfs. On the verified 8 GB eMMC, RT-Thread reports `sd0`, `sd0p0` and
`sd0p1`, selects High Speed mode, and detects 64 MiB and 128 MiB partitions.
Do not run `mkfs` on a device that contains the board's existing boot or Linux
partitions.

When `BSP_USING_QSPI0` is enabled, the PS QSPI controller is registered as
`qspi0`, its CS0 device as `qspi00`, and the W25Q256 is probed by SFUD as
`nor0`. The verified input clock is 200 MHz and the SPI clock is 50 MHz. Basic
read-only checks are:

```text
qspi_id
qspi_test
sf probe qspi00
sf read 0 64
```

`qspi_test rw` tests write, read and erase at the final 4 KiB sector only when
the complete sector is blank. It erases the test data afterwards and verifies
that the sector has returned to all `0xff`. The command refuses to modify a
non-empty sector. Do not use `sf erase`, `sf write` or `sf bench` on an address
containing the boot image or U-Boot environment.

When `BSP_USING_USB0_HOST` is enabled, CherryUSB uses the Zynq PS USB0
ChipIdea/EHCI controller at `0xe0002000`, interrupt 53, and its ULPI PHY. MIO9
controls the active-low PHY reset. The saved configuration selects the custom
EHCI host port, HID and mass-storage classes, D-cache maintenance, and a USB
mass-storage mount-point pattern of `/usb%c`.

The controller and attached devices can be inspected from MSH:

```text
lsusb
lsusb -v
list device
```

Hardware validation enumerated the onboard Genesys Logic USB 2.0 high-speed
hub (`05e3:0608`) as `/dev/hub2`. A USB disk appears as an RT-Thread block
device and is mounted below `/usb*` when its filesystem is supported. Do not
place the mount point at `/`, because the eMMC and USB automount paths would
then conflict.

The controller performs DMA from cached DDR. Keep both
`CONFIG_USB_DCACHE_ENABLE` and `CONFIG_USB_EHCI_DESC_DCACHE_ENABLE` enabled;
disabling either can cause corrupted descriptors or control transfers on the
Cortex-A9.

UART0 uses 115200 baud, 8 data bits, no parity, and 1 stop bit.

When `BSP_USING_UART1` is enabled, the second PS UART is registered as
`uart1` at `0xe0001000`, interrupt 82. The controller supports the same baud,
data-bit, parity, stop-bit and interrupt-driven receive operations as UART0.
Its controller datapath can be checked without external wiring:

```text
uart1_loopback
```

The supplied Vivado design enables UART1 through EMIO but currently ties its
RX input high and leaves TX unconnected in the PL top level. Therefore the
saved bitstream cannot provide external UART1 signals. To connect a second
serial adapter, export the processing-system `UART1_TX` and `UART1_RX` EMIO
signals, constrain them to suitable 3.3 V PL pins, rebuild the bitstream/XSA,
and cross-connect TX/RX/GND. The RT-Thread driver itself does not assign PL
pins.

When `BSP_USING_RGB_LCD` is enabled, the PL display pipeline is registered as
the RT-Thread graphic device `lcd`. The hardware path is AXI VDMA MM2S at
`0x43000000`, AXI4-Stream Video Out, VTC at `0x43c00000`, and the RGB888
output. The fixed panel mode is 1024x600 at a 50 MHz pixel clock. Horizontal
timing is 1024 active, 160 front porch, 20 sync and 140 back porch; vertical
timing is 600 active, 12 front porch, 3 sync and 20 back porch.

The driver allocates one 1,843,200-byte RGB888 framebuffer from the system
heap. Applications obtain its address, pitch and format with
`RTGRAPHIC_CTRL_GET_INFO`, write pixels in R-G-B byte order, then issue
`RTGRAPHIC_CTRL_RECT_UPDATE` so dirty cache lines are visible to VDMA.

Use the built-in checks after boot:

```text
list device
lcd_info
lcd_test
```

`lcd_test` draws eight vertical color bars. A healthy `lcd_info` result has
the VDMA halted bit clear and no bits from `0x00000ff0` set in the VDMA status;
the VTC control value is `0x00000007` and its error register is zero. Hardware
validation produced continuous active video with VDMA status `0x00011000`
and no DMA error bits. The PL design drives LCD reset and backlight from its
reset signal, so they are not RT-Thread GPIO pins.

When `BSP_USING_FT5246` is enabled, the controller at I2C address `0x38` is
registered as the standard RT-Thread touch device `touch0`. It supports five
contacts over `swi2c0`; GPIO60 is its active-low, falling-edge interrupt. The
panel reports coordinates in Y-X order, so the driver swaps them into the
LCD's 1024x600 X-Y coordinate system.

```text
touch_info
touch_read
```

`touch_info` reads the controller identity and interrupt level. Hardware
validation found chip ID `0x54`, firmware `0x14`, and vendor `0x82`.
`touch_read` samples one frame and prints active contacts; run it while a
finger is on the panel. Applications should normally open `touch0` with
`RT_DEVICE_FLAG_INT_RX`, install an RX callback, and read up to five
`struct rt_touch_data` records when GPIO60 asserts.

## Notes

The clock constants match the Navigator hardware configuration used to verify
this BSP: CPU clock 767 MHz and UART input clock 100 MHz. If the processing
system clock configuration is changed in Vivado, update the corresponding
definitions in `board/zynq7000.h`.
