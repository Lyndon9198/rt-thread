/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

#ifdef RT_LWIP_USING_LWIPERF
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
        rt_kprintf("iperf server is already listening on TCP 5001\n");
        return -RT_EBUSY;
    }

    iperf_session = lwiperf_start_tcp_server_default(iperf_report, RT_NULL);
    if (!iperf_session)
        return -RT_ERROR;

    rt_kprintf("iperf2-compatible server listening on TCP 5001\n");
    return RT_EOK;
}
MSH_CMD_EXPORT(iperf_server, start the iperf2-compatible TCP server on port 5001);
#endif

static void smp_test_entry(void *parameter)
{
    rt_ubase_t expected_cpu = (rt_ubase_t)parameter;

    rt_thread_mdelay(expected_cpu * 20U);
    rt_kprintf("SMP test: expected CPU%u, running on CPU%d.\n",
               expected_cpu, rt_hw_cpu_id());
}

static int smp_test(void)
{
    rt_thread_t thread;
    rt_ubase_t cpu;

    for (cpu = 0; cpu < RT_CPUS_NR; cpu++)
    {
        thread = rt_thread_create(cpu == 0 ? "smp0" : "smp1",
                                  smp_test_entry, (void *)cpu,
                                  2048, 15, 10);
        if (thread == RT_NULL)
        {
            return -RT_ENOMEM;
        }

        rt_thread_control(thread, RT_THREAD_CTRL_BIND_CPU, (void *)cpu);
        rt_thread_startup(thread);
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(smp_test, run one test thread on each CPU);

#ifdef BSP_USING_GPIO
#include <rtdevice.h>

static int gpio_test(void)
{
    static const rt_base_t leds[] = {54, 118, 119};
    static const rt_base_t keys[] = {11, 12, 55, 56, 57};
    rt_size_t index;
    int cycle;

    for (index = 0; index < sizeof(leds) / sizeof(leds[0]); index++)
        rt_pin_mode(leds[index], PIN_MODE_OUTPUT);

    for (index = 0; index < sizeof(keys) / sizeof(keys[0]); index++)
        rt_pin_mode(keys[index], PIN_MODE_INPUT_PULLUP);

    for (cycle = 0; cycle < 3; cycle++)
    {
        for (index = 0; index < sizeof(leds) / sizeof(leds[0]); index++)
            rt_pin_write(leds[index], PIN_HIGH);
        rt_thread_mdelay(250);
        for (index = 0; index < sizeof(leds) / sizeof(leds[0]); index++)
            rt_pin_write(leds[index], PIN_LOW);
        rt_thread_mdelay(250);
    }

    rt_kprintf("keys: MIO11=%d MIO12=%d EMIO55=%d EMIO56=%d EMIO57=%d\n",
               rt_pin_read(keys[0]), rt_pin_read(keys[1]),
               rt_pin_read(keys[2]), rt_pin_read(keys[3]),
               rt_pin_read(keys[4]));
    return RT_EOK;
}
MSH_CMD_EXPORT(gpio_test, blink the three N1 LEDs and read five keys);
#endif

#ifdef RT_USING_OFW
#include <drivers/ofw.h>
#include <drivers/platform.h>

static int ofw_test(void)
{
    struct rt_ofw_node *np;
    struct rt_ofw_node *phy;
    rt_uint32_t reg = 0;
    rt_uint32_t type = 0;
    rt_uint32_t irq = 0;

    np = rt_ofw_find_node_by_path("/amba/ethernet@e000b000");
    if (!np)
    {
        rt_kprintf("GEM0 OFW node: missing\n");
        return -RT_ENOSYS;
    }

    rt_ofw_prop_read_u32_index(np, "reg", 0, &reg);
    rt_ofw_prop_read_u32_index(np, "interrupts", 0, &type);
    rt_ofw_prop_read_u32_index(np, "interrupts", 1, &irq);
    phy = rt_ofw_parse_phandle(np, "phy-handle", 0);

    rt_kprintf("GEM0 OFW node: ok, reg=0x%08x, irq=%u, phy=%s\n",
               reg, type == 0 ? irq + 32U : irq + 16U,
               phy ? phy->full_name : "missing");
    if (phy)
        rt_ofw_node_put(phy);
    rt_ofw_node_put(np);

    rt_kprintf("GEM0 platform device: %s, driver: %s\n",
               np->dev ? "created" : "missing",
               np->dev && np->dev->drv ? np->dev->drv->parent.name : "unbound");
    rt_kprintf("GEM0 probe retry: %s\n", rt_strerror(rt_platform_ofw_request(np)));
    rt_kprintf("network device e0: %s\n",
               rt_device_find("e0") ? "registered" : "missing");
    return RT_EOK;
}
MSH_CMD_EXPORT(ofw_test, inspect the runtime GEM0 device-tree node);
#endif

int main(void)
{
    rt_kprintf("N1 Zynq-7020 RT-Thread SMP started on CPU%d.\n", rt_hw_cpu_id());
#ifdef RT_LWIP_USING_LWIPERF
    iperf_server();
#endif
    return RT_EOK;
}
