/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

#include <board.h>
#include <gic.h>
#include <interrupt.h>

#ifdef RT_USING_SMP

extern size_t MMUTable[];
extern void zynq_secondary_entry(void);

static void zynq_send_event(void)
{
    __asm__ volatile (
        "dsb\n"
        "sev\n"
        "isb\n"
        ::: "memory");
}

void rt_hw_secondary_cpu_up(void)
{
    volatile rt_uint32_t *cpu1_start =
        (volatile rt_uint32_t *)ZYNQ_CPU1_START_ADDRESS;

    *cpu1_start = (rt_uint32_t)zynq_secondary_entry;
    rt_hw_dsb();
    zynq_send_event();
}

void rt_hw_secondary_cpu_bsp_start(void)
{
    rt_hw_mmu_switch(MMUTable);
    zynq_scu_enable();
    rt_hw_vector_init();
    arm_gic_cpu_init(0, ZYNQ_GIC_CPU_BASE);
    zynq_private_timer_init();

    rt_hw_spin_lock(&_cpus_lock);
    rt_system_scheduler_start();
}

void rt_hw_secondary_cpu_idle_exec(void)
{
    __asm__ volatile ("wfe" ::: "memory");
}

#endif /* RT_USING_SMP */
