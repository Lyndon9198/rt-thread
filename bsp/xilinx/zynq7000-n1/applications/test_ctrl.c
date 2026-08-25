/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * test_ctrl.c - N1 GEM0 gigabit test harness.
 *
 * TCP 5002: any payload triggers a Zynq-7000 PS soft reset, re-entering the
 *           QSPI FSBL/U-Boot chain. With the board's uEnv.txt boot switch
 *           this lets the throughput test loop reboot the board remotely
 *           while RT-Thread runs, without touching the serial console.
 * TCP 5003: reports the aggregate RX fast-path PMU counters (frames, driver
 *           cycles, lwIP input cycles) accumulated by the erx thread.
 */

#include <rtthread.h>
#include <rthw.h>

#include <lwip/api.h>

#define TEST_CTRL_PORT          5002
#define TEST_STATS_PORT         5003
#define TEST_STATS_COUNT        36
#define ZYNQ_SLCR_UNLOCK        0xF8000008U
#define ZYNQ_SLCR_PSS_RST_CTRL  0xF8000200U

#if defined(SOC_XILINX_ZYNQ7000) && defined(RT_USING_SMP)
extern rt_uint64_t n1_rx_pmu_frames;
extern rt_uint64_t n1_rx_pmu_drv_cycles;
extern rt_uint64_t n1_rx_pmu_input_cycles;
extern rt_uint64_t n1_stack_cycles;
extern rt_uint64_t n1_stack_frames;
extern rt_uint64_t n1_app_cycles;
extern rt_uint64_t n1_proc_cycles;
extern rt_uint64_t n1_ackout_cycles;
extern rt_uint64_t n1_tx_drv_cycles;
extern rt_uint64_t n1_ack_build_cycles;
extern rt_uint64_t n1_ack_send_cycles;
extern rt_uint64_t n1_tx_copy_cycles;
extern rt_uint64_t n1_tx_kick_cycles;
extern rt_uint64_t n1_tx_poll_cycles;
extern rt_uint64_t n1_tcpwrite_cycles;
extern rt_uint64_t n1_tcpwrite_calls;
extern rt_uint64_t n1_sent_cycles;
extern rt_uint64_t n1_tx_zero_calls;
extern rt_uint64_t n1_tx_d_multi;
extern rt_uint64_t n1_tx_d_len;
extern rt_uint64_t n1_tx_d_align;
extern rt_uint64_t n1_tx_d_custom;
extern rt_uint64_t n1_tx_last_payload;
extern rt_uint64_t n1_tx_diag_totlen;
extern rt_uint64_t n1_tx_diag_len;
extern rt_uint64_t n1_tx_diag_next;
extern rt_uint64_t n1_tx_diag_type;
extern rt_uint64_t n1_tx_diag_flags;
extern rt_uint64_t n1_lwiperf_send_calls;
extern rt_uint64_t n1_lwiperf_send_segments;
extern rt_uint64_t n1_lwiperf_send_max_batch;
extern rt_uint64_t n1_lwiperf_send_errmem;
extern rt_uint64_t n1_lwiperf_ack_calls;
extern rt_uint64_t n1_lwiperf_ack_bytes;
extern rt_uint64_t n1_lwiperf_sndbuf_enter;
extern rt_uint64_t n1_lwiperf_sndbuf_exit;
#endif

static void n1_system_reset(void)
{
    rt_base_t level = rt_hw_interrupt_disable();

    /* Zynq-7000 SLCR: unlock, then assert the PS soft reset */
    *(volatile rt_uint32_t *)ZYNQ_SLCR_UNLOCK = 0xDF0D;
    *(volatile rt_uint32_t *)ZYNQ_SLCR_PSS_RST_CTRL = 0x1U;

    for (;;)
    {
        __asm__ volatile ("wfi");
    }
    rt_hw_interrupt_enable(level); /* never reached */
}

static void test_ctrl_entry(void *parameter)
{
    struct netconn *conn;

    RT_UNUSED(parameter);

    conn = netconn_new(NETCONN_TCP);
    if (conn == RT_NULL)
    {
        rt_kprintf("test_ctrl: netconn_new failed\n");
        return;
    }
    if (netconn_bind(conn, IP_ADDR_ANY, TEST_CTRL_PORT) != ERR_OK)
    {
        rt_kprintf("test_ctrl: bind TCP %d failed\n", TEST_CTRL_PORT);
        netconn_delete(conn);
        return;
    }
    netconn_listen(conn);

    rt_kprintf("test_ctrl: reset service listening on TCP %d\n", TEST_CTRL_PORT);

    while (1)
    {
        struct netconn *newconn;
        err_t err = netconn_accept(conn, &newconn);

        if (err != ERR_OK)
        {
            continue;
        }

        /* any payload on this port is the reset command */
        while (1)
        {
            struct netbuf *buf;

            err = netconn_recv(newconn, &buf);
            if (err != ERR_OK)
            {
                break;
            }
            if (buf != RT_NULL)
            {
                netbuf_delete(buf);
            }

            rt_kprintf("test_ctrl: reset requested, resetting SoC\n");
            n1_system_reset();
        }

        netconn_close(newconn);
        netconn_delete(newconn);
    }
}

#if defined(SOC_XILINX_ZYNQ7000) && defined(RT_USING_SMP)
static void test_stats_entry(void *parameter)
{
    struct netconn *conn;

    RT_UNUSED(parameter);

    conn = netconn_new(NETCONN_TCP);
    if (conn == RT_NULL)
    {
        rt_kprintf("test_ctrl: stats netconn_new failed\n");
        return;
    }
    if (netconn_bind(conn, IP_ADDR_ANY, TEST_STATS_PORT) != ERR_OK)
    {
        rt_kprintf("test_ctrl: bind TCP %d failed\n", TEST_STATS_PORT);
        netconn_delete(conn);
        return;
    }
    netconn_listen(conn);

    rt_kprintf("test_ctrl: stats service listening on TCP %d\n", TEST_STATS_PORT);

    while (1)
    {
        struct netconn *newconn;
        /* binary report: u32 magic + frames + drv + input + stack + app
         * + proc + ackout + tx_drv + ack_build + ack_send + stack_frames
         * (all u64 LE) */
        rt_uint8_t buf[4 + 8 * TEST_STATS_COUNT];
        rt_uint64_t vals[TEST_STATS_COUNT];
        int i;

        if (netconn_accept(conn, &newconn) != ERR_OK)
        {
            continue;
        }

        /* tolerate clients that connect and close immediately: never block
         * the accept loop on a bad peer (write with a byte counter, and
         * bound the close wait) */
        netconn_set_nonblocking(newconn, 1);
        netconn_set_sendtimeout(newconn, 500);

        buf[0] = 'N'; buf[1] = '1'; buf[2] = 'P'; buf[3] = 'M';
        vals[0] = n1_rx_pmu_frames;
        vals[1] = n1_rx_pmu_drv_cycles;
        vals[2] = n1_rx_pmu_input_cycles;
        vals[3] = n1_stack_cycles;
        vals[4] = n1_app_cycles;
        vals[5] = n1_proc_cycles;
        vals[6] = n1_ackout_cycles;
        vals[7] = n1_tx_drv_cycles;
        vals[8] = n1_ack_build_cycles;
        vals[9] = n1_ack_send_cycles;
        vals[10] = (rt_uint64_t)n1_stack_frames;
        vals[11] = n1_tx_copy_cycles;
        vals[12] = n1_tx_kick_cycles;
        vals[13] = n1_tx_poll_cycles;
        vals[14] = n1_tcpwrite_cycles;
        vals[15] = n1_tcpwrite_calls;
        vals[16] = n1_sent_cycles;
        vals[17] = n1_tx_zero_calls;
        vals[18] = n1_tx_d_multi;
        vals[19] = n1_tx_d_len;
        vals[20] = n1_tx_d_align;
        vals[21] = n1_tx_d_custom;
        vals[22] = n1_tx_last_payload;
        vals[23] = n1_tx_diag_totlen;
        vals[24] = n1_tx_diag_len;
        vals[25] = n1_tx_diag_next;
        vals[26] = n1_tx_diag_type;
        vals[27] = n1_tx_diag_flags;
        vals[28] = n1_lwiperf_send_calls;
        vals[29] = n1_lwiperf_send_segments;
        vals[30] = n1_lwiperf_send_max_batch;
        vals[31] = n1_lwiperf_send_errmem;
        vals[32] = n1_lwiperf_ack_calls;
        vals[33] = n1_lwiperf_ack_bytes;
        vals[34] = n1_lwiperf_sndbuf_enter;
        vals[35] = n1_lwiperf_sndbuf_exit;
        for (i = 0; i < TEST_STATS_COUNT; i++)
        {
            rt_uint64_t v = vals[i];
            rt_uint8_t *p = &buf[4 + i * 8];
            int b;

            for (b = 0; b < 8; b++)
            {
                p[b] = (rt_uint8_t)(v & 0xff);
                v >>= 8;
            }
        }

        {
            size_t written = 0;

            (void)netconn_write_partly(newconn, buf, (size_t)sizeof(buf),
                                       NETCONN_COPY, &written);
        }
        (void)netconn_close(newconn);
        netconn_delete(newconn);
    }
}
#endif /* SOC_XILINX_ZYNQ7000 && RT_USING_SMP */

static int test_ctrl_start(void)
{
    rt_thread_t thread;

    thread = rt_thread_create("tctl", test_ctrl_entry, RT_NULL,
                              4096, 20, 10);
    if (thread == RT_NULL)
    {
        return -RT_ENOMEM;
    }
    rt_thread_startup(thread);

#if defined(SOC_XILINX_ZYNQ7000) && defined(RT_USING_SMP)
    thread = rt_thread_create("tstat", test_stats_entry, RT_NULL,
                              4096, 20, 10);
    if (thread == RT_NULL)
    {
        return -RT_ENOMEM;
    }
    rt_thread_startup(thread);
#endif

    return RT_EOK;
}
INIT_APP_EXPORT(test_ctrl_start);
