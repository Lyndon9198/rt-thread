/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtdevice.h>

#include "zynq7000.h"

#ifdef BSP_USING_GPIO

#define GPIO_DATA_OFFSET(bank)         (0x40U + ((bank) * 4U))
#define GPIO_DATA_RO_OFFSET(bank)      (0x60U + ((bank) * 4U))
#define GPIO_DIRM_OFFSET(bank)         (0x204U + ((bank) * 0x40U))
#define GPIO_OEN_OFFSET(bank)          (0x208U + ((bank) * 0x40U))

#define AXI_GPIO_DATA                  0x00U
#define AXI_GPIO_TRI                   0x04U

static rt_uint32_t gpio_bank(rt_base_t pin)
{
    if (pin < 32)
        return 0;
    if (pin < 54)
        return 1;
    if (pin < 86)
        return 2;
    return 3;
}

static rt_uint32_t gpio_bank_pin(rt_base_t pin, rt_uint32_t bank)
{
    static const rt_uint8_t bank_start[] = {0, 32, 54, 86};

    return (rt_uint32_t)pin - bank_start[bank];
}

static void zynq_pin_mode(struct rt_device *device, rt_base_t pin,
                          rt_uint8_t mode)
{
    rt_uint32_t bank;
    rt_uint32_t bit;
    rt_uint32_t value;

    RT_UNUSED(device);

    if (pin >= N1_GPIO_PIN_COUNT)
        return;

    if (pin >= N1_AXI_GPIO_PIN_BASE)
    {
        bit = (rt_uint32_t)pin - N1_AXI_GPIO_PIN_BASE;
        value = __REG32(N1_AXI_GPIO_BASE + AXI_GPIO_TRI);
        if (mode == PIN_MODE_OUTPUT)
            value &= ~(1U << bit);
        else
            value |= 1U << bit;
        __REG32(N1_AXI_GPIO_BASE + AXI_GPIO_TRI) = value;
        return;
    }

    bank = gpio_bank(pin);
    bit = gpio_bank_pin(pin, bank);
    value = __REG32(ZYNQ_GPIO_BASE + GPIO_DIRM_OFFSET(bank));
    if (mode == PIN_MODE_OUTPUT || mode == PIN_MODE_OUTPUT_OD)
    {
        __REG32(ZYNQ_GPIO_BASE + GPIO_DIRM_OFFSET(bank)) = value | (1U << bit);
        value = __REG32(ZYNQ_GPIO_BASE + GPIO_OEN_OFFSET(bank));
        __REG32(ZYNQ_GPIO_BASE + GPIO_OEN_OFFSET(bank)) = value | (1U << bit);
    }
    else
    {
        __REG32(ZYNQ_GPIO_BASE + GPIO_OEN_OFFSET(bank)) &= ~(1U << bit);
        __REG32(ZYNQ_GPIO_BASE + GPIO_DIRM_OFFSET(bank)) &= ~(1U << bit);
    }
}

static void zynq_pin_write(struct rt_device *device, rt_base_t pin,
                           rt_uint8_t value)
{
    rt_uint32_t bank;
    rt_uint32_t bit;
    rt_uint32_t half;
    rt_uint32_t mask;

    RT_UNUSED(device);

    if (pin >= N1_GPIO_PIN_COUNT)
        return;

    if (pin >= N1_AXI_GPIO_PIN_BASE)
    {
        bit = (rt_uint32_t)pin - N1_AXI_GPIO_PIN_BASE;
        mask = __REG32(N1_AXI_GPIO_BASE + AXI_GPIO_DATA);
        if (value)
            mask |= 1U << bit;
        else
            mask &= ~(1U << bit);
        __REG32(N1_AXI_GPIO_BASE + AXI_GPIO_DATA) = mask;
        return;
    }

    bank = gpio_bank(pin);
    bit = gpio_bank_pin(pin, bank);
    half = bit >> 4;
    bit &= 0xFU;
    mask = (~(1U << bit) & 0xFFFFU) << 16;
    __REG32(ZYNQ_GPIO_BASE + bank * 8U + half * 4U) =
        mask | ((value ? 1U : 0U) << bit);
}

static rt_ssize_t zynq_pin_read(struct rt_device *device, rt_base_t pin)
{
    rt_uint32_t bank;
    rt_uint32_t bit;

    RT_UNUSED(device);

    if (pin >= N1_GPIO_PIN_COUNT)
        return PIN_LOW;

    if (pin >= N1_AXI_GPIO_PIN_BASE)
    {
        bit = (rt_uint32_t)pin - N1_AXI_GPIO_PIN_BASE;
        return (__REG32(N1_AXI_GPIO_BASE + AXI_GPIO_DATA) >> bit) & 1U;
    }

    bank = gpio_bank(pin);
    bit = gpio_bank_pin(pin, bank);
    return (__REG32(ZYNQ_GPIO_BASE + GPIO_DATA_RO_OFFSET(bank)) >> bit) & 1U;
}

static const struct rt_pin_ops zynq_pin_ops =
{
    zynq_pin_mode,
    zynq_pin_write,
    zynq_pin_read,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
};

static int zynq_gpio_init(void)
{
    return rt_device_pin_register("pin", &zynq_pin_ops, RT_NULL);
}
INIT_BOARD_EXPORT(zynq_gpio_init);

#endif /* BSP_USING_GPIO */
