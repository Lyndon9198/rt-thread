/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/input.h>

#define DBG_TAG                 "input.keys"
#define DBG_LVL                 DBG_INFO
#include <rtdbg.h>

#define ZYNQ_KEY_COUNT          3U
#define ZYNQ_KEY_DEBOUNCE_MS    10U

struct zynq_key
{
    const char *name;
    rt_base_t pin;
    rt_uint16_t code;
    rt_uint8_t pressed;
    rt_uint32_t events;
    struct rt_timer debounce;
};

static struct rt_input_device _keys_input;
static struct zynq_key _keys[ZYNQ_KEY_COUNT] =
{
    {.name = "PL_KEY0", .pin = 55, .code = BTN_2},
    {.name = "PL_KEY1", .pin = 56, .code = BTN_3},
    {.name = "PL_RESET", .pin = 57, .code = KEY_RESTART},
};

static void zynq_key_debounce(void *parameter)
{
    struct zynq_key *key = parameter;
    rt_uint8_t pressed = rt_pin_read(key->pin) == PIN_LOW;

    if (pressed == key->pressed)
    {
        return;
    }

    key->pressed = pressed;
    key->events++;
    rt_input_report_key(&_keys_input, key->code, pressed);
    rt_input_sync(&_keys_input);
}

static void zynq_key_irq(void *parameter)
{
    struct zynq_key *key = parameter;

    rt_timer_stop(&key->debounce);
    rt_timer_start(&key->debounce);
}

static int zynq_keys_hw_init(void)
{
    rt_err_t result;
    rt_size_t index;

    rt_memset(&_keys_input, 0, sizeof(_keys_input));
    for (index = 0; index < ZYNQ_KEY_COUNT; index++)
    {
        struct zynq_key *key = &_keys[index];

        rt_input_set_capability(&_keys_input, EV_KEY, key->code);
        rt_pin_mode(key->pin, PIN_MODE_INPUT_PULLUP);
        key->pressed = rt_pin_read(key->pin) == PIN_LOW;
        rt_timer_init(&key->debounce, key->name, zynq_key_debounce, key,
                      rt_tick_from_millisecond(ZYNQ_KEY_DEBOUNCE_MS),
                      RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);
    }

    result = rt_input_device_register(&_keys_input);
    if (result != RT_EOK)
    {
        return result;
    }

    for (index = 0; index < ZYNQ_KEY_COUNT; index++)
    {
        struct zynq_key *key = &_keys[index];

        result = rt_pin_attach_irq(key->pin, PIN_IRQ_MODE_RISING_FALLING,
                                   zynq_key_irq, key);
        if (result != RT_EOK)
        {
            LOG_E("cannot attach %s IRQ: %d", key->name, result);
            return result;
        }
        result = rt_pin_irq_enable(key->pin, PIN_IRQ_ENABLE);
        if (result != RT_EOK)
        {
            LOG_E("cannot enable %s IRQ: %d", key->name, result);
            return result;
        }
    }

    LOG_I("input0: PL_KEY0=GPIO55 PL_KEY1=GPIO56 PL_RESET=GPIO57");
    return RT_EOK;
}
INIT_DEVICE_EXPORT(zynq_keys_hw_init);

static int key_info(int argc, char **argv)
{
    rt_size_t index;

    RT_UNUSED(argc);
    RT_UNUSED(argv);
    for (index = 0; index < ZYNQ_KEY_COUNT; index++)
    {
        struct zynq_key *key = &_keys[index];

        rt_kprintf("%-8s GPIO%d level=%d pressed=%u events=%u code=0x%03x\n",
                   key->name, key->pin, rt_pin_read(key->pin), key->pressed,
                   key->events, key->code);
    }
    return 0;
}
MSH_CMD_EXPORT(key_info, show PL key levels and debounced event counters);
