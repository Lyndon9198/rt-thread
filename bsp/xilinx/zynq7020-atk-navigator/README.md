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
- Cortex-A9 private timer system tick

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
