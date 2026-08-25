/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtdevice.h>
#include <rtthread.h>

#include <board.h>

#define UART_IER(base)          (*(volatile rt_uint32_t *)((base) + 0x08U))
#define UART_IDR(base)          (*(volatile rt_uint32_t *)((base) + 0x0CU))
#define UART_ISR(base)          (*(volatile rt_uint32_t *)((base) + 0x14U))
#define UART_RXWM(base)         (*(volatile rt_uint32_t *)((base) + 0x20U))
#define UART_SR(base)           (*(volatile rt_uint32_t *)((base) + 0x2CU))
#define UART_FIFO(base)         (*(volatile rt_uint32_t *)((base) + 0x30U))

#define UART_IXR_RXTRIG         (1U << 0)
#define UART_IXR_RXOVR          (1U << 5)
#define UART_IXR_TOUT           (1U << 8)
#define UART_IXR_RX_MASK        (UART_IXR_RXTRIG | UART_IXR_RXOVR | UART_IXR_TOUT)
#define UART_SR_RXEMPTY         (1U << 1)
#define UART_SR_TXFULL          (1U << 4)

struct zynq_uart
{
    rt_ubase_t base;
    int irq;
    struct rt_serial_device serial;
};

static struct zynq_uart uart0 =
{
    .base = ZYNQ_UART0_BASE,
    .irq = ZYNQ_UART0_IRQ,
};

static void zynq_uart_isr(int vector, void *parameter)
{
    struct zynq_uart *uart = parameter;
    rt_uint32_t status = UART_ISR(uart->base);

    UART_ISR(uart->base) = status;
    if (status & UART_IXR_RX_MASK)
    {
        rt_hw_serial_isr(&uart->serial, RT_SERIAL_EVENT_RX_IND);
    }
}

static rt_err_t zynq_uart_configure(struct rt_serial_device *serial,
                                    struct serial_configure *configure)
{
    return RT_EOK;
}

static rt_err_t zynq_uart_control(struct rt_serial_device *serial,
                                  int command, void *argument)
{
    struct zynq_uart *uart = serial->parent.user_data;

    switch (command)
    {
    case RT_DEVICE_CTRL_CLR_INT:
        UART_IDR(uart->base) = UART_IXR_RX_MASK;
        rt_hw_interrupt_mask(uart->irq);
        break;
    case RT_DEVICE_CTRL_SET_INT:
        UART_ISR(uart->base) = UART_IXR_RX_MASK;
        UART_RXWM(uart->base) = 1U;
        UART_IER(uart->base) = UART_IXR_RX_MASK;
        rt_hw_interrupt_umask(uart->irq);
        break;
    default:
        break;
    }

    return RT_EOK;
}

static int zynq_uart_putc(struct rt_serial_device *serial, char character)
{
    struct zynq_uart *uart = serial->parent.user_data;

    while (UART_SR(uart->base) & UART_SR_TXFULL)
    {
    }
    UART_FIFO(uart->base) = (rt_uint32_t)character;
    return 1;
}

static int zynq_uart_getc(struct rt_serial_device *serial)
{
    struct zynq_uart *uart = serial->parent.user_data;

    if (UART_SR(uart->base) & UART_SR_RXEMPTY)
    {
        return -1;
    }
    return UART_FIFO(uart->base) & 0xFFU;
}

static const struct rt_uart_ops zynq_uart_ops =
{
    zynq_uart_configure,
    zynq_uart_control,
    zynq_uart_putc,
    zynq_uart_getc,
};

static int rt_hw_uart_init(void)
{
    struct serial_configure configure = RT_SERIAL_CONFIG_DEFAULT;

    UART_IDR(uart0.base) = 0xFFFFFFFFU;
    UART_ISR(uart0.base) = 0xFFFFFFFFU;
    uart0.serial.ops = &zynq_uart_ops;
    uart0.serial.config = configure;

    rt_hw_interrupt_install(uart0.irq, zynq_uart_isr, &uart0, "uart0");
    return rt_hw_serial_register(&uart0.serial, "uart0",
                                 RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX,
                                 &uart0);
}
INIT_BOARD_EXPORT(rt_hw_uart_init);
