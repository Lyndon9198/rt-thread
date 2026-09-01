/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtdevice.h>
#include <drivers/core/power.h>
#include <drivers/ofw.h>

#ifdef RT_USING_SFUD
#include <dev_spi_flash_sfud.h>
#endif

#ifdef DBG_TAG
#undef DBG_TAG
#endif
#define DBG_TAG "qspi.zynq"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define ZYNQ_QSPI_BASE          0xe000d000U
#define ZYNQ_QSPI_INPUT_CLK     200000000U

#define QSPI_CR                 0x00U
#define QSPI_SR                 0x04U
#define QSPI_IDR                0x0cU
#define QSPI_ER                 0x14U
#define QSPI_TXD0               0x1cU
#define QSPI_RXD                0x20U
#define QSPI_TXWR               0x28U
#define QSPI_RXWR               0x2cU
#define QSPI_TXD1               0x80U
#define QSPI_TXD2               0x84U
#define QSPI_TXD3               0x88U

#define QSPI_CR_IFMODE          (1U << 31)
#define QSPI_CR_SSFORCE         (1U << 14)
#define QSPI_CR_SSCTRL          (1U << 10)
#define QSPI_CR_DATA_32BIT      (3U << 6)
#define QSPI_CR_PRESCALE_MASK   (7U << 3)
#define QSPI_CR_CPHA            (1U << 2)
#define QSPI_CR_CPOL            (1U << 1)
#define QSPI_CR_MASTER          (1U << 0)
#define QSPI_CR_HOLD_B          (1U << 19)

#define QSPI_SR_RX_NOT_EMPTY    (1U << 4)
#define QSPI_INTR_DISABLE_ALL   0x7dU
#define QSPI_ENABLE             1U
#define QSPI_TIMEOUT            1000000U

struct zynq_qspi
{
    struct rt_spi_bus bus;
    volatile rt_uint32_t *regs;
};

static struct zynq_qspi qspi0;
static struct rt_spi_device qspi_flash0;

static rt_uint32_t qspi_read(struct zynq_qspi *qspi, rt_uint32_t offset)
{
    return qspi->regs[offset / sizeof(rt_uint32_t)];
}

static void qspi_write(struct zynq_qspi *qspi, rt_uint32_t offset,
                       rt_uint32_t value)
{
    qspi->regs[offset / sizeof(rt_uint32_t)] = value;
}

static rt_err_t zynq_qspi_configure(struct rt_spi_device *device,
                                    struct rt_spi_configuration *configuration)
{
    rt_uint32_t cr;
    rt_uint32_t divisor = 2U;
    rt_uint32_t prescaler = 0U;
    struct zynq_qspi *qspi = rt_container_of(device->bus, struct zynq_qspi, bus);

    if (configuration->data_width != 8U ||
        (configuration->mode & RT_SPI_SLAVE) ||
        !(configuration->mode & RT_SPI_MSB))
    {
        return -RT_EINVAL;
    }

    while (prescaler < 7U &&
           (ZYNQ_QSPI_INPUT_CLK / divisor) > configuration->max_hz)
    {
        ++prescaler;
        divisor <<= 1;
    }

    qspi_write(qspi, QSPI_ER, 0U);
    qspi_write(qspi, QSPI_IDR, QSPI_INTR_DISABLE_ALL);
    qspi_write(qspi, QSPI_TXWR, 1U);
    qspi_write(qspi, QSPI_RXWR, 1U);

    cr = QSPI_CR_IFMODE | QSPI_CR_SSFORCE | QSPI_CR_SSCTRL |
         QSPI_CR_DATA_32BIT | QSPI_CR_MASTER | QSPI_CR_HOLD_B |
         (prescaler << 3);
    if (configuration->mode & RT_SPI_CPHA)
    {
        cr |= QSPI_CR_CPHA;
    }
    if (configuration->mode & RT_SPI_CPOL)
    {
        cr |= QSPI_CR_CPOL;
    }

    qspi_write(qspi, QSPI_CR, cr);
    qspi_write(qspi, QSPI_ER, QSPI_ENABLE);
    configuration->usage_freq = ZYNQ_QSPI_INPUT_CLK / divisor;

    return RT_EOK;
}

static rt_uint32_t zynq_qspi_pack(const rt_uint8_t *buffer, rt_size_t size)
{
    rt_uint32_t value = 0xffffffffU;
    rt_uint8_t *bytes = (rt_uint8_t *)&value;

    if (buffer)
    {
        for (rt_size_t i = 0; i < size; ++i)
        {
            bytes[i] = buffer[i];
        }
    }

    return value;
}

static void zynq_qspi_unpack(rt_uint8_t *buffer, rt_size_t size,
                             rt_uint32_t value)
{
    if (!buffer)
    {
        return;
    }

    if (size == 1U)
    {
        buffer[0] = (rt_uint8_t)(value >> 24);
    }
    else if (size == 2U)
    {
        buffer[0] = (rt_uint8_t)(value >> 8);
        buffer[1] = (rt_uint8_t)(value >> 24);
    }
    else if (size == 3U)
    {
        buffer[0] = (rt_uint8_t)(value >> 8);
        buffer[1] = (rt_uint8_t)(value >> 16);
        buffer[2] = (rt_uint8_t)(value >> 24);
    }
    else
    {
        rt_uint8_t *bytes = (rt_uint8_t *)&value;

        for (rt_size_t i = 0; i < size; ++i)
        {
            buffer[i] = bytes[i];
        }
    }
}

static rt_err_t zynq_qspi_word(struct zynq_qspi *qspi,
                               const rt_uint8_t *send_buf,
                               rt_uint8_t *recv_buf, rt_size_t size)
{
    rt_uint32_t timeout = QSPI_TIMEOUT;
    rt_uint32_t tx_offset;

    if (size == 1U)
    {
        tx_offset = QSPI_TXD1;
    }
    else if (size == 2U)
    {
        tx_offset = QSPI_TXD2;
    }
    else if (size == 3U)
    {
        tx_offset = QSPI_TXD3;
    }
    else
    {
        tx_offset = QSPI_TXD0;
    }

    qspi_write(qspi, tx_offset, zynq_qspi_pack(send_buf, size));

    while (!(qspi_read(qspi, QSPI_SR) & QSPI_SR_RX_NOT_EMPTY))
    {
        if (--timeout == 0U)
        {
            return -RT_ETIMEOUT;
        }
    }

    zynq_qspi_unpack(recv_buf, size, qspi_read(qspi, QSPI_RXD));

    return RT_EOK;
}

static rt_ssize_t zynq_qspi_xfer(struct rt_spi_device *device,
                                 struct rt_spi_message *message)
{
    rt_err_t err;
    rt_size_t done = 0;
    rt_uint32_t cr;
    const rt_uint8_t *send_buf = message->send_buf;
    rt_uint8_t *recv_buf = message->recv_buf;
    struct zynq_qspi *qspi = rt_container_of(device->bus, struct zynq_qspi, bus);

    cr = qspi_read(qspi, QSPI_CR);
    if (message->cs_take)
    {
        cr &= ~QSPI_CR_SSCTRL;
        qspi_write(qspi, QSPI_CR, cr);
    }

    while (done < message->length)
    {
        rt_size_t count = rt_min_t(rt_size_t, 4U, message->length - done);

        err = zynq_qspi_word(qspi,
                             send_buf ? send_buf + done : RT_NULL,
                             recv_buf ? recv_buf + done : RT_NULL,
                             count);
        if (err)
        {
            cr = qspi_read(qspi, QSPI_CR) | QSPI_CR_SSCTRL;
            qspi_write(qspi, QSPI_CR, cr);
            LOG_E("transfer timeout after %u bytes", (unsigned int)done);
            return err;
        }

        done += count;
    }

    if (message->cs_release)
    {
        cr = qspi_read(qspi, QSPI_CR) | QSPI_CR_SSCTRL;
        qspi_write(qspi, QSPI_CR, cr);
    }

    return (rt_ssize_t)done;
}

static const struct rt_spi_ops zynq_qspi_ops =
{
    .configure = zynq_qspi_configure,
    .xfer = zynq_qspi_xfer,
};

static rt_err_t zynq_qspi_prepare_reset(struct rt_device *device)
{
    rt_uint8_t exit_4byte = 0xe9U;
    rt_uint32_t cr;
    struct rt_spi_bus *bus = rt_container_of(device, struct rt_spi_bus, parent);
    struct zynq_qspi *qspi = rt_container_of(bus, struct zynq_qspi, bus);

    cr = qspi_read(qspi, QSPI_CR) & ~QSPI_CR_SSCTRL;
    qspi_write(qspi, QSPI_CR, cr);
    if (zynq_qspi_word(qspi, &exit_4byte, RT_NULL, 1U))
    {
        return -RT_EIO;
    }
    qspi_write(qspi, QSPI_CR, cr | QSPI_CR_SSCTRL);
    rt_hw_us_delay(50U);

    return RT_EOK;
}

static int zynq_qspi_init(void)
{
    rt_err_t err;
    struct rt_spi_configuration config =
    {
        .mode = RT_SPI_MODE_0 | RT_SPI_MSB,
        .data_width = 8,
        .max_hz = 50000000U,
    };

    qspi0.regs = (volatile rt_uint32_t *)ZYNQ_QSPI_BASE;
    qspi0.bus.parent.ofw_node =
        rt_ofw_find_node_by_compatible(RT_NULL, "xlnx,zynq-qspi-1.0");
    if (!qspi0.bus.parent.ofw_node)
    {
        return -RT_ERROR;
    }

    err = rt_spi_bus_register(&qspi0.bus, "qspi0", &zynq_qspi_ops);
    if (err)
    {
        return err;
    }

    err = rt_spi_bus_attach_device(&qspi_flash0, "qspi00", "qspi0", RT_NULL);
    if (err)
    {
        return err;
    }

    err = rt_spi_configure(&qspi_flash0, &config);
    if (err)
    {
        return err;
    }

    return rt_dm_power_off_handler(&qspi0.bus.parent,
                                   RT_DM_POWER_OFF_MODE_RESET,
                                   RT_DM_POWER_OFF_PRIO_PLATFORM,
                                   zynq_qspi_prepare_reset);
}
INIT_DEVICE_EXPORT(zynq_qspi_init);

#ifdef RT_USING_SFUD
static int zynq_qspi_flash_init(void)
{
    if (!rt_sfud_flash_probe("nor0", "qspi00"))
    {
        LOG_E("failed to probe SPI NOR");
        return -RT_ERROR;
    }

    return RT_EOK;
}
INIT_ENV_EXPORT(zynq_qspi_flash_init);
#endif

static int qspi_id(int argc, char **argv)
{
    rt_uint8_t command = 0x9fU;
    rt_uint8_t id[3] = {0};

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    if (rt_spi_send_then_recv(&qspi_flash0, &command, 1U, id, sizeof(id)) != RT_EOK)
    {
        return -RT_ERROR;
    }

    rt_kprintf("QSPI JEDEC ID: %02x %02x %02x\n", id[0], id[1], id[2]);

    return (id[0] == 0xefU && id[2] == 0x19U) ? RT_EOK : -RT_ERROR;
}
MSH_CMD_EXPORT(qspi_id, read the QSPI NOR JEDEC ID);

#ifdef RT_USING_SFUD
static int qspi_test(int argc, char **argv)
{
    static const rt_uint8_t pattern[] = "RT-Thread Zynq7020 QSPI test";
    rt_uint8_t buffer[256];
    rt_uint32_t address;
    sfud_flash_t flash = rt_sfud_flash_find_by_dev_name("nor0");

    if (!flash)
    {
        rt_kprintf("nor0 is not available\n");
        return -RT_ERROR;
    }

    rt_kprintf("QSPI: %s, %u bytes, erase granularity %u bytes\n",
               flash->name, (unsigned int)flash->chip.capacity,
               (unsigned int)flash->chip.erase_gran);

    if (argc != 2 || rt_strcmp(argv[1], "rw"))
    {
        rt_kprintf("read-only check passed; use 'qspi_test rw' for a restored write test\n");
        return RT_EOK;
    }

    address = flash->chip.capacity - flash->chip.erase_gran;
    for (rt_uint32_t offset = 0; offset < flash->chip.erase_gran;
         offset += sizeof(buffer))
    {
        rt_size_t size = rt_min_t(rt_size_t, sizeof(buffer),
                                  flash->chip.erase_gran - offset);

        if (sfud_read(flash, address + offset, size, buffer) != SFUD_SUCCESS)
        {
            rt_kprintf("failed to inspect the test sector\n");
            return -RT_EIO;
        }

        for (rt_size_t i = 0; i < size; ++i)
        {
            if (buffer[i] != 0xffU)
            {
                rt_kprintf("test sector 0x%08x is not empty; refusing to modify it\n",
                           address);
                return -RT_EBUSY;
            }
        }
    }

    if (sfud_write(flash, address, sizeof(pattern), pattern) != SFUD_SUCCESS ||
        sfud_read(flash, address, sizeof(pattern), buffer) != SFUD_SUCCESS ||
        rt_memcmp(pattern, buffer, sizeof(pattern)))
    {
        rt_kprintf("QSPI write/read verification failed\n");
        return -RT_EIO;
    }

    if (sfud_erase(flash, address, flash->chip.erase_gran) != SFUD_SUCCESS ||
        sfud_read(flash, address, sizeof(pattern), buffer) != SFUD_SUCCESS)
    {
        rt_kprintf("failed to restore the test sector\n");
        return -RT_EIO;
    }

    for (rt_size_t i = 0; i < sizeof(pattern); ++i)
    {
        if (buffer[i] != 0xffU)
        {
            rt_kprintf("test sector restore verification failed\n");
            return -RT_ERROR;
        }
    }

    rt_kprintf("QSPI write/read/erase/restore passed at 0x%08x\n", address);

    return RT_EOK;
}
MSH_CMD_EXPORT(qspi_test, test QSPI NOR; add rw to test and restore an empty sector);
#endif
