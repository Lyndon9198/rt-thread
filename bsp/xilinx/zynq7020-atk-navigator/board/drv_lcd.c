/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>

#define DBG_TAG                         "lcd.vdma"
#define DBG_LVL                         DBG_INFO
#include <rtdbg.h>

#define ZYNQ_LCD_WIDTH                  1024U
#define ZYNQ_LCD_HEIGHT                 600U
#define ZYNQ_LCD_BYTES_PER_PIXEL        3U
#define ZYNQ_LCD_PITCH                  (ZYNQ_LCD_WIDTH * ZYNQ_LCD_BYTES_PER_PIXEL)
#define ZYNQ_LCD_FB_SIZE                (ZYNQ_LCD_PITCH * ZYNQ_LCD_HEIGHT)

#define ZYNQ_VDMA_BASE                  0x43000000U
#define ZYNQ_VTC_BASE                   0x43c00000U

#define VDMA_MM2S_DMACR                 0x00U
#define VDMA_MM2S_DMASR                 0x04U
#define VDMA_PARK_PTR                   0x28U
#define VDMA_VERSION                    0x2cU
#define VDMA_MM2S_VSIZE                 0x50U
#define VDMA_MM2S_HSIZE                 0x54U
#define VDMA_MM2S_STRIDE                0x58U
#define VDMA_MM2S_START_ADDR(n)         (0x5cU + ((n) * 4U))
#define VDMA_DMACR_RUNSTOP              (1U << 0)
#define VDMA_DMACR_CIRCULAR             (1U << 1)
#define VDMA_DMACR_RESET                (1U << 2)
#define VDMA_DMASR_HALTED               (1U << 0)
#define VDMA_DMASR_ERROR_MASK           0x00000ff0U

#define VTC_CTL                         0x000U
#define VTC_ISR                         0x004U
#define VTC_ERROR                       0x008U
#define VTC_VERSION                     0x010U
#define VTC_GASIZE                      0x060U
#define VTC_GTSTAT                      0x064U
#define VTC_GFENC                       0x068U
#define VTC_GPOL                        0x06cU
#define VTC_GHSIZE                      0x070U
#define VTC_GVSIZE                      0x074U
#define VTC_GHSYNC                      0x078U
#define VTC_GVBHOFF                     0x07cU
#define VTC_GVSYNC                      0x080U
#define VTC_GVSHOFF                     0x084U
#define VTC_GVBHOFF_F1                  0x088U
#define VTC_GVSYNC_F1                   0x08cU
#define VTC_GVSHOFF_F1                  0x090U
#define VTC_GASIZE_F1                   0x094U
#define VTC_CTL_RESET                   (1U << 31)
#define VTC_CTL_GE                      (1U << 2)
#define VTC_CTL_RU                      (1U << 1)
#define VTC_CTL_SW                      (1U << 0)
#define VTC_POL_AVP                     (1U << 4)
#define VTC_POL_HSP                     (1U << 3)
#define VTC_POL_VSP                     (1U << 2)

#define PACK_START_END(start, end)      ((uint32_t)(start) | ((uint32_t)(end) << 16))
#define PACK_HV(horizontal, vertical)   ((uint32_t)(horizontal) | ((uint32_t)(vertical) << 16))

extern void rt_hw_cpu_dcache_clean(void *addr, int size);

struct zynq_lcd
{
    struct rt_device parent;
    rt_uint8_t *framebuffer;
};

static struct zynq_lcd _lcd;

static inline uint32_t reg_read(uint32_t base, uint32_t offset)
{
    return *(volatile uint32_t *)(base + offset);
}

static inline void reg_write(uint32_t base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(base + offset) = value;
}

static void zynq_lcd_vtc_start(void)
{
    reg_write(ZYNQ_VTC_BASE, VTC_CTL, VTC_CTL_RESET);
    reg_write(ZYNQ_VTC_BASE, VTC_GASIZE, PACK_HV(1024, 600));
    reg_write(ZYNQ_VTC_BASE, VTC_GASIZE_F1, 600U << 16);
    reg_write(ZYNQ_VTC_BASE, VTC_GHSIZE, 1344);
    reg_write(ZYNQ_VTC_BASE, VTC_GVSIZE, PACK_HV(635, 635));
    reg_write(ZYNQ_VTC_BASE, VTC_GHSYNC, PACK_START_END(1184, 1204));
    reg_write(ZYNQ_VTC_BASE, VTC_GVSYNC, PACK_START_END(612, 615));
    reg_write(ZYNQ_VTC_BASE, VTC_GVSYNC_F1, PACK_START_END(612, 615));
    reg_write(ZYNQ_VTC_BASE, VTC_GVBHOFF, PACK_START_END(1024, 1024));
    reg_write(ZYNQ_VTC_BASE, VTC_GVBHOFF_F1, PACK_START_END(1024, 1024));
    reg_write(ZYNQ_VTC_BASE, VTC_GVSHOFF, PACK_START_END(1184, 1184));
    reg_write(ZYNQ_VTC_BASE, VTC_GVSHOFF_F1, PACK_START_END(1184, 1184));
    reg_write(ZYNQ_VTC_BASE, VTC_GFENC, 0);
    reg_write(ZYNQ_VTC_BASE, VTC_GPOL, VTC_POL_AVP | VTC_POL_HSP | VTC_POL_VSP);
    reg_write(ZYNQ_VTC_BASE, VTC_CTL, VTC_CTL_GE | VTC_CTL_RU | VTC_CTL_SW);
}

static rt_err_t zynq_lcd_vdma_start(void)
{
    uint32_t timeout = 100000U;
    uint32_t address = (uint32_t)_lcd.framebuffer;

    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_DMACR, VDMA_DMACR_RESET);
    while ((reg_read(ZYNQ_VDMA_BASE, VDMA_MM2S_DMACR) & VDMA_DMACR_RESET) && timeout)
    {
        timeout--;
    }
    if (timeout == 0)
    {
        LOG_E("MM2S reset timed out");
        return -RT_ETIMEOUT;
    }

    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_DMASR, VDMA_DMASR_ERROR_MASK);
    reg_write(ZYNQ_VDMA_BASE, VDMA_PARK_PTR, 0);
    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_START_ADDR(0), address);
    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_START_ADDR(1), address);
    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_START_ADDR(2), address);
    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_STRIDE, ZYNQ_LCD_PITCH);
    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_HSIZE, ZYNQ_LCD_PITCH);
    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_DMACR,
              VDMA_DMACR_RUNSTOP | VDMA_DMACR_CIRCULAR);
    reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_VSIZE, ZYNQ_LCD_HEIGHT);

    if (reg_read(ZYNQ_VDMA_BASE, VDMA_MM2S_DMASR) & VDMA_DMASR_HALTED)
    {
        LOG_E("MM2S did not start, status=0x%08x",
              reg_read(ZYNQ_VDMA_BASE, VDMA_MM2S_DMASR));
        return -RT_ERROR;
    }
    return RT_EOK;
}

static void zynq_lcd_flush_rect(const struct rt_device_rect_info *rect)
{
    rt_uint32_t y;

    if (rect == RT_NULL || rect->x >= ZYNQ_LCD_WIDTH || rect->y >= ZYNQ_LCD_HEIGHT)
    {
        rt_hw_cpu_dcache_clean(_lcd.framebuffer, ZYNQ_LCD_FB_SIZE);
        return;
    }

    for (y = rect->y; y < rect->y + rect->height && y < ZYNQ_LCD_HEIGHT; y++)
    {
        rt_uint32_t width = rect->width;
        if (rect->x + width > ZYNQ_LCD_WIDTH)
        {
            width = ZYNQ_LCD_WIDTH - rect->x;
        }
        rt_hw_cpu_dcache_clean(_lcd.framebuffer + y * ZYNQ_LCD_PITCH +
                               rect->x * ZYNQ_LCD_BYTES_PER_PIXEL,
                               width * ZYNQ_LCD_BYTES_PER_PIXEL);
    }
}

static rt_err_t zynq_lcd_init(rt_device_t device)
{
    RT_UNUSED(device);
    return RT_EOK;
}

static rt_err_t zynq_lcd_control(rt_device_t device, int cmd, void *args)
{
    RT_UNUSED(device);

    switch (cmd)
    {
    case RTGRAPHIC_CTRL_RECT_UPDATE:
        zynq_lcd_flush_rect((const struct rt_device_rect_info *)args);
        break;
    case RTGRAPHIC_CTRL_GET_INFO:
        if (args == RT_NULL)
        {
            return -RT_EINVAL;
        }
        ((struct rt_device_graphic_info *)args)->pixel_format = RTGRAPHIC_PIXEL_FORMAT_RGB888;
        ((struct rt_device_graphic_info *)args)->bits_per_pixel = 24;
        ((struct rt_device_graphic_info *)args)->pitch = ZYNQ_LCD_PITCH;
        ((struct rt_device_graphic_info *)args)->width = ZYNQ_LCD_WIDTH;
        ((struct rt_device_graphic_info *)args)->height = ZYNQ_LCD_HEIGHT;
        ((struct rt_device_graphic_info *)args)->framebuffer = _lcd.framebuffer;
        ((struct rt_device_graphic_info *)args)->smem_len = ZYNQ_LCD_FB_SIZE;
        break;
    case RTGRAPHIC_CTRL_POWERON:
        zynq_lcd_vtc_start();
        return zynq_lcd_vdma_start();
    case RTGRAPHIC_CTRL_POWEROFF:
        reg_write(ZYNQ_VDMA_BASE, VDMA_MM2S_DMACR, 0);
        reg_write(ZYNQ_VTC_BASE, VTC_CTL, 0);
        break;
    default:
        return -RT_ENOSYS;
    }
    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_device_ops zynq_lcd_ops =
{
    zynq_lcd_init,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    zynq_lcd_control
};
#endif

static void zynq_lcd_fill_bars(void)
{
    static const rt_uint8_t colors[8][3] =
    {
        {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
        {255, 0, 255}, {255, 0, 0}, {0, 0, 255}, {0, 0, 0}
    };
    rt_uint32_t x;
    rt_uint32_t y;

    for (y = 0; y < ZYNQ_LCD_HEIGHT; y++)
    {
        for (x = 0; x < ZYNQ_LCD_WIDTH; x++)
        {
            const rt_uint8_t *color = colors[(x * 8U) / ZYNQ_LCD_WIDTH];
            rt_uint8_t *pixel = _lcd.framebuffer + y * ZYNQ_LCD_PITCH + x * 3U;
            pixel[0] = color[0];
            pixel[1] = color[1];
            pixel[2] = color[2];
        }
    }
    rt_hw_cpu_dcache_clean(_lcd.framebuffer, ZYNQ_LCD_FB_SIZE);
}

static int zynq_lcd_hw_init(void)
{
    rt_err_t result;

    _lcd.framebuffer = rt_malloc_align(ZYNQ_LCD_FB_SIZE, 64);
    if (_lcd.framebuffer == RT_NULL)
    {
        LOG_E("cannot allocate %u-byte framebuffer", ZYNQ_LCD_FB_SIZE);
        return -RT_ENOMEM;
    }
    zynq_lcd_fill_bars();

    _lcd.parent.type = RT_Device_Class_Graphic;
#ifdef RT_USING_DEVICE_OPS
    _lcd.parent.ops = &zynq_lcd_ops;
#else
    _lcd.parent.init = zynq_lcd_init;
    _lcd.parent.control = zynq_lcd_control;
#endif
    result = rt_device_register(&_lcd.parent, "lcd", RT_DEVICE_FLAG_RDWR);
    if (result != RT_EOK)
    {
        rt_free_align(_lcd.framebuffer);
        return result;
    }

    zynq_lcd_vtc_start();
    result = zynq_lcd_vdma_start();
    if (result != RT_EOK)
    {
        return result;
    }
    LOG_I("1024x600 RGB888 framebuffer @ 0x%08x", (uint32_t)_lcd.framebuffer);
    return RT_EOK;
}
INIT_DEVICE_EXPORT(zynq_lcd_hw_init);

static int lcd_info(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    rt_kprintf("fb=0x%08x size=%u pitch=%u\n", (uint32_t)_lcd.framebuffer,
               ZYNQ_LCD_FB_SIZE, ZYNQ_LCD_PITCH);
    rt_kprintf("vdma: version=0x%08x cr=0x%08x sr=0x%08x park=0x%08x\n",
               reg_read(ZYNQ_VDMA_BASE, VDMA_VERSION),
               reg_read(ZYNQ_VDMA_BASE, VDMA_MM2S_DMACR),
               reg_read(ZYNQ_VDMA_BASE, VDMA_MM2S_DMASR),
               reg_read(ZYNQ_VDMA_BASE, VDMA_PARK_PTR));
    rt_kprintf("vtc: version=0x%08x ctl=0x%08x isr=0x%08x err=0x%08x stat=0x%08x\n",
               reg_read(ZYNQ_VTC_BASE, VTC_VERSION), reg_read(ZYNQ_VTC_BASE, VTC_CTL),
               reg_read(ZYNQ_VTC_BASE, VTC_ISR), reg_read(ZYNQ_VTC_BASE, VTC_ERROR),
               reg_read(ZYNQ_VTC_BASE, VTC_GTSTAT));
    return 0;
}
MSH_CMD_EXPORT(lcd_info, show RGB LCD VDMA and VTC status);

static int lcd_test(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);
    zynq_lcd_fill_bars();
    rt_kprintf("RGB LCD color bars refreshed\n");
    return 0;
}
MSH_CMD_EXPORT(lcd_test, draw RGB LCD color bars);
