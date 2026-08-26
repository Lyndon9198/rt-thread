/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtdevice.h>
#include <rtthread.h>

#include <board.h>

#define UART_CR(base)           (*(volatile rt_uint32_t *)((base) + 0x00U))
#define UART_MR(base)           (*(volatile rt_uint32_t *)((base) + 0x04U))
#define UART_IER(base)          (*(volatile rt_uint32_t *)((base) + 0x08U))
#define UART_IDR(base)          (*(volatile rt_uint32_t *)((base) + 0x0CU))
#define UART_ISR(base)          (*(volatile rt_uint32_t *)((base) + 0x14U))
#define UART_BAUDGEN(base)      (*(volatile rt_uint32_t *)((base) + 0x18U))
#define UART_RXTOUT(base)       (*(volatile rt_uint32_t *)((base) + 0x1CU))
#define UART_RXWM(base)         (*(volatile rt_uint32_t *)((base) + 0x20U))
#define UART_SR(base)           (*(volatile rt_uint32_t *)((base) + 0x2CU))
#define UART_FIFO(base)         (*(volatile rt_uint32_t *)((base) + 0x30U))
#define UART_BAUDDIV(base)      (*(volatile rt_uint32_t *)((base) + 0x34U))

#define UART_CR_RXRST           (1U << 0)
#define UART_CR_TXRST           (1U << 1)
#define UART_CR_RXEN            (1U << 2)
#define UART_CR_RXDIS           (1U << 3)
#define UART_CR_TXEN            (1U << 4)
#define UART_CR_TXDIS           (1U << 5)
#define UART_MR_CLKSEL          (1U << 0)
#define UART_MR_CHARLEN_MASK    (3U << 1)
#define UART_MR_CHARLEN_7       (2U << 1)
#define UART_MR_CHARLEN_6       (3U << 1)
#define UART_MR_PARITY_MASK     (7U << 3)
#define UART_MR_PARITY_ODD      (1U << 3)
#define UART_MR_PARITY_NONE     (4U << 3)
#define UART_MR_STOP_MASK       (3U << 6)
#define UART_MR_STOP_2          (2U << 6)
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

static rt_err_t zynq_uart_set_baudrate(struct zynq_uart *uart,
                                       rt_uint32_t baudrate)
{
    rt_uint32_t best_error = 0xFFFFFFFFU;
    rt_uint32_t best_baudgen = 0;
    rt_uint32_t best_divisor = 0;
    rt_uint32_t divisor;

    if (baudrate == 0U || baudrate > ZYNQ_UART_INPUT_CLOCK_HZ / 2U)
    {
        return -RT_EINVAL;
    }

    for (divisor = 4U; divisor <= 254U; divisor++)
    {
        rt_uint32_t baudgen = ZYNQ_UART_INPUT_CLOCK_HZ /
                              (baudrate * (divisor + 1U));
        rt_uint32_t actual;
        rt_uint32_t error;

        if (baudgen < 2U || baudgen > 65535U)
        {
            continue;
        }

        actual = ZYNQ_UART_INPUT_CLOCK_HZ / (baudgen * (divisor + 1U));
        error = actual > baudrate ? actual - baudrate : baudrate - actual;
        if (error < best_error)
        {
            best_error = error;
            best_baudgen = baudgen;
            best_divisor = divisor;
        }
    }

    if (best_baudgen == 0U || ((rt_uint64_t)best_error * 100U / baudrate) > 3U)
    {
        return -RT_EINVAL;
    }

    UART_BAUDGEN(uart->base) = best_baudgen;
    UART_BAUDDIV(uart->base) = best_divisor;
    return RT_EOK;
}

static void zynq_uart_isr(int vector, void *parameter)
{
    struct zynq_uart *uart = parameter;
    rt_uint32_t status = UART_ISR(uart->base);

    RT_UNUSED(vector);
    UART_ISR(uart->base) = status;
    if (status & UART_IXR_RX_MASK)
    {
        rt_hw_serial_isr(&uart->serial, RT_SERIAL_EVENT_RX_IND);
    }
}

static rt_err_t zynq_uart_configure(struct rt_serial_device *serial,
                                    struct serial_configure *configure)
{
    struct zynq_uart *uart = serial->parent.user_data;
    rt_uint32_t mode = UART_MR(uart->base);

    mode &= ~(UART_MR_CLKSEL | UART_MR_CHARLEN_MASK |
              UART_MR_PARITY_MASK | UART_MR_STOP_MASK);
    switch (configure->data_bits)
    {
    case DATA_BITS_8:
        break;
    case DATA_BITS_7:
        mode |= UART_MR_CHARLEN_7;
        break;
    case DATA_BITS_6:
        mode |= UART_MR_CHARLEN_6;
        break;
    default:
        return -RT_EINVAL;
    }

    if (configure->parity == PARITY_ODD)
    {
        mode |= UART_MR_PARITY_ODD;
    }
    else if (configure->parity == PARITY_NONE)
    {
        mode |= UART_MR_PARITY_NONE;
    }
    else if (configure->parity != PARITY_EVEN)
    {
        return -RT_EINVAL;
    }

    if (configure->stop_bits == STOP_BITS_2)
    {
        mode |= UART_MR_STOP_2;
    }
    else if (configure->stop_bits != STOP_BITS_1)
    {
        return -RT_EINVAL;
    }

    UART_CR(uart->base) = UART_CR_RXDIS | UART_CR_TXDIS;
    UART_CR(uart->base) = UART_CR_RXRST | UART_CR_TXRST;
    while (UART_CR(uart->base) & (UART_CR_RXRST | UART_CR_TXRST))
    {
    }
    UART_MR(uart->base) = mode;
    if (zynq_uart_set_baudrate(uart, configure->baud_rate) != RT_EOK)
    {
        return -RT_EINVAL;
    }
    UART_RXWM(uart->base) = 1U;
    UART_RXTOUT(uart->base) = 10U;
    UART_CR(uart->base) = UART_CR_RXEN | UART_CR_TXEN;
    return RT_EOK;
}

static rt_err_t zynq_uart_control(struct rt_serial_device *serial,
                                  int command, void *argument)
{
    struct zynq_uart *uart = serial->parent.user_data;

    RT_UNUSED(argument);
    switch (command)
    {
    case RT_DEVICE_CTRL_CLR_INT:
        UART_IDR(uart->base) = UART_IXR_RX_MASK;
        rt_hw_interrupt_mask(uart->irq);
        break;
    case RT_DEVICE_CTRL_SET_INT:
        UART_ISR(uart->base) = UART_IXR_RX_MASK;
        UART_IER(uart->base) = UART_IXR_RX_MASK;
        rt_hw_interrupt_umask(uart->irq);
        break;
    default:
        return -RT_EINVAL;
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
