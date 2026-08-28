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
- PS GPIO pin device with interrupt support
- VFP/NEON context management on both Cortex-A9 cores
- PS watchdog device
- TTC0 timer0 clock-timer device
- PS CAN0 device
- Cortex-A9 private timer system tick

PS GPIO pin numbers 0-53 address MIO0-MIO53. Pin numbers 54-117 address
EMIO0-EMIO63. Pull-up and pull-down settings belong to the Zynq MIO pin-control
registers and are not changed by the GPIO direction API.

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

UART0 uses 115200 baud, 8 data bits, no parity, and 1 stop bit.

## Notes

The clock constants match the Navigator hardware configuration used to verify
this BSP: CPU clock 767 MHz and UART input clock 100 MHz. If the processing
system clock configuration is changed in Vivado, update the corresponding
definitions in `board/zynq7000.h`.
