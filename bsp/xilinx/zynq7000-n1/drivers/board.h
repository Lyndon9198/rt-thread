/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <rtconfig.h>
#include <zynq7000.h>
#include <mmu.h>

extern int __bss_end;

#define HEAP_BEGIN      ((void *)&__bss_end)
#define HEAP_END        ((void *)0x08000000U)

void rt_hw_board_init(void);
void zynq_private_timer_init(void);
void zynq_scu_enable(void);

#endif /* __BOARD_H__ */
