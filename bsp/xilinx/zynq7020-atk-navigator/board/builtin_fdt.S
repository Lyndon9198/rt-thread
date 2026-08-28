/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtconfig.h>

    .section .rodata
    .align 4
    .global rt_hw_builtin_fdt
rt_hw_builtin_fdt:
#ifdef RT_BUILTIN_FDT_PATH
    .incbin RT_BUILTIN_FDT_PATH
#endif
