/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <rtconfig.h>
#include <mmu.h>

#include "zynq7000.h"

extern int __bss_end;

#define HEAP_BEGIN      ((void *)&__bss_end)
#define HEAP_END        ((void *)ZYNQ_DDR_END)

void rt_hw_board_init(void);
void zynq_private_timer_init(void);
void zynq_scu_enable(void);

#endif /* __BOARD_H__ */
