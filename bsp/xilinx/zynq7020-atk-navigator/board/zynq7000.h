/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ZYNQ7000_H__
#define __ZYNQ7000_H__

#include <rtdef.h>

#define __REG32(address)               (*(volatile rt_uint32_t *)(address))

#define ZYNQ_DDR_END                   0x40000000U

#define ZYNQ_UART0_BASE                0xE0000000U
#define ZYNQ_UART0_IRQ                 59
#define ZYNQ_UART_INPUT_CLOCK_HZ       100000000U

#define ZYNQ_GPIO_BASE                 0xE000A000U
#define ZYNQ_GPIO_IRQ                  52
#define ZYNQ_GPIO_PIN_COUNT            118

#define ZYNQ_WDT_BASE                  0xF8005000U
#define ZYNQ_WDT_CLOCK_HZ              125000000U

#define ZYNQ_TTC0_TIMER0_BASE           0xF8001000U
#define ZYNQ_TTC0_TIMER0_IRQ            42
#define ZYNQ_TTC_INPUT_CLOCK_HZ         125000000U

#define ZYNQ_CAN0_BASE                  0xE0008000U
#define ZYNQ_CAN0_IRQ                   60
#define ZYNQ_CAN0_CLOCK_HZ              100000000U

#define ZYNQ_GEM_DMA_START              0x3FE00000U
#define ZYNQ_GEM_DMA_SIZE               0x00200000U

#define ZYNQ_SCU_BASE                  0xF8F00000U
#define ZYNQ_GIC_CPU_BASE              0xF8F00100U
#define ZYNQ_PRIVATE_TIMER_BASE        0xF8F00600U
#define ZYNQ_GIC_DIST_BASE             0xF8F01000U
#define ZYNQ_CPU1_START_ADDRESS        0xFFFFFFF0U

#define ZYNQ_PRIVATE_TIMER_IRQ         29
#define ZYNQ_CPU_CLOCK_HZ              767000000U
#define ZYNQ_PRIVATE_TIMER_CLOCK_HZ    (ZYNQ_CPU_CLOCK_HZ / 2U)

#ifndef MAX_HANDLERS
#define MAX_HANDLERS                   96
#endif
#define ARM_GIC_MAX_NR                 1
#define ARM_GIC_NR_IRQS                MAX_HANDLERS
#define GIC_IRQ_START                  0
#define GIC_ACK_INTID_MASK             0x000003FFU

rt_inline rt_uint32_t platform_get_gic_dist_base(void)
{
    return ZYNQ_GIC_DIST_BASE;
}

rt_inline rt_uint32_t platform_get_gic_cpu_base(void)
{
    return ZYNQ_GIC_CPU_BASE;
}

#endif /* __ZYNQ7000_H__ */
