/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtdevice.h>

#include <board.h>

#ifdef BSP_USING_GPIO

#define GPIO_DATA_OFFSET(bank)         (0x40U + (bank) * 4U)
#define GPIO_DATA_RO_OFFSET(bank)      (0x60U + (bank) * 4U)
#define GPIO_BANK_OFFSET(bank)         (0x40U * (bank))
#define GPIO_DIRM_OFFSET(bank)         (0x204U + GPIO_BANK_OFFSET(bank))
#define GPIO_OEN_OFFSET(bank)          (0x208U + GPIO_BANK_OFFSET(bank))
#define GPIO_INT_MASK_OFFSET(bank)     (0x20CU + GPIO_BANK_OFFSET(bank))
#define GPIO_INT_EN_OFFSET(bank)       (0x210U + GPIO_BANK_OFFSET(bank))
#define GPIO_INT_DIS_OFFSET(bank)      (0x214U + GPIO_BANK_OFFSET(bank))
#define GPIO_INT_STAT_OFFSET(bank)     (0x218U + GPIO_BANK_OFFSET(bank))
#define GPIO_INT_TYPE_OFFSET(bank)     (0x21CU + GPIO_BANK_OFFSET(bank))
#define GPIO_INT_POL_OFFSET(bank)      (0x220U + GPIO_BANK_OFFSET(bank))
#define GPIO_INT_ANY_OFFSET(bank)      (0x224U + GPIO_BANK_OFFSET(bank))

#define ZYNQ_GPIO_BANK_COUNT           4U

struct zynq_pin_irq
{
    void (*hdr)(void *args);
    void *args;
    rt_uint8_t mode;
};

static struct zynq_pin_irq pin_irq_table[ZYNQ_GPIO_PIN_COUNT];
static struct rt_spinlock gpio_lock = RT_SPINLOCK_INIT;

static rt_uint32_t zynq_gpio_bank(rt_base_t pin)
{
    if (pin < 32)
    {
        return 0;
    }
    if (pin < 54)
    {
        return 1;
    }
    if (pin < 86)
    {
        return 2;
    }
    return 3;
}

static rt_uint32_t zynq_gpio_bank_pin(rt_base_t pin, rt_uint32_t bank)
{
    static const rt_uint8_t bank_start[] = {0, 32, 54, 86};

    return (rt_uint32_t)pin - bank_start[bank];
}

static rt_bool_t zynq_gpio_pin_valid(rt_base_t pin)
{
    return pin >= 0 && pin < ZYNQ_GPIO_PIN_COUNT;
}

static void zynq_pin_mode(struct rt_device *device, rt_base_t pin,
                          rt_uint8_t mode)
{
    rt_ubase_t level;
    rt_uint32_t bank;
    rt_uint32_t bit;
    rt_uint32_t mask;

    RT_UNUSED(device);
    if (!zynq_gpio_pin_valid(pin))
    {
        return;
    }

    bank = zynq_gpio_bank(pin);
    bit = zynq_gpio_bank_pin(pin, bank);
    mask = 1U << bit;
    level = rt_spin_lock_irqsave(&gpio_lock);

    if (mode == PIN_MODE_OUTPUT || mode == PIN_MODE_OUTPUT_OD)
    {
        __REG32(ZYNQ_GPIO_BASE + GPIO_DIRM_OFFSET(bank)) |= mask;
        __REG32(ZYNQ_GPIO_BASE + GPIO_OEN_OFFSET(bank)) |= mask;
    }
    else
    {
        __REG32(ZYNQ_GPIO_BASE + GPIO_OEN_OFFSET(bank)) &= ~mask;
        __REG32(ZYNQ_GPIO_BASE + GPIO_DIRM_OFFSET(bank)) &= ~mask;
    }

    rt_spin_unlock_irqrestore(&gpio_lock, level);
}

static void zynq_pin_write(struct rt_device *device, rt_base_t pin,
                           rt_uint8_t value)
{
    rt_uint32_t bank;
    rt_uint32_t bit;
    rt_uint32_t half;
    rt_uint32_t mask;

    RT_UNUSED(device);
    if (!zynq_gpio_pin_valid(pin))
    {
        return;
    }

    bank = zynq_gpio_bank(pin);
    bit = zynq_gpio_bank_pin(pin, bank);
    half = bit >> 4;
    bit &= 0xFU;
    mask = (~(1U << bit) & 0xFFFFU) << 16;
    __REG32(ZYNQ_GPIO_BASE + bank * 8U + half * 4U) =
        mask | ((value == PIN_HIGH ? 1U : 0U) << bit);
}

static rt_ssize_t zynq_pin_read(struct rt_device *device, rt_base_t pin)
{
    rt_uint32_t bank;
    rt_uint32_t bit;

    RT_UNUSED(device);
    if (!zynq_gpio_pin_valid(pin))
    {
        return -RT_EINVAL;
    }

    bank = zynq_gpio_bank(pin);
    bit = zynq_gpio_bank_pin(pin, bank);
    return (__REG32(ZYNQ_GPIO_BASE + GPIO_DATA_RO_OFFSET(bank)) >> bit) & 1U;
}

static rt_base_t zynq_pin_get(const char *name)
{
    const char *number;
    rt_base_t pin = 0;
    rt_base_t limit;
    rt_base_t offset = 0;

    if (!name)
    {
        return -RT_EINVAL;
    }
    if (name[0] == ' ' && name[1] == '\0')
    {
        rt_kprintf(" MIO0-MIO53, EMIO0-EMIO63\n");
        return -RT_EINVAL;
    }
    if (rt_strncmp(name, "MIO", 3) == 0)
    {
        number = name + 3;
        limit = 54;
    }
    else if (rt_strncmp(name, "EMIO", 4) == 0)
    {
        number = name + 4;
        limit = 64;
        offset = 54;
    }
    else
    {
        return -RT_EINVAL;
    }

    if (*number < '0' || *number > '9')
    {
        return -RT_EINVAL;
    }
    do
    {
        pin = pin * 10 + (*number - '0');
        number++;
    } while (*number >= '0' && *number <= '9');

    if (*number != '\0')
    {
        return -RT_EINVAL;
    }
    if (pin >= limit)
    {
        return -RT_EINVAL;
    }
    return pin + offset;
}

static rt_err_t zynq_pin_attach_irq(struct rt_device *device, rt_base_t pin,
                                    rt_uint8_t mode,
                                    void (*hdr)(void *args), void *args)
{
    rt_ubase_t level;
    struct zynq_pin_irq *irq;

    RT_UNUSED(device);
    if (!zynq_gpio_pin_valid(pin) || mode > PIN_IRQ_MODE_LOW_LEVEL || !hdr)
    {
        return -RT_EINVAL;
    }

    level = rt_spin_lock_irqsave(&gpio_lock);
    irq = &pin_irq_table[pin];
    if (irq->hdr && (irq->hdr != hdr || irq->args != args || irq->mode != mode))
    {
        rt_spin_unlock_irqrestore(&gpio_lock, level);
        return -RT_EBUSY;
    }
    irq->hdr = hdr;
    irq->args = args;
    irq->mode = mode;
    rt_spin_unlock_irqrestore(&gpio_lock, level);
    return RT_EOK;
}

static rt_err_t zynq_pin_detach_irq(struct rt_device *device, rt_base_t pin)
{
    rt_ubase_t level;
    rt_uint32_t bank;
    rt_uint32_t bit;

    RT_UNUSED(device);
    if (!zynq_gpio_pin_valid(pin))
    {
        return -RT_EINVAL;
    }

    bank = zynq_gpio_bank(pin);
    bit = zynq_gpio_bank_pin(pin, bank);
    level = rt_spin_lock_irqsave(&gpio_lock);
    __REG32(ZYNQ_GPIO_BASE + GPIO_INT_DIS_OFFSET(bank)) = 1U << bit;
    pin_irq_table[pin].hdr = RT_NULL;
    pin_irq_table[pin].args = RT_NULL;
    pin_irq_table[pin].mode = 0;
    rt_spin_unlock_irqrestore(&gpio_lock, level);
    return RT_EOK;
}

static rt_err_t zynq_pin_irq_enable(struct rt_device *device, rt_base_t pin,
                                    rt_uint8_t enabled)
{
    rt_ubase_t level;
    rt_uint32_t bank;
    rt_uint32_t bit;
    rt_uint32_t mask;
    rt_uint32_t type;
    rt_uint32_t polarity;
    rt_uint32_t any;
    rt_uint8_t mode;

    RT_UNUSED(device);
    if (!zynq_gpio_pin_valid(pin))
    {
        return -RT_EINVAL;
    }

    bank = zynq_gpio_bank(pin);
    bit = zynq_gpio_bank_pin(pin, bank);
    mask = 1U << bit;
    level = rt_spin_lock_irqsave(&gpio_lock);
    if (enabled == PIN_IRQ_DISABLE)
    {
        __REG32(ZYNQ_GPIO_BASE + GPIO_INT_DIS_OFFSET(bank)) = mask;
        rt_spin_unlock_irqrestore(&gpio_lock, level);
        return RT_EOK;
    }
    if (enabled != PIN_IRQ_ENABLE || !pin_irq_table[pin].hdr)
    {
        rt_spin_unlock_irqrestore(&gpio_lock, level);
        return -RT_EINVAL;
    }

    mode = pin_irq_table[pin].mode;
    type = __REG32(ZYNQ_GPIO_BASE + GPIO_INT_TYPE_OFFSET(bank));
    polarity = __REG32(ZYNQ_GPIO_BASE + GPIO_INT_POL_OFFSET(bank));
    any = __REG32(ZYNQ_GPIO_BASE + GPIO_INT_ANY_OFFSET(bank));
    type &= ~mask;
    polarity &= ~mask;
    any &= ~mask;

    if (mode == PIN_IRQ_MODE_RISING || mode == PIN_IRQ_MODE_FALLING ||
        mode == PIN_IRQ_MODE_RISING_FALLING)
    {
        type |= mask;
    }
    if (mode == PIN_IRQ_MODE_RISING || mode == PIN_IRQ_MODE_HIGH_LEVEL)
    {
        polarity |= mask;
    }
    if (mode == PIN_IRQ_MODE_RISING_FALLING)
    {
        any |= mask;
    }

    __REG32(ZYNQ_GPIO_BASE + GPIO_INT_TYPE_OFFSET(bank)) = type;
    __REG32(ZYNQ_GPIO_BASE + GPIO_INT_POL_OFFSET(bank)) = polarity;
    __REG32(ZYNQ_GPIO_BASE + GPIO_INT_ANY_OFFSET(bank)) = any;
    __REG32(ZYNQ_GPIO_BASE + GPIO_INT_STAT_OFFSET(bank)) = mask;
    __REG32(ZYNQ_GPIO_BASE + GPIO_INT_EN_OFFSET(bank)) = mask;
    rt_spin_unlock_irqrestore(&gpio_lock, level);
    return RT_EOK;
}

static void zynq_gpio_isr(int vector, void *parameter)
{
    rt_uint32_t bank;

    RT_UNUSED(vector);
    RT_UNUSED(parameter);
    for (bank = 0; bank < ZYNQ_GPIO_BANK_COUNT; bank++)
    {
        rt_uint32_t status = __REG32(ZYNQ_GPIO_BASE + GPIO_INT_STAT_OFFSET(bank));
        rt_uint32_t mask = __REG32(ZYNQ_GPIO_BASE + GPIO_INT_MASK_OFFSET(bank));
        rt_uint32_t bit;

        status &= ~mask;
        __REG32(ZYNQ_GPIO_BASE + GPIO_INT_STAT_OFFSET(bank)) = status;
        for (bit = 0; bit < 32U; bit++)
        {
            if (status & (1U << bit))
            {
                rt_uint32_t pin = bit + (bank == 0U ? 0U :
                                         bank == 1U ? 32U :
                                         bank == 2U ? 54U : 86U);
                if (pin < ZYNQ_GPIO_PIN_COUNT && pin_irq_table[pin].hdr)
                {
                    pin_irq_table[pin].hdr(pin_irq_table[pin].args);
                }
            }
        }
    }
}

static const struct rt_pin_ops zynq_pin_ops =
{
    zynq_pin_mode,
    zynq_pin_write,
    zynq_pin_read,
    zynq_pin_attach_irq,
    zynq_pin_detach_irq,
    zynq_pin_irq_enable,
    zynq_pin_get,
    RT_NULL,
};

static int zynq_gpio_init(void)
{
    rt_uint32_t bank;

    for (bank = 0; bank < ZYNQ_GPIO_BANK_COUNT; bank++)
    {
        __REG32(ZYNQ_GPIO_BASE + GPIO_INT_DIS_OFFSET(bank)) = 0xFFFFFFFFU;
        __REG32(ZYNQ_GPIO_BASE + GPIO_INT_STAT_OFFSET(bank)) = 0xFFFFFFFFU;
    }
    rt_hw_interrupt_install(ZYNQ_GPIO_IRQ, zynq_gpio_isr, RT_NULL, "gpio");
    rt_hw_interrupt_umask(ZYNQ_GPIO_IRQ);
    return rt_device_pin_register("pin", &zynq_pin_ops, RT_NULL);
}
INIT_BOARD_EXPORT(zynq_gpio_init);

#endif /* BSP_USING_GPIO */
