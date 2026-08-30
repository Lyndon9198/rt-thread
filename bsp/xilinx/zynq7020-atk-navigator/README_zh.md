# ZYNQ7020 ATK Navigator BSP

[English](README.md)

## 简介

本 BSP 支持基于 Xilinx Zynq-7020 SoC 的正点原子领航者开发板。本项目由
社区独立维护，并非正点原子官方产品。

支持的启动流程为：

```text
BootROM -> FSBL -> U-Boot -> RT-Thread SMP
```

U-Boot 将 `rtthread.bin` 加载到 `0x00200000`，然后通过
`go 0x00200000` 启动 RT-Thread。

## 硬件支持

| 功能 | RT-Thread 接口 | 默认状态 | 验证情况 |
| --- | --- | --- | --- |
| Cortex-A9 双核 | SMP、CPU0/CPU1 | 开启 | 两个 CPU 均进入调度器 |
| GICv2、MMU、Cache | 架构移植层 | 开启 | SMP 和驱动正常工作 |
| 1 GiB PS DDR3 | 系统堆 | 开启 | 顶部 2 MiB 保留给 GEM DMA |
| VFP/NEON | 线程浮点上下文 | 开启 | 两个 CPU 均支持上下文切换 |
| PS UART0 | `uart0`、控制台 | 开启 | 115200 8N1，中断接收 |
| PS GPIO | `pin` | 开启 | 支持 MIO/EMIO 输入、输出和中断 |
| PS Watchdog | `wdt` | 开启 | 支持超时、启动、停止和喂狗 |
| TTC0 Timer0 | `timer0` | 开启 | 支持单次和周期时钟定时器 |
| PS CAN0 | `can0` | 开启 | 支持正常、监听和内部回环模式 |
| PS GEM0 | `e0` | 开启 | RTL8211E、PHY 地址 7、RGMII-ID |
| DHCP、DNS、SAL | 网络协议栈 | 开启 | 默认使用 DHCP |
| DFS/devfs | `/dev` | 开启 | 支持标准 RT-Thread 设备访问 |

本分支尚未正式支持 I2C、SPI/QSPI、SD/eMMC、USB、XADC、显示和触摸设备。
其他开发分支中存在驱动代码，不代表这些外设已经在本分支完成硬件验证。

## GPIO 编号

RT-Thread pin 编号与 Zynq GPIO 的对应关系如下：

```text
pin 0-53   -> MIO0-MIO53
pin 54-117 -> EMIO0-EMIO63
```

GPIO 方向接口不会修改 Zynq MIO pin-control 寄存器中的上下拉配置。EMIO
信号必须已经在 Vivado 设计中导出，并包含在 XSA 和设备树中，否则对应 pin
编号无法控制外部引脚。

## 外设说明

### Watchdog

Watchdog 注册为 `wdt`，通过 RT-Thread 标准 watchdog 控制接口设置整秒超时、
启动、停止和喂狗。

### TTC 定时器

TTC0 Timer0 注册为 `timer0`，使用当前 RT-Thread `rt_clock_timer` 接口。
计数频率为 976562 Hz，即 125 MHz 除以 128；使用 16 位间隔计数器，支持
中断驱动的单次和周期模式。

### CAN0

PS CAN0 注册为 `can0`，支持标准帧、扩展帧、数据帧、远程帧、中断收发、
正常模式、监听模式和内部回环模式。支持 125 kbit/s、250 kbit/s、
500 kbit/s 和 1 Mbit/s，当前硬件设计的 CAN 参考时钟为 100 MHz。

CAN0 通过 EMIO 引出：RX 连接 FPGA 管脚 L16，TX 连接 J14，I/O 电压为
3.3 V。这两个信号是控制器逻辑电平信号，连接实际 CAN 总线前必须外接
3.3 V CAN 收发器。

### GEM0

GEM0 注册为 `e0`，外接 RTL8211E PHY，MDIO 地址为 7，接口模式为
RGMII-ID。默认配置启用 DHCP、大 TCP 窗口、TCP/IP core-lock input 和
零拷贝接收。

DDR 顶部 2 MiB 被保留为 GEM descriptor 和数据缓冲区使用的非缓存 DMA
区域，不计入系统堆。

## 工具链

安装 Arm bare-metal GCC 工具链，并确保 `arm-none-eabi-gcc` 位于 `PATH`。
也可以将 `RTT_EXEC_PATH` 设置为工具链的 `bin` 目录。

## 快速开始

从 RT-Thread 仓库根目录执行：

```sh
cd bsp/xilinx/zynq7020-atk-navigator
scons --menuconfig
scons --pyconfig-silent
scons -j8
```

不要手工修改 `rtconfig.h`。应通过 menuconfig 修改 `.config`，然后运行
`scons --pyconfig-silent` 重新生成 `rtconfig.h`。

构建生成：

- `rtthread.elf`：用于 JTAG 调试；
- `rtthread.bin`：用于 U-Boot 加载；
- `build/`：中间构建目录。

这些生成文件不应提交到 Git。

## Menuconfig 配置

板级驱动菜单位于：

```text
Hardware Drivers Config
    ALIENTEK Navigator ZYNQ7020 BSP
```

### BSP 配置项

| Kconfig 符号 | 菜单项 | 自动选择的框架 |
| --- | --- | --- |
| `BSP_USING_UART0` | Enable PS UART0 | RT-Thread serial |
| `BSP_USING_GPIO` | Enable PS GPIO | RT-Thread pin |
| `BSP_USING_WDT` | Enable PS watchdog | RT-Thread watchdog |
| `BSP_USING_TTC0_TIMER0` | Enable TTC0 timer0 | Clock-time |
| `BSP_USING_CAN0` | Enable PS CAN0 | RT-Thread CAN |
| `BSP_USING_GEM0` | Enable PS GEM0 Ethernet | DM、DMA、OFW/FDT、Cadence GEM、PHY、SAL、lwIP、netdev |

仓库中的默认 `.config` 已开启上述全部选项。GEM0 会自动选择其依赖项，
不要在其他菜单中单独关闭 DMA、OFW/FDT、Cadence Ethernet、PHY v2、SAL、
lwIP 或 netdev。

### SMP 配置

默认配置为：

```text
RT_USING_SMP=y
RT_CPUS_NR=2
```

可在内核配置菜单的 SMP/多核相关选项中检查。该 BSP 的启动代码、GIC、
定时器和网络线程配置均按双核模式验证。

### 网络配置

默认关键参数如下：

| 配置 | 默认值 |
| --- | --- |
| `RT_LWIP_DHCP` | 开启 |
| 静态回退地址 | `192.168.1.30/24` |
| 网关 | `192.168.1.1` |
| `RT_LWIP_PBUF_NUM` | 128 |
| `RT_LWIP_TCP_SEG_NUM` | 1024 |
| `RT_LWIP_TCP_SND_BUF` | 262144 字节 |
| `RT_LWIP_TCP_WND` | 524288 字节 |
| lwIP TCP/IP 线程 | 优先级 10、栈 4096、邮箱 1024 |
| Ethernet RX 线程 | 优先级 12、栈 4096、邮箱 64 |

若要使用静态 IP，在以下菜单关闭 DHCP：

```text
RT-Thread Components
    Network
        light weight TCP/IP stack
            Enable DHCP
```

然后在相同菜单中设置 IP 地址、网关和子网掩码。若要复现本文的千兆网性能，
应保留默认的大 TCP 窗口、发送缓冲区和 segment 数量。

### 不修改配置直接构建

如果使用仓库中已经保存的默认 `.config`：

```sh
scons --pyconfig-silent
scons -j8
```

## 从 U-Boot 启动

将 `rtthread.bin` 放入 TFTP 服务器目录，在 U-Boot 中执行：

```text
setenv serverip <tftp-server-ip>
setenv ipaddr <uboot-ip>
tftpboot 0x00200000 rtthread.bin
dcache flush
go 0x00200000
```

UART0 参数为 115200 baud、8 数据位、无校验、1 停止位。

## 首次启动检查

进入 MSH 后检查版本、双核线程、内存、设备和网络：

```text
msh />version
msh />list_thread
msh />free
msh />list_device
msh />ifconfig
```

默认配置下，`list_device` 应包含 `uart0`、`pin`、`wdt`、`timer0`、
`can0` 和 Ethernet 设备 `e0`。DHCP 地址可能与静态回退地址不同，后续
测试应以 `ifconfig` 显示的地址为准。

基础网络验证：

```text
msh />ping <host-ip>
msh />iperf_server
```

Linux 主机需要安装 iperf2，然后执行：

```sh
iperf -c <board-ip> -r -t 20 -i 5
```

`-r` 的第一阶段为主机发送、开发板接收；第二阶段为开发板发送、主机接收。

实机 20 秒测试结果：

| 方向 | 测试结果 |
| --- | --- |
| Linux 主机 -> Zynq7020 | 约 949 Mbit/s |
| Zynq7020 -> Linux 主机 | 主机统计约 640 Mbit/s |
| Zynq7020 -> Linux 主机 | RT-Thread 统计约 654 Mbit/s |

TX 路径会批量提交 descriptor，并直接映射单 pbuf TCP segment。该 pbuf
由 TCP unacked 队列持有直到收到 ACK，其生命周期晚于本地 GEM DMA 完成。

## 内存布局

RT-Thread 链接并加载到 `0x00200000`。系统堆使用普通缓存 DDR 区域，
`0x3fe00000`-`0x3fffffff` 为 GEM descriptor 和 packet buffer 保留的
2 MiB 非缓存 DMA 区域。因此 `free` 显示的可用容量会略小于完整 1 GiB。

## 已知限制

- 启动时可能出现 `tx_clk set_rate 125000000 Hz failed: ENOSYS`。当前 Zynq
  clock provider 不支持动态修改该时钟，但 FSBL 已完成 GEM 参考时钟配置，
  此警告不影响千兆链路。
- EMIO GPIO 必须先在 Vivado 设计中导出，并体现在 XSA 和设备树中。
- CAN0 需要外部 3.3 V CAN 收发器后才能连接物理 CAN 总线。
- `iperf_server` 是 BSP 性能验证命令，不是生产网络服务。
- 当前网络性能优化仅在 Zynq7000 SMP 配置下完成验证。

## 时钟配置说明

当前常量与验证使用的 Navigator 硬件设计一致：CPU 时钟 767 MHz，UART
输入时钟 100 MHz。如果在 Vivado 中修改 PS 时钟配置，需要同步检查
`board/zynq7000.h` 中的对应定义，并重新生成 FSBL/XSA 相关文件。
