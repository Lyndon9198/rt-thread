/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtdevice.h>
#include <drivers/clock_time.h>

#include <board.h>

#ifdef BSP_USING_TTC0_TIMER0

#define TTC_CLOCK_CONTROL       __REG32(ZYNQ_TTC0_TIMER0_BASE + 0x00U)
#define TTC_COUNTER_CONTROL     __REG32(ZYNQ_TTC0_TIMER0_BASE + 0x0CU)
#define TTC_COUNTER_VALUE       __REG32(ZYNQ_TTC0_TIMER0_BASE + 0x18U)
#define TTC_INTERVAL_VALUE      __REG32(ZYNQ_TTC0_TIMER0_BASE + 0x24U)
#define TTC_INTERRUPT_STATUS    __REG32(ZYNQ_TTC0_TIMER0_BASE + 0x54U)
#define TTC_INTERRUPT_ENABLE    __REG32(ZYNQ_TTC0_TIMER0_BASE + 0x60U)

#define TTC_CLOCK_PRESCALE      (1U | (6U << 1))
#define TTC_COUNTER_DISABLE     (1U << 0)
#define TTC_COUNTER_INTERVAL    (1U << 1)
#define TTC_COUNTER_RESET       (1U << 4)
#define TTC_INTERRUPT_INTERVAL  (1U << 0)
#define TTC_COUNTER_FREQUENCY   (ZYNQ_TTC_INPUT_CLOCK_HZ / 128U)

static rt_clock_timer_t zynq_ttc_timer;

static void zynq_ttc_init(rt_clock_timer_t *timer, rt_uint32_t state)
{
    RT_UNUSED(timer);
    TTC_COUNTER_CONTROL = TTC_COUNTER_DISABLE;
    TTC_INTERRUPT_ENABLE = 0U;
    (void)TTC_INTERRUPT_STATUS;
    if (state)
    {
        TTC_CLOCK_CONTROL = TTC_CLOCK_PRESCALE;
        TTC_COUNTER_CONTROL = TTC_COUNTER_DISABLE | TTC_COUNTER_RESET;
    }
}

static rt_err_t zynq_ttc_start(rt_clock_timer_t *timer, rt_uint32_t count,
                               rt_clock_timer_mode_t mode)
{
    RT_UNUSED(timer);
    if (count == 0U || count > 0xFFFFU ||
        (mode != CLOCK_TIMER_MODE_ONESHOT && mode != CLOCK_TIMER_MODE_PERIOD))
    {
        return -RT_EINVAL;
    }

    TTC_COUNTER_CONTROL = TTC_COUNTER_DISABLE;
    TTC_INTERVAL_VALUE = count;
    (void)TTC_INTERRUPT_STATUS;
    TTC_INTERRUPT_ENABLE = TTC_INTERRUPT_INTERVAL;
    TTC_COUNTER_CONTROL = TTC_COUNTER_INTERVAL | TTC_COUNTER_RESET;
    return RT_EOK;
}

static void zynq_ttc_stop(rt_clock_timer_t *timer)
{
    RT_UNUSED(timer);
    TTC_COUNTER_CONTROL = TTC_COUNTER_DISABLE;
    TTC_INTERRUPT_ENABLE = 0U;
    (void)TTC_INTERRUPT_STATUS;
}

static rt_uint32_t zynq_ttc_count_get(rt_clock_timer_t *timer)
{
    RT_UNUSED(timer);
    return TTC_COUNTER_VALUE & 0xFFFFU;
}

static rt_err_t zynq_ttc_control(rt_clock_timer_t *timer, rt_uint32_t command,
                                 void *argument)
{
    RT_UNUSED(timer);
    if (command != CLOCK_TIMER_CTRL_FREQ_SET || !argument ||
        *(rt_uint32_t *)argument != TTC_COUNTER_FREQUENCY)
    {
        return -RT_ENOSYS;
    }
    return RT_EOK;
}

static const struct rt_clock_timer_info zynq_ttc_info =
{
    TTC_COUNTER_FREQUENCY,
    TTC_COUNTER_FREQUENCY,
    0xFFFFU,
    CLOCK_TIMER_CNTMODE_UP,
};

static const struct rt_clock_timer_ops zynq_ttc_ops =
{
    zynq_ttc_init,
    zynq_ttc_start,
    zynq_ttc_stop,
    zynq_ttc_count_get,
    zynq_ttc_control,
};

static void zynq_ttc_isr(int vector, void *parameter)
{
    rt_uint32_t status = TTC_INTERRUPT_STATUS;

    RT_UNUSED(vector);
    RT_UNUSED(parameter);
    if (status & TTC_INTERRUPT_INTERVAL)
    {
        rt_clock_timer_isr(&zynq_ttc_timer);
    }
}

static int zynq_ttc_register(void)
{
    zynq_ttc_timer.info = &zynq_ttc_info;
    zynq_ttc_timer.ops = &zynq_ttc_ops;
    rt_hw_interrupt_install(ZYNQ_TTC0_TIMER0_IRQ, zynq_ttc_isr,
                            RT_NULL, "ttc0_0");
    rt_hw_interrupt_umask(ZYNQ_TTC0_TIMER0_IRQ);
    return rt_clock_timer_register(&zynq_ttc_timer, "timer0", RT_NULL);
}
INIT_BOARD_EXPORT(zynq_ttc_register);

#endif /* BSP_USING_TTC0_TIMER0 */
