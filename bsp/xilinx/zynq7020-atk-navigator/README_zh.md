# ZYNQ7020 ATK Navigator BSP

[English](README.md)

## 简介

本 BSP 支持基于 Xilinx Zynq-7020 的正点原子领航者开发板，由社区独立
维护，并非正点原子官方产品。当前启动链为：

```text
BootROM -> FSBL -> U-Boot -> RT-Thread SMP
```

U-Boot 将 `rtthread.bin` 加载到 `0x00200000` 后启动。当前分支已支持双核
Cortex-A9 SMP、GICv2、MMU/Cache、1 GiB DDR、UART0、GPIO/AXI GPIO、
软件 I2C、Watchdog、TTC、CAN0、GEM0、SD1 eMMC、QSPI Flash 和 USB0 Host。

## 构建与配置

在 BSP 目录执行：

```sh
cd bsp/xilinx/zynq7020-atk-navigator
scons --menuconfig
scons --pyconfig-silent
scons -j8
```

不要手工修改 `rtconfig.h`；修改 `.config` 后必须通过
`scons --pyconfig-silent` 重新生成。板级菜单位于：

```text
Hardware Drivers Config
    ALIENTEK Navigator ZYNQ7020 BSP
```

主要配置项如下：

| 配置项 | 设备或功能 |
| --- | --- |
| `BSP_USING_UART0` | PS UART0，控制台 `uart0` |
| `BSP_USING_UART1` | PS UART1，设备 `uart1`，内部回环可测试 |
| `BSP_USING_GPIO` | PS MIO/EMIO `pin` 设备 |
| `BSP_USING_AXI_GPIO0` | PL LED，pin 118/119 |
| `BSP_USING_TOUCH_I2C` | FT5246 使用的软件 I2C `swi2c0` |
| `BSP_USING_WDT` | PS Watchdog `wdt` |
| `BSP_USING_TTC0_TIMER0` | TTC0 Timer0 `timer0` |
| `BSP_USING_CAN0` | PS CAN0 `can0` |
| `BSP_USING_GEM0` | PS GEM0 `e0` |
| `BSP_USING_SD1_EMMC` | PS SD1 及板载 eMMC |
| `BSP_USING_QSPI0` | PS QSPI0 及 W25Q256 |
| `BSP_USING_USB0_HOST` | PS USB0 EHCI Host、HID、U 盘 |

默认 `.config` 已选择 USB Host 的 `ehci_custom` 控制器。若在 CherryUSB
菜单中重新选择 Host IP，必须保持 `ehci_custom`，否则不会编译 Zynq EHCI
核心。

## 从 U-Boot 启动

TFTP 启动命令示例：

```text
setenv serverip <服务器地址>
setenv ipaddr <开发板地址>
tftpboot 0x00200000 rtthread.bin
dcache flush
go 0x00200000
```

UART0 参数为 115200、8N1。

## eMMC / SD1

板载 8 GB eMMC 连接 SD1：MIO46～51、4-bit、最高 50 MHz。RT-Thread
注册 `sd0` 及分区设备；实机已进入 High Speed 模式并识别约 7.28 GiB。

```text
mount sd0p0 / elm
ls /
umount /
```

不要对包含启动文件或 Linux 分区的设备执行 `mkfs`。U-Boot 中 eMMC 是
`mmc 1`，可把 `rtthread.bin` 和由 `rtthread.cmd` 生成的
`rtthread.scr` 放入第一 FAT 分区，实现脱离 TFTP 启动。

## QSPI Flash

PS QSPI0 注册为 `qspi0`，CS0 设备为 `qspi00`，W25Q256 通过 SFUD 注册
为 `nor0`。容量 32 MiB，实机时钟 50 MHz。

```text
qspi_id
qspi_test
sf probe qspi00
sf read 0 64
```

`qspi_test rw` 只会在末尾 4 KiB 扇区完全为空时进行写、读、擦除验证，
并在结束后确认恢复为 `0xff`。禁止擦写 BootROM 启动镜像或 U-Boot 环境。

## UART1

UART1 注册为 `uart1`，基地址 `0xe0001000`、中断 82，支持与 UART0 相同
的波特率、数据位、校验位、停止位和中断接收。无需外部接线即可验证控制器
数据通路：

```text
uart1_loopback
```

当前 Vivado 设计虽然把 UART1 设置为 EMIO，却在 PL 顶层将 RX 固定为 1，
且 TX 未连接，因此现有 bitstream 无法从外部引脚收发 UART1。要接第二个
USB 转串口，需在 Vivado 中导出 PS 的 `UART1_TX`/`UART1_RX` EMIO，约束到
合适的 3.3 V PL 管脚，重新生成 bitstream/XSA，并交叉连接 TX、RX 和 GND。
RT-Thread 驱动不会替 Vivado 分配 PL 管脚。

## USB0 Host

USB0 使用 Zynq PS ChipIdea/EHCI 控制器：基地址 `0xe0002000`、中断 53、
ULPI PHY，MIO9 控制低有效 PHY 复位。CherryUSB 提供 Hub、HID 和 MSC
类支持。控制器 DMA 使用缓存 DDR，因此必须保留：

```text
CONFIG_USB_DCACHE_ENABLE
CONFIG_USB_EHCI_DESC_DCACHE_ENABLE
```

检查 USB 设备：

```text
lsusb
lsusb -v
list device
```

实机已将板载 Genesys Logic USB 2.0 高速 Hub（`05e3:0608`）枚举为
`/dev/hub2`。U 盘会注册为块设备，并按 `/usb%c` 模式自动挂载；该挂载点
不得设为 `/`，否则会与 eMMC 根目录挂载冲突。键盘、鼠标由 HID 类驱动
识别。

## GPIO、CAN 和网络注意事项

Zynq pin 0～53 对应 MIO0～53，pin 54～117 对应 EMIO0～63。AXI GPIO
两路 LED 是 pin 118（H15）和 pin 119（L15）。触摸软件 I2C 的 SDA/SCL
分别为 GPIO62/GPIO64，FT5246 地址为 `0x38`。

CAN0 RX/TX 通过 EMIO 连接 L16/J14，接入 CAN 总线前必须使用外部 3.3 V
CAN 收发器。GEM0 使用 RTL8211E、MDIO 地址 7、RGMII-ID；DDR 顶部
2 MiB 保留给 GEM 非缓存 DMA，不计入系统堆。

## 启动检查

```text
version
list thread
free
list device
ifconfig
lsusb -v
```

生成的 `rtthread.elf`、`rtthread.bin`、`rtthread.map` 和 `build/` 不应
提交到 Git。
