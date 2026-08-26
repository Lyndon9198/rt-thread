/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

    .syntax unified
    .arm
    .section .text.zynq_secondary_entry, "ax"
    .align 5
    .global zynq_secondary_entry
    .type zynq_secondary_entry, %function

zynq_secondary_entry:
    mrc     p15, 0, r0, c1, c0, 1
    orr     r0, r0, #(1 << 6)
    mcr     p15, 0, r0, c1, c0, 1
    dsb
    isb
    b       rt_secondary_cpu_entry

    .size zynq_secondary_entry, . - zynq_secondary_entry
