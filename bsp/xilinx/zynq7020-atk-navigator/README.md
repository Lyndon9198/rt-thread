# ZYNQ7020 ATK Navigator BSP

[中文说明](README_zh.md)

## Introduction

This BSP supports the ALIENTEK Navigator board based on the Xilinx Zynq-7020
SoC. It is an independent, community-maintained port and is not an official
ALIENTEK product.

The supported boot flow is BootROM -> FSBL -> U-Boot -> RT-Thread. U-Boot
loads `rtthread.bin` at `0x00200000` and starts it with `go 0x00200000`.

## Hardware support

| Function | RT-Thread interface | Default | Validation notes |
| --- | --- | --- | --- |
| Dual Cortex-A9 | SMP, CPU0/CPU1 | Enabled | Both CPUs enter the scheduler |
| GICv2, MMU and cache | Architecture port | Enabled | Required by SMP and drivers |
| 1 GiB PS DDR3 | System heap | Enabled | Top 2 MiB is reserved for GEM DMA |
| VFP/NEON | Per-thread context | Enabled | Context is managed on both CPUs |
| PS UART0 | `uart0`, console | Enabled | 115200 8N1, interrupt RX |
| PS GPIO | `pin` | Enabled | MIO/EMIO input, output and IRQ |
| PS watchdog | `wdt` | Enabled | Timeout, start, stop and keepalive |
| TTC0 timer0 | `timer0` | Enabled | One-shot and periodic clock timer |
| PS CAN0 | `can0` | Enabled | Normal, listen and internal loopback |
| PS GEM0 | `e0` | Enabled | RTL8211E, PHY address 7, RGMII-ID |
| DHCP, DNS and SAL | Network stack | Enabled | DHCP is used by default |
| DFS/devfs | `/dev` | Enabled | Standard RT-Thread device access |

I2C, SPI/QSPI, SD/eMMC, USB, XADC and display/touch devices are not part of
this validated BSP yet. A driver being present on another development branch
does not mean it is supported by this branch.

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

GEM0 is registered as `e0`. Its RTL8211E PHY uses MDIO address 7 and RGMII-ID.
The default configuration uses DHCP and enables a larger TCP window, direct
TCP/IP-core-locked input, and zero-copy receive buffers. The top 2 MiB of DDR
is reserved for non-cacheable GEM descriptor and packet storage; it is not
included in the system heap.

Hardware validation used a direct gigabit link to a Linux host. ICMP passed
without packet loss. A 20-second iperf2 reverse test measured 949 Mbit/s from
the host to the board and 640 Mbit/s from the board to the host; RT-Thread's
report for the latter direction was 654 Mbit/s. The TX path batches descriptor
kicks and directly maps single-pbuf TCP segments. These buffers remain owned
by the TCP unacknowledged queue until an ACK arrives, which is later than GEM
DMA completion.

Start the server from MSH and run an iperf2 bidirectional test on the host:

```text
msh />iperf_server
```

```sh
iperf -c <board-ip> -r -t 20 -i 5
```

## Toolchain

Install an Arm bare-metal GCC toolchain and make sure `arm-none-eabi-gcc` is in
`PATH`. Alternatively, set `RTT_EXEC_PATH` to the toolchain's `bin` directory.

## Quick start

From the RT-Thread repository root:

```sh
cd bsp/xilinx/zynq7020-atk-navigator
scons --menuconfig
scons --pyconfig-silent
scons -j8
```

Do not edit `rtconfig.h` manually. Change `.config` through menuconfig and run
`scons --pyconfig-silent` to regenerate `rtconfig.h` before building.

The build creates `rtthread.elf` for debugging and `rtthread.bin` for U-Boot.
Generated binaries and build directories must not be committed.

## Menuconfig

Board drivers are under:

```text
Hardware Drivers Config
    ALIENTEK Navigator ZYNQ7020 BSP
```

The board menu contains these switches:

| Kconfig symbol | Menu item | Selects |
| --- | --- | --- |
| `BSP_USING_UART0` | Enable PS UART0 | RT-Thread serial framework |
| `BSP_USING_GPIO` | Enable PS GPIO | RT-Thread pin framework |
| `BSP_USING_WDT` | Enable PS watchdog | RT-Thread watchdog framework |
| `BSP_USING_TTC0_TIMER0` | Enable TTC0 timer0 | Clock-time framework |
| `BSP_USING_CAN0` | Enable PS CAN0 | RT-Thread CAN framework |
| `BSP_USING_GEM0` | Enable PS GEM0 Ethernet | DM, DMA, OFW/FDT, Cadence GEM, PHY, SAL, lwIP and netdev |

The checked-in default configuration enables all items above. Important
system and network values are:

| Setting | Default value |
| --- | --- |
| `RT_USING_SMP` / `RT_CPUS_NR` | Enabled / 2 |
| `RT_LWIP_DHCP` | Enabled |
| Static fallback address | `192.168.1.30/24` |
| Gateway | `192.168.1.1` |
| `RT_LWIP_PBUF_NUM` | 128 |
| `RT_LWIP_TCP_SEG_NUM` | 1024 |
| `RT_LWIP_TCP_SND_BUF` | 262144 bytes |
| `RT_LWIP_TCP_WND` | 524288 bytes |
| lwIP TCP/IP thread | Priority 10, stack 4096, mailbox 1024 |
| Ethernet RX thread | Priority 12, stack 4096, mailbox 64 |

To use a fixed address, disable DHCP at:

```text
RT-Thread Components
    Network
        light weight TCP/IP stack
            Enable DHCP
```

Then set the IPv4 address, gateway and netmask in the same menu. Keep the
larger TCP buffers when reproducing the documented gigabit performance.

Driver dependencies are selected automatically by the BSP options. Avoid
disabling a selected framework independently: for example, GEM0 requires DMA,
OFW/FDT, the Cadence Ethernet driver, PHY v2, SAL, lwIP and netdev.

## Build without changing the configuration

Run the following commands in this BSP directory:

```sh
scons --pyconfig-silent
scons -j8
```

## Boot from U-Boot

Copy `rtthread.bin` to the TFTP server and use these commands at the U-Boot
prompt (replace the server address and file path when needed):

```text
setenv serverip <tftp-server-ip>
setenv ipaddr <uboot-ip>
tftpboot 0x00200000 rtthread.bin
dcache flush
go 0x00200000
```

UART0 uses 115200 baud, 8 data bits, no parity, and 1 stop bit.

## First boot checks

After the MSH prompt appears, check the CPUs, memory and registered devices:

```text
msh />version
msh />list_thread
msh />free
msh />list_device
msh />ifconfig
```

With the default configuration, `list_device` includes `uart0`, `pin`, `wdt`,
`timer0`, `can0` and Ethernet device `e0`. DHCP may assign an address different
from the static fallback address; always use the address shown by `ifconfig`.

For a basic network check:

```text
msh />ping <host-ip>
msh />iperf_server
```

On a Linux host with iperf2 installed:

```sh
iperf -c <board-ip> -r -t 20 -i 5
```

The first `-r` phase is host-to-board and the second is board-to-host.

## Memory layout

RT-Thread is linked and loaded at `0x00200000`. The heap extends through the
normal cached DDR area. The range `0x3fe00000`-`0x3fffffff` is a non-cacheable
2 MiB region reserved for GEM descriptors and packet buffers, so `free` reports
slightly less than the full 1 GiB DDR capacity.

## Known limitations

- `tx_clk set_rate 125000000 Hz failed: ENOSYS` can be printed because the
  current Zynq clock provider cannot change this clock. FSBL has already set
  the GEM reference clock, and the warning does not prevent a gigabit link.
- EMIO GPIO pins only exist when the corresponding EMIO signals are exported
  by the Vivado design and represented by the XSA/device tree.
- CAN0 exposes controller-level RX/TX signals and needs an external 3.3 V CAN
  transceiver for connection to a physical CAN bus.
- The `iperf_server` command is a BSP validation utility, not a production
  network service.

## Notes

The clock constants match the Navigator hardware configuration used to verify
this BSP: CPU clock 767 MHz and UART input clock 100 MHz. If the processing
system clock configuration is changed in Vivado, update the corresponding
definitions in `board/zynq7000.h`.
