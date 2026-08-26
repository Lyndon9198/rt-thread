/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

    .syntax unified
    .arm
    .section .boot, "ax"
    .align 5
    .global zynq_uboot_entry
    .type zynq_uboot_entry, %function

zynq_uboot_entry:
    cpsid   if
    cps     #0x13
    b       _reset
    .space  20

    .size zynq_uboot_entry, . - zynq_uboot_entry
