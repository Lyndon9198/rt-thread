/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

#ifdef BSP_USING_GEM0
#include <lwip/apps/lwiperf.h>

static void *iperf_session;

static void iperf_report(void *arg, enum lwiperf_report_type report_type,
        const ip_addr_t *local_addr, u16_t local_port,
        const ip_addr_t *remote_addr, u16_t remote_port,
        u32_t bytes, u32_t duration_ms, u32_t bandwidth_kbit)
{
    RT_UNUSED(arg);
    RT_UNUSED(local_addr);
    RT_UNUSED(local_port);
    RT_UNUSED(remote_addr);
    RT_UNUSED(remote_port);
    rt_kprintf("iperf result: type=%d, %u bytes, %u ms, %u Kbit/s\n",
            report_type, bytes, duration_ms, bandwidth_kbit);
    iperf_session = RT_NULL;
}

static int iperf_server(void)
{
    if (iperf_session)
    {
        return -RT_EBUSY;
    }
    iperf_session = lwiperf_start_tcp_server_default(iperf_report, RT_NULL);
    return iperf_session ? RT_EOK : -RT_ERROR;
}
MSH_CMD_EXPORT(iperf_server, start an iperf2-compatible TCP server);

#endif

int main(void)
{
    rt_kprintf("RT-Thread on ALIENTEK Navigator ZYNQ7020, CPU%d.\n",
               rt_hw_cpu_id());
    return RT_EOK;
}
