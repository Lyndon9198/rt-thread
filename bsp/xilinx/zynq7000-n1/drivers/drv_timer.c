/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

#include <board.h>

#define TIMER_LOAD              (*(volatile rt_uint32_t *)(ZYNQ_PRIVATE_TIMER_BASE + 0x00U))
#define TIMER_COUNTER           (*(volatile rt_uint32_t *)(ZYNQ_PRIVATE_TIMER_BASE + 0x04U))
#define TIMER_CONTROL           (*(volatile rt_uint32_t *)(ZYNQ_PRIVATE_TIMER_BASE + 0x08U))
#define TIMER_ISR               (*(volatile rt_uint32_t *)(ZYNQ_PRIVATE_TIMER_BASE + 0x0CU))

#define TIMER_CONTROL_ENABLE    (1U << 0)
#define TIMER_CONTROL_AUTO      (1U << 1)
#define TIMER_CONTROL_IRQ       (1U << 2)

static void zynq_timer_isr(int vector, void *parameter)
{
    TIMER_ISR = 1U;
    rt_tick_increase();
}

void zynq_private_timer_init(void)
{
    TIMER_CONTROL = 0U;
    TIMER_ISR = 1U;
    TIMER_LOAD = ZYNQ_PRIVATE_TIMER_CLOCK_HZ / RT_TICK_PER_SECOND;
    TIMER_COUNTER = TIMER_LOAD;
    TIMER_CONTROL = TIMER_CONTROL_ENABLE | TIMER_CONTROL_AUTO | TIMER_CONTROL_IRQ;
    rt_hw_interrupt_umask(ZYNQ_PRIVATE_TIMER_IRQ);
}

void rt_hw_us_delay(rt_uint32_t us)
{
    const rt_uint32_t reload = ZYNQ_PRIVATE_TIMER_CLOCK_HZ / RT_TICK_PER_SECOND;
    rt_uint64_t remaining = (rt_uint64_t)us *
                            (ZYNQ_PRIVATE_TIMER_CLOCK_HZ / 1000000U);
    rt_uint32_t previous = TIMER_COUNTER;

    while (remaining)
    {
        rt_uint32_t current = TIMER_COUNTER;
        rt_uint32_t elapsed = previous >= current ?
                              previous - current : previous + reload - current;

        if (elapsed)
        {
            remaining = elapsed >= remaining ? 0 : remaining - elapsed;
            previous = current;
        }
    }
}

static int rt_hw_timer_init(void)
{
    rt_hw_interrupt_install(ZYNQ_PRIVATE_TIMER_IRQ, zynq_timer_isr,
                            RT_NULL, "tick");
    zynq_private_timer_init();
    return RT_EOK;
}
INIT_BOARD_EXPORT(rt_hw_timer_init);
