/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtdevice.h>

#include <board.h>

#ifdef BSP_USING_WDT

#define WDT_ZMR                 __REG32(ZYNQ_WDT_BASE + 0x00U)
#define WDT_CCR                 __REG32(ZYNQ_WDT_BASE + 0x04U)
#define WDT_RESTART             __REG32(ZYNQ_WDT_BASE + 0x08U)

#define WDT_ZMR_WDEN            (1U << 0)
#define WDT_ZMR_RSTEN           (1U << 1)
#define WDT_ZMR_KEY             0x00ABC000U
#define WDT_CCR_CRV_SHIFT       2U
#define WDT_CCR_CRV_MAX         0xFFFU
#define WDT_CCR_PRESCALE_4096   0x3U
#define WDT_CCR_KEY             0x00920000U
#define WDT_RESTART_KEY         0x00001999U
#define WDT_COUNTER_DIVIDER     (4096ULL * 4096ULL)

static struct rt_watchdog_device zynq_wdt;
static rt_uint32_t zynq_wdt_timeout = 10U;

static rt_err_t zynq_wdt_set_timeout(rt_uint32_t timeout)
{
    rt_uint64_t ticks;
    rt_uint32_t reload;

    if (timeout == 0U)
    {
        return -RT_EINVAL;
    }

    ticks = (rt_uint64_t)timeout * ZYNQ_WDT_CLOCK_HZ;
    reload = (rt_uint32_t)((ticks + WDT_COUNTER_DIVIDER - 1U) /
                           WDT_COUNTER_DIVIDER);
    if (reload == 0U || reload > WDT_CCR_CRV_MAX)
    {
        return -RT_EINVAL;
    }

    WDT_CCR = WDT_CCR_KEY | (reload << WDT_CCR_CRV_SHIFT) |
              WDT_CCR_PRESCALE_4096;
    zynq_wdt_timeout = timeout;
    WDT_RESTART = WDT_RESTART_KEY;
    return RT_EOK;
}

static rt_err_t zynq_wdt_init(rt_watchdog_t *wdt)
{
    RT_UNUSED(wdt);
    WDT_ZMR = WDT_ZMR_KEY;
    return zynq_wdt_set_timeout(zynq_wdt_timeout);
}

static rt_err_t zynq_wdt_control(rt_watchdog_t *wdt, int command, void *argument)
{
    RT_UNUSED(wdt);

    switch (command)
    {
    case RT_DEVICE_CTRL_WDT_GET_TIMEOUT:
        if (!argument)
        {
            return -RT_EINVAL;
        }
        *(rt_uint32_t *)argument = zynq_wdt_timeout;
        break;
    case RT_DEVICE_CTRL_WDT_SET_TIMEOUT:
        if (!argument)
        {
            return -RT_EINVAL;
        }
        return zynq_wdt_set_timeout(*(rt_uint32_t *)argument);
    case RT_DEVICE_CTRL_WDT_KEEPALIVE:
        WDT_RESTART = WDT_RESTART_KEY;
        break;
    case RT_DEVICE_CTRL_WDT_START:
        WDT_RESTART = WDT_RESTART_KEY;
        WDT_ZMR = WDT_ZMR_KEY | WDT_ZMR_RSTEN | WDT_ZMR_WDEN;
        break;
    case RT_DEVICE_CTRL_WDT_STOP:
        WDT_ZMR = WDT_ZMR_KEY;
        break;
    default:
        return -RT_EINVAL;
    }
    return RT_EOK;
}

static const struct rt_watchdog_ops zynq_wdt_ops =
{
    zynq_wdt_init,
    zynq_wdt_control,
};

static int zynq_wdt_register(void)
{
    zynq_wdt.ops = &zynq_wdt_ops;
    return rt_hw_watchdog_register(&zynq_wdt, "wdt",
                                   RT_DEVICE_FLAG_RDWR, RT_NULL);
}
INIT_BOARD_EXPORT(zynq_wdt_register);

#endif /* BSP_USING_WDT */
