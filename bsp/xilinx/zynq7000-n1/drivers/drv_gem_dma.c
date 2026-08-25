/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>

#define ZYNQ_GEM_DMA_START  ((rt_ubase_t)0x08000000U)
#define ZYNQ_GEM_DMA_SIZE   ((rt_size_t)0x00200000U)

void *macb_platform_dma_alloc(struct rt_device *dev, rt_size_t size,
                              rt_ubase_t *dma_handle, rt_ubase_t flags)
{
    static rt_bool_t allocated;

    RT_UNUSED(dev);
    RT_UNUSED(flags);

    if (allocated || !dma_handle || size > ZYNQ_GEM_DMA_SIZE)
    {
        return RT_NULL;
    }

    allocated = RT_TRUE;
    *dma_handle = ZYNQ_GEM_DMA_START;

    return (void *)ZYNQ_GEM_DMA_START;
}

void macb_platform_dma_free(struct rt_device *dev, rt_size_t size,
                            void *cpu_addr, rt_ubase_t dma_handle,
                            rt_ubase_t flags)
{
    RT_UNUSED(dev);
    RT_UNUSED(size);
    RT_UNUSED(cpu_addr);
    RT_UNUSED(dma_handle);
    RT_UNUSED(flags);
}
