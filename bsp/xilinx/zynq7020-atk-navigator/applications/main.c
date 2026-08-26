/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

int main(void)
{
    rt_kprintf("RT-Thread on ALIENTEK Navigator ZYNQ7020, CPU%d.\n",
               rt_hw_cpu_id());
    return RT_EOK;
}
