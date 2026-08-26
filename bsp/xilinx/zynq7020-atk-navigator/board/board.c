/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

#include <board.h>
#include <interrupt.h>

extern size_t MMUTable[];
extern void rt_hw_ipi_handler_install(int ipi_vector, rt_isr_handler_t handler);

struct mem_desc platform_mem_desc[] =
{
    {0x00000000U, 0x40000000U, 0x00000000U, NORMAL_MEM},
    {0x40000000U, 0xFFF00000U, 0x40000000U, DEVICE_MEM},
    {0xFFF00000U, 0xFFFFFFFFU, 0xFFF00000U, DEVICE_MEM},
};

const rt_uint32_t platform_mem_desc_size =
    sizeof(platform_mem_desc) / sizeof(platform_mem_desc[0]);

static void idle_wfi(void)
{
    __asm__ volatile ("wfi" ::: "memory");
}

void zynq_scu_enable(void)
{
    volatile rt_uint32_t *scu_control = (volatile rt_uint32_t *)ZYNQ_SCU_BASE;
    rt_uint32_t value;

    value = *scu_control;
    *scu_control = value | 1U;

    __asm__ volatile (
        "mrc p15, 0, %0, c1, c0, 1\n"
        "orr %0, %0, #(1 << 6)\n"
        "mcr p15, 0, %0, c1, c0, 1\n"
        "dsb\n"
        "isb\n"
        : "=&r"(value)
        :
        : "memory");
}

void rt_hw_board_init(void)
{
    rt_hw_mmu_map_init(&rt_kernel_space, (void *)0x80000000U,
                       0x10000000U, MMUTable, 0);
    rt_hw_init_mmu_table(platform_mem_desc, platform_mem_desc_size);
    rt_hw_mmu_init();
    rt_hw_mmu_ioremap_init(&rt_kernel_space, (void *)0x80000000U,
                           0x10000000U);

    zynq_scu_enable();
    rt_system_heap_init(HEAP_BEGIN, HEAP_END);
    rt_hw_interrupt_init();
    rt_components_board_init();
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
    rt_thread_idle_sethook(idle_wfi);

#ifdef RT_USING_SMP
    rt_hw_ipi_handler_install(RT_SCHEDULE_IPI, rt_scheduler_ipi_handler);
#endif
}
