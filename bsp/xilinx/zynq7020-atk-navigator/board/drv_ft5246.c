/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rtdevice.h>

#define DBG_TAG                 "touch.ft5246"
#define DBG_LVL                 DBG_INFO
#include <rtdbg.h>

#define FT5246_I2C_BUS          "swi2c0"
#define FT5246_I2C_ADDRESS      0x38U
#define FT5246_IRQ_PIN          60
#define FT5246_MAX_POINTS       5U
#define FT5246_MAX_TRACK_ID     16U
#define FT5246_POINT_BYTES      6U
#define FT5246_FRAME_BYTES      (3U + FT5246_MAX_POINTS * FT5246_POINT_BYTES)
#define FT5246_REG_CHIP_ID      0xa3U
#define FT5246_REG_FW_ID        0xa6U
#define FT5246_REG_VENDOR_ID    0xa8U

struct ft5246_state
{
    struct rt_touch_device touch;
    struct rt_i2c_bus_device *bus;
    rt_uint16_t last_x[FT5246_MAX_TRACK_ID];
    rt_uint16_t last_y[FT5246_MAX_TRACK_ID];
    rt_uint8_t last_width[FT5246_MAX_TRACK_ID];
    rt_uint16_t active_mask;
    rt_uint8_t chip_id;
    rt_uint8_t firmware_id;
    rt_uint8_t vendor_id;
};

static struct ft5246_state _ft5246;

static rt_err_t ft5246_read_regs(rt_uint8_t reg, rt_uint8_t *data, rt_size_t size)
{
    struct rt_i2c_msg messages[2];

    messages[0].addr = FT5246_I2C_ADDRESS;
    messages[0].flags = RT_I2C_WR;
    messages[0].buf = &reg;
    messages[0].len = 1;
    messages[1].addr = FT5246_I2C_ADDRESS;
    messages[1].flags = RT_I2C_RD;
    messages[1].buf = data;
    messages[1].len = size;

    return rt_i2c_transfer(_ft5246.bus, messages, 2) == 2 ? RT_EOK : -RT_EIO;
}

static rt_err_t ft5246_write_reg(rt_uint8_t reg, rt_uint8_t value)
{
    struct rt_i2c_msg message;
    rt_uint8_t data[2] = {reg, value};

    message.addr = FT5246_I2C_ADDRESS;
    message.flags = RT_I2C_WR;
    message.buf = data;
    message.len = sizeof(data);
    return rt_i2c_transfer(_ft5246.bus, &message, 1) == 1 ? RT_EOK : -RT_EIO;
}

static void ft5246_set_event(struct rt_touch_data *point, rt_uint8_t event,
                             rt_uint8_t id, rt_uint16_t x, rt_uint16_t y,
                             rt_uint8_t width)
{
    point->event = event;
    point->track_id = id;
    point->x_coordinate = x;
    point->y_coordinate = y;
    point->width = width;
    point->timestamp = rt_touch_get_ts();
}

static rt_size_t ft5246_read_points(struct rt_touch_device *touch, void *buffer,
                                    rt_size_t point_count)
{
    rt_uint8_t frame[FT5246_FRAME_BYTES];
    struct rt_touch_data *points = buffer;
    rt_uint16_t current_mask = 0;
    rt_uint8_t reported;
    rt_size_t output = 0;
    rt_uint8_t index;

    RT_UNUSED(touch);
    if (point_count == 0 || points == RT_NULL)
    {
        return 0;
    }
    if (point_count > FT5246_MAX_POINTS)
    {
        point_count = FT5246_MAX_POINTS;
    }
    rt_memset(points, 0, sizeof(*points) * point_count);

    if (ft5246_read_regs(0, frame, sizeof(frame)) != RT_EOK)
    {
        LOG_E("failed to read touch frame");
        return 0;
    }

    reported = frame[2] & 0x0fU;
    /* This panel returns 0xff in the idle touch-data window. */
    if (reported == 0x0fU)
    {
        reported = 0;
    }
    else if (reported > FT5246_MAX_POINTS)
    {
        LOG_W("invalid point count %u", reported);
        return 0;
    }

    for (index = 0; index < reported && output < point_count; index++)
    {
        const rt_uint8_t *raw = &frame[3U + index * FT5246_POINT_BYTES];
        rt_uint8_t event = raw[0] >> 6;
        rt_uint8_t id = raw[2] >> 4;
        rt_uint16_t raw_x = ((rt_uint16_t)(raw[0] & 0x0fU) << 8) | raw[1];
        rt_uint16_t raw_y = ((rt_uint16_t)(raw[2] & 0x0fU) << 8) | raw[3];
        rt_uint16_t x = raw_y;
        rt_uint16_t y = raw_x;

        if (id >= FT5246_MAX_TRACK_ID || x >= 1024U || y >= 600U)
        {
            continue;
        }
        current_mask |= 1U << id;
        _ft5246.last_x[id] = x;
        _ft5246.last_y[id] = y;
        _ft5246.last_width[id] = raw[4];
        ft5246_set_event(&points[output++],
                         event == 0U ? RT_TOUCH_EVENT_DOWN :
                         event == 1U ? RT_TOUCH_EVENT_UP : RT_TOUCH_EVENT_MOVE,
                         id, x, y, raw[4]);
    }

    for (index = 0; index < FT5246_MAX_TRACK_ID && output < point_count; index++)
    {
        rt_uint16_t mask = 1U << index;
        if ((_ft5246.active_mask & mask) && !(current_mask & mask))
        {
            ft5246_set_event(&points[output++], RT_TOUCH_EVENT_UP, index,
                             _ft5246.last_x[index], _ft5246.last_y[index],
                             _ft5246.last_width[index]);
        }
    }
    _ft5246.active_mask = current_mask;
    return point_count;
}

static rt_err_t ft5246_control(struct rt_touch_device *touch, int command, void *arg)
{
    switch (command)
    {
    case RT_TOUCH_CTRL_GET_INFO:
        if (arg == RT_NULL)
        {
            return -RT_EINVAL;
        }
        rt_memcpy(arg, &touch->info, sizeof(touch->info));
        return RT_EOK;
    case RT_TOUCH_CTRL_GET_ID:
        if (arg == RT_NULL)
        {
            return -RT_EINVAL;
        }
        *(rt_uint8_t *)arg = _ft5246.chip_id;
        return RT_EOK;
    default:
        return -RT_ENOSYS;
    }
}

static const struct rt_touch_ops ft5246_ops =
{
    .touch_readpoint = ft5246_read_points,
    .touch_control = ft5246_control,
};

static int ft5246_hw_init(void)
{
    rt_err_t result;

    _ft5246.bus = (struct rt_i2c_bus_device *)rt_device_find(FT5246_I2C_BUS);
    if (_ft5246.bus == RT_NULL)
    {
        LOG_E("cannot find %s", FT5246_I2C_BUS);
        return -RT_ENOSYS;
    }
    ft5246_write_reg(0x00, 0x00); /* working mode */
    ft5246_write_reg(0xa5, 0x00); /* active power mode */
    rt_thread_mdelay(20);
    if (ft5246_read_regs(FT5246_REG_CHIP_ID, &_ft5246.chip_id, 1) != RT_EOK ||
        ft5246_read_regs(FT5246_REG_FW_ID, &_ft5246.firmware_id, 1) != RT_EOK ||
        ft5246_read_regs(FT5246_REG_VENDOR_ID, &_ft5246.vendor_id, 1) != RT_EOK)
    {
        LOG_E("controller did not respond at 0x%02x", FT5246_I2C_ADDRESS);
        return -RT_EIO;
    }

    _ft5246.touch.info.type = RT_TOUCH_TYPE_CAPACITANCE;
    _ft5246.touch.info.vendor = RT_TOUCH_VENDOR_FT;
    _ft5246.touch.info.point_num = FT5246_MAX_POINTS;
    _ft5246.touch.info.range_x = 1024;
    _ft5246.touch.info.range_y = 600;
    _ft5246.touch.config.dev_name = FT5246_I2C_BUS;
    _ft5246.touch.config.irq_pin.pin = FT5246_IRQ_PIN;
    _ft5246.touch.config.irq_pin.mode = PIN_MODE_INPUT_PULLUP;
    _ft5246.touch.ops = &ft5246_ops;

    result = rt_hw_touch_register(&_ft5246.touch, "touch0",
                                  RT_DEVICE_FLAG_INT_RX, RT_NULL);
    if (result != RT_EOK)
    {
        return result;
    }
    LOG_I("chip=0x%02x fw=0x%02x vendor=0x%02x irq=GPIO%d",
          _ft5246.chip_id, _ft5246.firmware_id, _ft5246.vendor_id,
          FT5246_IRQ_PIN);
    return RT_EOK;
}
INIT_DEVICE_EXPORT(ft5246_hw_init);

static int touch_info(int argc, char **argv)
{
    rt_uint8_t irq_level;

    RT_UNUSED(argc);
    RT_UNUSED(argv);
    irq_level = rt_pin_read(FT5246_IRQ_PIN);
    rt_kprintf("FT5246: addr=0x%02x chip=0x%02x fw=0x%02x vendor=0x%02x "
               "irq=GPIO%d level=%u\n",
               FT5246_I2C_ADDRESS, _ft5246.chip_id, _ft5246.firmware_id,
               _ft5246.vendor_id, FT5246_IRQ_PIN, irq_level);
    return 0;
}
MSH_CMD_EXPORT(touch_info, show FT5246 identity and interrupt level);

static int touch_read(int argc, char **argv)
{
    struct rt_touch_data points[FT5246_MAX_POINTS];
    rt_size_t index;

    RT_UNUSED(argc);
    RT_UNUSED(argv);
    ft5246_read_points(&_ft5246.touch, points, FT5246_MAX_POINTS);
    for (index = 0; index < FT5246_MAX_POINTS; index++)
    {
        if (points[index].event != RT_TOUCH_EVENT_NONE)
        {
            rt_kprintf("id=%u event=%u x=%u y=%u width=%u\n",
                       points[index].track_id, points[index].event,
                       points[index].x_coordinate, points[index].y_coordinate,
                       points[index].width);
        }
    }
    return 0;
}
MSH_CMD_EXPORT(touch_read, read one FT5246 touch frame);
