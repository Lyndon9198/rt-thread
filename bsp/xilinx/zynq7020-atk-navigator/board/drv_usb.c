/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>

#include <board.h>
#include <usbh_core.h>

#ifdef BSP_USING_USB0_HOST

#define ZYNQ_USB0_BASE                 0xE0002000U
#define ZYNQ_USB0_IRQ                  53
#define ZYNQ_USB0_USBMODE              (ZYNQ_USB0_BASE + 0x1A8U)
#define ZYNQ_USB0_PORTSC1              (ZYNQ_USB0_BASE + 0x184U)
#define ZYNQ_USBMODE_CM_MASK           0x3U
#define ZYNQ_USBMODE_CM_HOST           0x3U
#define ZYNQ_PORTSC_PTS_MASK           (3U << 30)
#define ZYNQ_PORTSC_PTS_ULPI           (2U << 30)
#define ZYNQ_PORTSC_SPEED_MASK         (3U << 26)
#define ZYNQ_PORTSC_SPEED_LOW          (1U << 26)
#define ZYNQ_PORTSC_SPEED_HIGH         (2U << 26)

#define ZYNQ_GPIO_DIRM0                (ZYNQ_GPIO_BASE + 0x204U)
#define ZYNQ_GPIO_OEN0                 (ZYNQ_GPIO_BASE + 0x208U)
#define ZYNQ_GPIO_MASK_DATA_0_LSW      (ZYNQ_GPIO_BASE + 0x000U)
#define ZYNQ_USB_PHY_RESET_PIN         9U

extern void rt_hw_cpu_dcache_clean(void *addr, int size);
extern void rt_hw_cpu_dcache_inv_range(void *addr, int size);
extern void rt_hw_cpu_dcache_clean_and_invalidate(void *addr, int size);

static void zynq_usb_phy_reset(void)
{
    const rt_uint32_t bit = 1U << ZYNQ_USB_PHY_RESET_PIN;
    const rt_uint32_t mask = (~bit & 0xffffU) << 16;

    __REG32(ZYNQ_GPIO_DIRM0) |= bit;
    __REG32(ZYNQ_GPIO_OEN0) |= bit;
    __REG32(ZYNQ_GPIO_MASK_DATA_0_LSW) = mask | bit;
    rt_thread_mdelay(1);
    __REG32(ZYNQ_GPIO_MASK_DATA_0_LSW) = mask;
    rt_thread_mdelay(2);
    __REG32(ZYNQ_GPIO_MASK_DATA_0_LSW) = mask | bit;
    rt_thread_mdelay(10);
}

static void zynq_usb0_isr(int vector, void *param)
{
    RT_UNUSED(vector);
    RT_UNUSED(param);
    USBH_IRQHandler(0);
}

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    RT_UNUSED(bus);

    zynq_usb_phy_reset();
    rt_hw_interrupt_install(ZYNQ_USB0_IRQ, zynq_usb0_isr, RT_NULL, "usb0");
    rt_hw_interrupt_umask(ZYNQ_USB0_IRQ);
}

void usb_hc_low_level2_init(struct usbh_bus *bus)
{
    rt_uint32_t value;

    RT_UNUSED(bus);

    value = __REG32(ZYNQ_USB0_USBMODE);
    value &= ~ZYNQ_USBMODE_CM_MASK;
    value |= ZYNQ_USBMODE_CM_HOST;
    __REG32(ZYNQ_USB0_USBMODE) = value;

    value = __REG32(ZYNQ_USB0_PORTSC1);
    value &= ~ZYNQ_PORTSC_PTS_MASK;
    value |= ZYNQ_PORTSC_PTS_ULPI;
    __REG32(ZYNQ_USB0_PORTSC1) = value;
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    RT_UNUSED(bus);
    rt_hw_interrupt_mask(ZYNQ_USB0_IRQ);
}

uint8_t usbh_get_port_speed(struct usbh_bus *bus, const uint8_t port)
{
    rt_uint32_t speed;

    RT_UNUSED(bus);
    RT_UNUSED(port);

    speed = __REG32(ZYNQ_USB0_PORTSC1) & ZYNQ_PORTSC_SPEED_MASK;
    if (speed == ZYNQ_PORTSC_SPEED_HIGH)
    {
        return USB_SPEED_HIGH;
    }
    if (speed == ZYNQ_PORTSC_SPEED_LOW)
    {
        return USB_SPEED_LOW;
    }
    return USB_SPEED_FULL;
}

void usb_dcache_clean(uintptr_t addr, size_t size)
{
    rt_hw_cpu_dcache_clean((void *)addr, (int)size);
}

void usb_dcache_invalidate(uintptr_t addr, size_t size)
{
    rt_hw_cpu_dcache_inv_range((void *)addr, (int)size);
}

void usb_dcache_flush(uintptr_t addr, size_t size)
{
    rt_hw_cpu_dcache_clean_and_invalidate((void *)addr, (int)size);
}

static int zynq_usb0_host_init(void)
{
    return usbh_initialize(0, ZYNQ_USB0_BASE, RT_NULL);
}
INIT_DEVICE_EXPORT(zynq_usb0_host_init);

#endif /* BSP_USING_USB0_HOST */
