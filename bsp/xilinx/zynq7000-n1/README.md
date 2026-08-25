# N1 Zynq-7020 RT-Thread SMP BSP

This BSP targets the N1 board hardware exported by Vivado 2024.2 as
`N1_linux_display.xsa`. The first development stage provides the two Cortex-A9
cores in one RT-Thread SMP system, PS UART0, GICv2, the Cortex-A9 private timer,
MMU/cache, and FinSH/MSH.

The established N1 boot chain is retained:

```text
QSPI BOOT.BIN (FSBL + PL bitstream + U-Boot)
    -> U-Boot TFTP/eMMC loader
        -> RT-Thread SMP
```

The BSP does not contain generated XSA, bitstream, FSBL, U-Boot, or binary
artifacts.

## Hardware baseline

- SoC: XC7Z020-CLG400-2
- DDR: 1 GiB at `0x00000000`
- RT-Thread link/load address: `0x00200000`
- PS UART0: `0xE0000000`, MIO14/MIO15, 115200 8N1
- GIC distributor/CPU interface: `0xF8F01000`/`0xF8F00100`
- Cortex-A9 private timer interrupt: PPI 29
- CPU/peripheral clock from XSA: 767 MHz/100 MHz
- QSPI: W25Q256 containing the existing `BOOT.BIN`
- eMMC: U-Boot `mmc 1`, FAT partition `mmc 1:1`

## Build

Install an `arm-none-eabi` GCC toolchain and SCons, then run:

```bash
cd /home/h/rt-thread/bsp/xilinx/zynq7000-n1
scons --pyconfig-silent
scons -j$(nproc)
```

Set a non-default toolchain path when necessary:

```bash
export RTT_EXEC_PATH=/path/to/gcc-arm-none-eabi/bin
scons -j$(nproc)
```

Successful builds produce `rtthread.elf` and `rtthread.bin`. These are build
artifacts and must not be committed.

## First boot through the existing TFTP workflow

Copy only the raw image to the existing TFTP root:

```bash
sudo cp rtthread.bin /tftpboot/
sudo chmod 644 /tftpboot/rtthread.bin
```

Stop at the existing U-Boot prompt and execute:

```text
setenv serverip 192.168.1.5
setenv ipaddr 192.168.1.10
tftpboot 0x00200000 rtthread.bin
dcache flush
go 0x00200000
```

Do not use `bootm` for the raw image. `rtthread.bin` is linked to and loaded at
`0x00200000`. The first 32 bytes disable inherited U-Boot IRQ/FIQ state and
enter the reset path; the exception vector table follows at a 32-byte aligned
address.

The serial console is the CH340 adapter. Prefer its stable host path:

```text
/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0
```

On the development host used for the first board test, Ethernet was
`192.168.1.5/24`; do not reuse the older `192.168.1.2` example without checking
`ip -brief address` first. This U-Boot build stopped responding after the
`dcache off` command, so the verified sequence deliberately uses `dcache flush`
followed directly by `go`. The BSP entry code masks inherited IRQ/FIQ state and
the Cortex-A port rebuilds MMU/cache state.

Expected initial output includes the RT-Thread banner, the MSH prompt, and:

```text
N1 Zynq-7020 RT-Thread SMP started on CPU0.
```

Use `ps` from MSH to inspect `tidle0` and `tidle1`, then run `smp_test`. The
expected result contains one line for CPU0 and one for CPU1.

## GPIO and board I/O

The `pin` device covers Zynq PS MIO/EMIO pins 0 through 117. Pins 118 and 119
map to the two output bits of the AXI GPIO at `0x41200000`. The first board I/O
test deliberately touches only signals confirmed by the N1 device tree and
XDC: EMIO54/J16 and AXI GPIO H15/L15 LEDs, plus the five board keys on MIO11,
MIO12, and EMIO55--57.

Run:

```text
gpio_test
```

The three LEDs blink together three times and the current active-low key levels
are printed. PS GPIO interrupt support is not enabled yet; the first version
uses polling so PHY reset, LCD control, and touch wiring can be validated before
interrupt handling is added.

## Persistent eMMC boot

Keep the verified QSPI `BOOT.BIN` unchanged. Put `rtthread.bin` in the FAT
partition of eMMC and test without saving the environment:

```text
mmc dev 1
mmc rescan
fatload mmc 1:1 0x00200000 rtthread.bin
dcache flush
go 0x00200000
```

Only after repeated successful boots should an RT-Thread boot command be saved.
Keep the existing Linux command available for recovery, for example:

```text
setenv boot_rtt 'mmc dev 1; mmc rescan; fatload mmc 1:1 0x00200000 rtthread.bin; dcache flush; go 0x00200000'
saveenv
```

Run it explicitly with:

```text
run boot_rtt
```

Do not replace the current `bootcmd` until RT-Thread SMP has passed the board
validation below.

## Validation order

1. Confirm the banner and MSH input on UART0.
2. Confirm the tick advances and delayed threads wake correctly.
3. Confirm CPU1 joins the scheduler and both idle threads exist.
4. Run concurrent threads on both CPUs for at least one hour.
5. Reboot repeatedly through TFTP, then through eMMC.
6. Add SD/eMMC, CAN, Ethernet, PL interrupt, VDMA, display, and touch drivers in
   separate changes after the SMP base is stable.

If boot stops, capture the complete UART log from the U-Boot `go` command onward.
Also record whether the failure occurs before the RT-Thread banner, during CPU1
startup, or after the MSH prompt.

## Board test record

The initial N1 board test on 2026-08-21 verified:

- QSPI FSBL/bitstream/U-Boot remained unchanged.
- U-Boot downloaded `rtthread.bin` from `192.168.1.5` at about 14.2 MiB/s.
- RT-Thread reached its banner and the `msh >` prompt.
- `ps` showed `tidle1`, confirming that CPU1 joined the SMP scheduler.
- The GPIO-enabled image registered the standard `pin` device and executed
  `gpio_test` without a CPU exception. Serial receive dropped before the final
  key-level line was captured, so physical LED/key confirmation remains open.

No U-Boot environment was saved and no QSPI/eMMC content was written during
this test.

The next peripheral stage is GEM0 Ethernet. RT-Thread already contains a
Cadence GEM driver with Zynq support, but it is a device-tree/device-model
driver. The initial BSP booted as a small fixed-address BSP without an FDT, so
GEM integration added a board FDT/device model (including PHY address 7 and
`rgmii-id`) instead of duplicating a second private GEM driver. Display work
follows the same path because VDMA and VTC also require the XSA-derived hardware
description.

### GEM0 integration status

The BSP now embeds `zynq7000-n1.dtb` and enables Device Model, Cadence GEM,
PHY v2, lwIP 2.0.3, SAL, netdev, static address `192.168.1.30`, `ping`, and an
iperf2-compatible lwiperf server on TCP port 5001. The runtime `ofw_test`
command has verified the following live FDT values on the board:

```text
GEM0 OFW node: ok, reg=0xe000b000, irq=54, phy=ethernet-phy@7
```

The legacy Cortex-A9 port keeps its proven GICv2 implementation. A narrow
Cadence compatibility path translates the standard GIC SPI tuple to IRQ 54 and
uses the BSP's existing identity-mapped device window instead of remapping the
GEM registers. The BSP reserves physical memory
`0x08000000--0x081fffff` exclusively for GEM DMA. It is outside the system heap,
mapped as shared normal non-cacheable memory, and registered as a DMA pool
before Device Model probes GEM0. GEM descriptors and packet buffers must not
fall back to the cacheable heap.

On 2026-08-22, a clean JTAG reset/download verified 5/5 ICMP replies with
0.805--1.601 ms latency. Start a receive-throughput test from the host with:

```shell
iperf -c 192.168.1.30 -p 5001 -t 10 -i 1
```

This is the iperf2 protocol; the lwIP 2.0.3 `lwiperf` application is not
compatible with an `iperf3` client. The current TCP test reached 50.3 Mbit/s
during its first second, then stopped receiving after about 6.05 MiB. Its
10-second average was 4.96 Mbit/s and ICMP stopped responding afterward. This
was traced with JTAG to several independent problems: CPU1 accessed the Zynq SCU
before switching from the 4 MiB early page table to `MMUTable`, and sustained
RX could leave `rx_tail` on an empty descriptor while completed descriptors
waited later in the ring. CPU1 now switches page tables before touching the SCU.
The lwIP Ethernet threads are pinned to CPU0 so the non-coherent GEM DMA ring,
its cache maintenance, and IRQ 54 execute on the same Cortex-A9. GEM ISR status
is explicitly written back to clear it; JTAG verified that this removed an IRQ
storm of more than 20 million entries. The A9 ring is 256 RX / 64 TX descriptors
and RX interrupts are masked while the Ethernet thread drains the ring.

The 17.3 Mbit/s result came from the superseded cache-maintenance implementation
and is not an acceptance result. That implementation scanned forward over RX
descriptor holes, which can reorder TCP packets, and has been removed.

The fixed non-cacheable DMA image was tested on 2026-08-22 through the documented
U-Boot TFTP sequence. A 60-second host-to-board iperf2 run transferred 57.1 MiB
at 7.98 Mbit/s without stalling. Immediately afterward, a 1000-packet ping test
received all 1000 replies with 0% loss and 0.797 ms average latency. Descriptor
reliability is therefore improved, but this throughput is not acceptable for a
gigabit interface. Acceptance still requires all of the following on the same
image:

```text
ping: 0% loss for at least 1000 packets
iperf2 host -> board: 60 seconds without a stall
iperf2 board -> host: 60 seconds without a stall
ping after both throughput tests: 0% loss
```

Subsequent receive-path work increased the standard-MTU result to a stable
406 Mbit/s for 20 seconds. The measured improvements were: 512/128 RX/TX
descriptors, a TCPIP input-message pool large enough to avoid burst drops,
direct core-locked lwIP input, GEM RX checksum offload, and zero-copy custom
pbufs backed by the non-cacheable RX DMA buffers.

Jumbo frames cannot be used to remove the remaining packet-rate limit on this
PS GEM. The N1 device reports the Zynq-7000 GEM v2 implementation; Xilinx's
official `xemacps` driver enables jumbo mode only for GEM versions greater than
2. On this board the jumbo maximum-length register at GEM offset `0x48` reads
back zero, and 7960-byte TCP segments receive no ACK even with the jumbo bit
set. The experimental jumbo configuration was therefore disabled and the host
test interface restored to MTU 1500.

The 2026-08-22 optimization pass changed the release build from `-Os` to `-O2`
and removed a redundant completion poll from every `macb_eth_rx()` call. The RX
thread already drains the ring after one notification; the old call also
scanned TX completions and attempted another notification for every frame.
With MTU 1500, direct core-locked input, RX checksum offload, and zero-copy RX,
the resulting image passed this host-to-board test:

```text
0.0000-20.0068 sec  1.12 GBytes  479 Mbits/sec
ping: 3 transmitted, 3 received, 0% loss
```

Use `iperf -c 192.168.1.30 -p 5001 -t 20 -i 2`. Do not use `iperf3` against
this endpoint: it closes the incompatible control connection and produces a
zero-byte lwiperf report.

The following measured experiments were rejected rather than retained:

- Whole-image `-O3` and network-group `-O3` caused startup exceptions; the
  stable image remains `-O2`.
- `RT_DMA_F_NOCACHE` synchronization flags reduced throughput from 478 to
  461 Mbit/s with this port's mapping/synchronization implementation.
- Splitting RX delivery and TCP input across CPU1/CPU0 through the tcpip mailbox
  reduced throughput to 341 Mbit/s. Direct core-locked input on CPU0 is faster.
- Increasing RX interrupt moderation from about 50 us to about 200 us was
  effectively neutral (478 versus 479 Mbit/s), confirming that lwIP's
  per-frame TCP processing is now the dominant standard-MTU limit.

The GEM/PHY link negotiates 1000 Mbit/s, but 1000 Mbit/s is not a valid TCP
payload result for Gigabit Ethernet; near-line-rate TCP is approximately
940 Mbit/s. The current datapath has not reached that target. Further gains
require a materially different receive path, such as batching multiple packets
through one cross-core message or using a newer/faster IP stack. The PS GEM v2
jumbo limitation prevents using large frames as the packet-rate workaround.

### GEM0 performance continuation record

This section records the exact hand-off state after the later 2026-08-22
optimization pass. The image currently running on the board was built with
global `-O2`, downloaded through U-Boot TFTP, and measured as:

```text
iperf -c 192.168.1.30 -p 5001 -t 10 -i 2
0.0000-10.0063 sec  751 MBytes  629 Mbits/sec
```

The retained changes responsible for the progression from 406 to 629 Mbit/s
are:

1. `components/net/lwip/port/ethernetif.c`: bind the direct RX/lwIP input
   thread to CPU1. GEM IRQ 54 remains on CPU0, so interrupt handling and the
   TCP receive hot path execute in parallel without a per-packet cross-core
   mailbox. This was the largest recent gain: 479 to 598 Mbit/s.
2. `components/drivers/ethernet/cadence/macb.c`: do not call
   `macb_poll_completions()` from every `macb_eth_rx()` invocation when a real
   IRQ is installed. The RX thread already drains the ring after one notice.
3. Reinitialize each preallocated Zynq custom pbuf directly instead of calling
   `pbuf_alloced_custom()` for every frame. The buffer size, PBUF_RAW offset,
   and lifetime are fixed by the RX descriptor ring. This raised 598 to
   609 Mbit/s.
4. Treat the reserved `0x08000000--0x081fffff` GEM DMA window according to its
   page-table mapping: it is shared normal non-cacheable memory, so the Zynq
   specialization of `macb_dma_flush()`/`macb_dma_inval()` performs no cache
   maintenance. Existing DMB/WMB ownership barriers remain. This raised 609 to
   626 Mbit/s.
5. Remove the second, redundant DMB before exposing RX payload to lwIP. The DMB
   immediately after observing descriptor ownership remains and orders both
   descriptor control and payload reads. The current 10-second result is
   629 Mbit/s.
6. Keep the existing 512 RX / 128 TX rings, zero-copy RX custom pbufs, GEM RX
   checksum offload, 1024 TCPIP input-message pool, direct core-locked input,
   and approximately 200 us GEM interrupt moderation.

The current source must still receive a 60-second test followed by a 1000-ping
test before treating 629 Mbit/s as the new long-duration acceptance baseline.
The board is presently running this image and the host interface is MTU 1500.

Rejected experiments that must not be repeated without a new design reason:

- Zynq GEM v2 jumbo/MTU 8000 or 9000: the hardware does not acknowledge jumbo
  TCP segments and its jumbo maximum-length register reads zero.
- Per-packet tcpip mailbox split across CPU1/CPU0: 341 Mbit/s.
- Whole-image or RT-Thread `DefineGroup`-based local `-O3`: startup exception in
  `_workqueue_thread_entry`; `DefineGroup` flag scope contaminated other files.
- Merely adding `RT_DMA_F_NOCACHE` while retaining the generic DMA sync call
  stack: 461 Mbit/s. The retained implementation instead removes those calls
  only in the Zynq specialization whose fixed window is known non-cacheable.
- Removing only RX payload invalidate while leaving generic descriptor sync:
  596 Mbit/s, effectively no improvement over the 598 Mbit/s CPU1 baseline.
- More TCP window/ring-size tuning and 200 us versus 50 us interrupt moderation:
  no material gain after the current bottleneck moved into per-frame processing.

Continue in this order:

1. Run `iperf -c 192.168.1.30 -p 5001 -t 60 -i 5`, then
   `ping -c 1000 -i 0.01 192.168.1.30`. Revert the last DMB removal if any data
   corruption, TCP retransmission burst, or descriptor stall appears.
2. Add lightweight PMU counters around `macb_eth_rx()`, `ethernet_input()`, and
   `tcp_input()` and print only aggregate cycles after the run. Do not print per
   packet. CPU1 is saturated; optimize the largest measured stage.
3. Inspect the RX fast path for remaining non-inlined calls and duplicated
   validation. Likely candidates are descriptor helper calls, pbuf header
   manipulation, custom-pbuf free/refill, and core mutex take/release. Change
   one cost center per image and retain only measured gains.
4. Prototype batching only if single-core reductions cannot close the gap. A
   useful design must enqueue a whole list/ring span with one cross-core wakeup;
   the already-tested one-message-per-packet tcpip path is too expensive.
5. If approximately 940 Mbit/s remains mandatory after software fast-path work,
   evaluate a purpose-built raw TCP receive benchmark or a newer lwIP version
   in a separate branch. Do not call a raw Ethernet/UDP number a TCP result.

The success criterion remains approximately 940 Mbit/s TCP payload at MTU 1500,
not merely a PHY link report of 1000 Mbit/s. The 629 Mbit/s result above is a
historical intermediate result; the later completion record below supersedes
it for host-to-board RX. Board-to-host TX remains separate work.

### GEM0 gigabit completion record (2026-08-23)

The remaining limit was found and removed by profiling with Cortex-A9 PMU
cycle counters around the RX fast path. The counters are accumulated by the
`erx` thread (CPU1) and reported over TCP 5003 by the BSP test-control service
(see `applications/test_ctrl.c`); TCP 5002 triggers a Zynq PS soft reset.

The per-frame breakdown at 617--770 Mbit/s was:

```text
drv (macb_eth_rx)          ~0.4 us
TCPIP core lock            ~1.8 us
ethernet_input+ip+tcp_pre  ~3.1 us   (non-cacheable DMA header reads)
tcp_process                ~0.8 us
app callback + pbuf free   ~2.0 us
empty-ACK TX (tcp_output)  ~5.6 us   (every 2nd frame, ~11 us per ACK)
```

The empty-ACK TX dominated: with the TX thread enabled,
`ethernetif_linkoutput()` handed every ACK to the `etx` thread on CPU0 through
a mailbox plus a cross-core completion, costing about 17 us per ACK. The
retained changes, each measured on the board:

1. `.config` `CONFIG_RT_LWIP_TCP_WND=524288` plus `LWIP_WND_SCALE 1` /
   `TCP_RCV_SCALE 4` in `lwipopts.h`. RFC 1323 window scaling was previously
   disabled, so the 64 KiB `TCP_WND` cap (`65535 / 0.83 ms RTT ~= 630 Mbit/s`)
   was the true first wall, not per-frame processing.
2. `CONFIG_LWIP_NO_TX_THREAD=y`: ACK transmission runs directly in the `erx`
   thread (CPU1) under the lwIP core lock instead of the CPU0 `etx` thread
   round trip. The GEM TX interrupt is disabled for
   `SOC_XILINX_ZYNQ7000` (`macb_gem_commit_irq_enable` /
   `macb_gem_write_traffic_ier`): TX completion is polled inline by
   `macb_eth_tx()`, so the TCOMP interrupt would only race the `erx` thread's
   TX cleanup across CPUs. This raised the result to about 770 Mbit/s.
3. `TCP_ACK_EVERY_NTH 4` (new `tcp_pcb.ack_cnt` counter and `tcp_ack` macro
   variant) plus `TCP_WND_UPDATE_THRESHOLD` raised to `TCP_MSS * 16`: one data
   ACK per four segments instead of the stock every-second alternating scheme,
   and no separate window-update ACK every four segments. This removed the
   ACK-rate ceiling and reached the wire limit.

The 2026-08-23 acceptance run on the retained image:

```text
iperf -c 192.168.1.30 -p 5001 -t 60 -i 5
0.0000-60.0190 sec  6.63 GBytes  949 Mbits/sec     (host -> board, no stall)

iperf2-compatible reverse (custom lwiperf client, board -> host, 60 s)
TOTAL: 1918.28 MB in 61.37s = 250.1 Mbit/s         (stable, no stall)

ping -c 1000 -i 0.01 192.168.1.30
1000 packets transmitted, 1000 received, 0% loss, avg 0.79 ms
```

949 Mbit/s is the MTU-1500 wire limit for Gigabit Ethernet
(`1 Gbit/s * 1460 / 1538`), so the host-to-board direction is complete. The
board-to-host direction is CPU-bound at about 250 Mbit/s (the TX path copies
each 1460-byte segment into the non-cacheable DMA window and runs
`tcp_write`/`tcp_output` per segment on the `erx` thread); it is stable but
not wire rate. Reaching 940 Mbit/s there would require TX checksum offload,
a cached TX copy path, or TX-side batching, which is separate follow-up work.

### TX (board-to-host) optimization record (2026-08-23)

PMU profiling of the reverse path showed 46 us per segment on the `erx`
thread: `tcp_process` ACK handling ~7-10 us (incl. freeing the previous
segment), `tcp_write` ~7-10 us (incl. a 1500-byte pbuf allocation per
segment), `tcp_output` ~15-20 us (software TCP checksum + ARP + driver), the
driver's 1460-byte copy into the non-cacheable TX window ~7.3 us, plus lock
and interrupt overhead. 949 Mbit/s would need 12.4 us per segment.

Retained changes (each measured on the board):

1. `TCP_SND_BUF` 262144 and `RT_LWIP_TCP_SEG_NUM` 1024 in `.config`: removes
   the 64 KiB send-window wall (was not the active limit, but is required for
   any faster TX path).
2. Zero-copy TX in `macb_eth_tx`: when the outgoing frame is a single pbuf
   (not chained, not custom), the GEM DMAs directly from the pbuf payload
   after a cache clean, instead of copying 1460 bytes into the non-cacheable
   TX window. The GEM accepts the resulting word-unaligned frame start
   (`LWIP_MEM_ALIGN` + the 14-byte ethernet header always lands at
   `base + 2 mod 4`). Driver copy cost dropped 7.3 us to ~2.0 us (cache
   clean); reverse throughput rose from 250 to 260 Mbit/s.
3. `MEM_USE_POOLS 1` + `MEMP_USE_CUSTOM_POOLS 1` and `src/core/mem.c` added
   back to the lwIP build (the RT-Thread port otherwise routes `mem_malloc`
   to `rt_malloc`): pooled allocation. This did not move the reverse number
   measurably, but removes per-segment heap allocation from the hot path.

Rejected experiments (with reasons):

- GEM `NCFGR.TXCOEN` (bit 11) TX checksum offload with
  `CHECKSUM_GEN_IP/TCP 0`: the board lost connectivity twice (JTAG verified
  the bit was set); reverted. The GEM's TX checksum generation is
  incompatible with this driver/lwIP combination.
- lwiperf chunked writes (8x MSS per `tcp_write`): throughput dropped to
  169 Mbit/s; reverted.
- Aligning the frame start via `PBUF_LINK_HLEN` (16/18/23): no effect,
  because `LWIP_MEM_ALIGN` realigns the pbuf payload and the 14-byte ethernet
  header leaves the frame start at `base + 2 (mod 4)` regardless.

2026-08-23 reverse acceptance after the retained changes:

```text
reverse (board -> host, 60 s): 1998.19 MB in 61.37s = 260.5 Mbit/s (stable)
forward (host -> board, 10 s): 937-941 Mbit/s (unchanged)
```

A final lwIP 2.0.3 optimization pass added `TCP_CHECKSUM_ON_COPY 1` (compute
the TCP checksum during `tcp_write` instead of re-walking the 1460-byte
payload in `tcp_output`) and `TCP_QUEUE_OOSEQ 0` (no out-of-order queueing on
this point-to-point link). PMU per-segment cost dropped from 43.8 us to
19.9 us (tcp_process 2.4 + tcp_write 2.0 + sent callback 3.4 + tcp_output 6.7
+ driver 2.2), and the reverse number rose to 271.8 Mbit/s:

```text
reverse (board -> host, 60 s): 2084.87 MB in 61.37s = 271.8 Mbit/s (stable)
forward (host -> board, 20 s): 949-950 Mbit/s (unchanged)
```

The board-side processing capability is now ~587 Mbit/s (19.9 us/segment),
but the observed segment rate is ~23k/s (271.8 Mbit/s). The host receiver
(`ss -tin`) shows `rcv_space ~= 118 KiB` and quick ACKs per segment, so the
host ACK supply throttles the board's send window to one segment per ACK
(23k ACKs/s). lwIP 2.0.3 cannot batch the host's ACKs the way the board
batches its own (TCP_ACK_EVERY_NTH on the RX side), and the host side is not
controllable from this BSP.

A host-side discovery raised the reverse number further: the Linux receiver
was ACKing one segment per ACK (quick ACK), throttling the board's send
window to one segment per ACK. Setting `TCP_QUICKACK=0` on the data socket
of the test client makes the host use delayed ACKs (one ACK per two
segments), so the board advances two segments per ACK:

```text
reverse with QUICKACK=0 (board -> host, 15 s): 337 Mbit/s (up from 272)
```

The board's per-segment cost is ~20 us (587 Mbit/s capability), but the
single-connection steady state is set by the host ACK supply (delayed ACK,
two segments per ACK) and the board's ~51 us per ACK (2 segments): the
single-connection ceiling is ~458 Mbit/s, measured 337. Two parallel
connections share the board CPU and sum to ~429 Mbit/s, confirming the CPU
is the shared bottleneck rather than the link. The host receive window is not
limiting (SO_RCVBUF 256 KiB vs 2 MiB measured identical). Further host-side
checks bounded the steady state: a fast non-blocking receive loop
(`recv_into`, 1 MiB buffers) measured the same 337 Mbit/s, so
the test application is not the limit; `SO_RCVBUF` 256 KiB vs 2 MiB measured
identical, and the in-flight data (101 KiB at 337 Mbit/s x 2.4 ms RTT) stays
below the 142 KiB receive window, so the window is not the limit either. The
board CPU is ~74% busy per ACK. The steady state is set by the host Linux
delayed-ACK cadence (one ACK per two segments, ~14.4k ACK/s) interacting
with the board's per-ACK processing; raising `net.ipv4.tcp_rmem` is expected
to be neutral for the same reason. Final board-to-host result: 337 Mbit/s
stable for 60 s with the host QUICKACK disabled.

Follow-up TX work on 2026-08-25 instrumented the lwiperf refill path through
TCP 5003. The 328.5 Mbit/s baseline made 215,379 sent callbacks in 15 seconds,
acknowledged 2924 bytes per callback, and queued only 2.003 segments per
`tcp_output`. Deferring queue refill until 16, 32, and 64 MSS had been ACKed
measured 375.6, 378.8, and 384.8 Mbit/s respectively. The retained 64-MSS
setting reduces about 215k refill/output calls to about 8k per 15-second run.

The default checksum-on-copy implementation copied into the new pbuf and then
checksummed that destination. On N1, checksumming the identical cache-hot
immutable source after the copy raised the 64-MSS result to 477.5 Mbit/s. A
second experiment cached the source checksum and measured 472.6 Mbit/s, so it
was rejected. GEM scatter-gather using separate header and payload descriptors
was also rejected: the transfer repeatedly stopped at 94,924 bytes and the TX
ring ceased returning descriptors. The driver and `LWIP_NETIF_TX_SINGLE_PBUF`
configuration were restored after that test.

The retained image then completed a 60-second board-to-host run with
3,711,457,264 bytes in 61.378 seconds (483.7 Mbit/s). A post-test check
received 20/20 ICMP replies with 0% loss and 0.804 ms average latency.

The next pass batches up to 32 already-contiguous GEM TX descriptors per
kick/completion poll while retaining a single descriptor per Ethernet frame.
It also uses Cortex-A9 D-cache clean-only for TX buffers; invalidating a buffer
that is only read by DMA was unnecessary and doubled the per-line maintenance
operations. The combined image completed 3,990,854,544 bytes in 61.369 seconds
(520.2 Mbit/s), followed by 20/20 ICMP replies with 0% loss and 0.804 ms average
latency. PMU driver cache maintenance fell from about 1284 to 864 cycles per
segment. Using newlib `memcpy` caused a sustained-test network failure and an
integer ARM `LDM/STM` copy measured no lower `tcp_write` cost; both copy
experiments were rejected.

An 8-entry immutable-payload checksum cache reached 540.7 Mbit/s for 15
seconds and reduced `tcp_write` from about 6355 to 4981 cycles per segment,
but its 60-second run stopped after 2,040,182,124 bytes and left networking
unresponsive. It was rejected and the retained image remains the 520.2
Mbit/s ring-batching plus cache-clean-only version.

Replacing the SMP `SYS_ARCH_PROTECT` recursive mutex globally with an IRQ-save
spinlock exposed substantial lock overhead: a 5-second reverse run reached
714.0 Mbit/s. It is not safe, however. The first version stopped after
833,746,164 bytes during a 15-second run and left networking unresponsive.
A per-CPU nesting-aware version removed same-CPU recursive acquisition and
reached 682.0 Mbit/s for 5 seconds, but stopped after 875,490,484 bytes in the
15-second run and again lost all ICMP replies. This indicates a remaining
cross-CPU lock-order or long IRQ-disabled dependency, not merely recursive
entry. Both variants were rejected and the recursive mutex was restored.

Do not retry a global spinlock for `SYS_ARCH_PROTECT`. The next low-impact
direction is to leave lwIP core logic and its global protection semantics
unchanged, then reduce only the measured hot-path overhead: use RT-Thread's
existing SMP-safe fixed-block allocator for the TX pbuf/memp pools, measure
the `memp_malloc` and `pbuf_alloc` portions independently, and retain a change
only after 5-, 15-, and 60-second reverse runs plus post-test ping. A second
candidate is driver-local preallocation/recycling of contiguous TX staging
buffers; it avoids changing lwIP locking and does not require a new TCP path.

The next TX direction is single-descriptor ring batching: prepare several
complete contiguous frames, publish their descriptors together, and perform
one GEM kick/completion poll per batch. Raising the lwiperf ACK threshold again
cannot remove the remaining per-segment pbuf allocation/copy or per-descriptor
driver cost. For standard-MTU TCP over gigabit Ethernet, the practical payload
target is approximately 940--950 Mbit/s rather than a literal 1000 Mbit/s.

Reaching 949 Mbit/s board-to-host still requires a materially different TX
path. Within the currently tested RT-Thread macb plus lwIP API design, the
remaining candidate is a purpose-built batched raw TCP sender; the tested
lwIP 2.0.3/2.1.2/2.2.0 configurations and host-side settings did not reach
line rate. Porting the Xilinx `xemacps` TX architecture, adding true
scatter-gather batching, or implementing compatible GEM TX checksum offload
remain broader alternatives. This separate effort is not attempted here.

### Why Xilinx standalone/FreeRTOS reports 935 Mbit/s TCP TX

The AMD/Xilinx Standalone LWIP performance table
(https://xilinx-wiki.atlassian.net/wiki/pages/diffpages.action?originalId=62521688&pageId=62292299)
lists TCP Tx 935 Mbit/s at MTU 1500 on Zynq-7000. That number is not
reproducible by the lwIP version alone: Xilinx uses lwIP 2.2.0 with the
xemacps driver and their own host/test setup. Porting lwIP 2.2.0 into this
BSP (RT_USING_LWIP220) and re-applying the ACK-batching patch measured
per-ACK cost of 76 us (CPU1 saturated at ~97%) and 300 Mbit/s reverse --
worse than the 2.0.3 optimized 51 us / 337 Mbit/s. The remaining gap to 935
is dominated by the host delayed-ACK cadence and the per-segment CPU cost of
the lwIP TX chain in this RT-Thread environment, not by the lwIP minor
version. The lwIP 2.2.0 experiment is kept separately and is not part of the
main N1 BSP commit.


### lwIP 2.1.2 experiment (2026-08-23)

Switching the BSP to the in-tree lwIP 2.1.2 (`CONFIG_RT_USING_LWIP212`) was
attempted as the first "different TX stack" option. The port was adapted for
2.1.2 (pbuf `type_internal` API in `macb.c`, `src/core/mem.c` and lwiperf
added to the 2.1.2 SConscript, `mem_overflow_*` guards in `sys_arch.c`), and
the ACK-batching patch (`TCP_ACK_EVERY_NTH`, `tcp_pcb.ack_cnt`) plus the PMU
counters were ported to the 2.1.2 tree. Results on the board:

```text
lwIP 2.1.2 + ACK batching: forward 938-942 Mbit/s, reverse 237 Mbit/s
lwIP 2.0.3 + all patches:  forward 949 Mbit/s,    reverse 271 Mbit/s
```

PMU showed 2.1.2's per-segment TX cost is higher than 2.0.3's optimized path
(48 us vs 20 us: tcp_process 9.2 vs 2.4, tcp_write 8.6 vs 2.0, sent 16.1 vs
3.4, tcp_output 14.8 vs 6.7 us), so 2.1.2 does not help the board-to-host
direction on this CPU. The BSP stays on 2.0.3; the 2.1.2 port changes remain
in the tree (config gated) for future evaluation.

### Remote test loop

The board no longer needs the serial console for the throughput loop. The
eMMC `/boot/uEnv.txt` boot switch is honored by the existing `boot.scr`:

```text
uenvcmd=setenv ipaddr 192.168.1.10; setenv serverip 192.168.1.5; \
if tftpboot 0x02000000 boot_linux.flag; then echo BOOT_LINUX_FLAG; \
elif tftpboot 0x00200000 rtthread.bin; then dcache flush; go 0x00200000; \
else echo RTT_BOOT_FAILED; fi
```

- `/tftpboot/boot_linux.flag` absent: U-Boot TFTP-loads `rtthread.bin` and
  `go`es into RT-Thread (default).
- `/tftpboot/boot_linux.flag` present: U-Boot continues to PetaLinux on eMMC.

While RT-Thread runs, TCP 5002 triggers the Zynq PS soft reset (SLCR unlock
plus `SLCR_PSS_RST_CTRL`), so the loop is: edit image, `cp` to `/tftpboot`,
connect to 5002 to reboot, run `iperf`/`ping`, read the aggregate PMU
counters from TCP 5003.
