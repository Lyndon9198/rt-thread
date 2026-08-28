/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtdevice.h>

#include <board.h>

#ifdef BSP_USING_CAN0

#define CAN_REG(offset)             __REG32(ZYNQ_CAN0_BASE + (offset))

#define CAN_SRR                     CAN_REG(0x00U)
#define CAN_MSR                     CAN_REG(0x04U)
#define CAN_BRPR                    CAN_REG(0x08U)
#define CAN_BTR                     CAN_REG(0x0CU)
#define CAN_ECR                     CAN_REG(0x10U)
#define CAN_ESR                     CAN_REG(0x14U)
#define CAN_SR                      CAN_REG(0x18U)
#define CAN_ISR                     CAN_REG(0x1CU)
#define CAN_IER                     CAN_REG(0x20U)
#define CAN_ICR                     CAN_REG(0x24U)
#define CAN_TX_ID                   CAN_REG(0x30U)
#define CAN_TX_DLC                  CAN_REG(0x34U)
#define CAN_TX_DW1                  CAN_REG(0x38U)
#define CAN_TX_DW2                  CAN_REG(0x3CU)
#define CAN_RX_ID                   CAN_REG(0x50U)
#define CAN_RX_DLC                  CAN_REG(0x54U)
#define CAN_RX_DW1                  CAN_REG(0x58U)
#define CAN_RX_DW2                  CAN_REG(0x5CU)
#define CAN_AFR                     CAN_REG(0x60U)

#define CAN_SRR_RESET               (1U << 0)
#define CAN_SRR_ENABLE              (1U << 1)
#define CAN_MSR_SLEEP               (1U << 0)
#define CAN_MSR_LOOPBACK            (1U << 1)
#define CAN_MSR_SNOOP               (1U << 2)

#define CAN_SR_CONFIG               (1U << 0)
#define CAN_SR_LOOPBACK             (1U << 1)
#define CAN_SR_SLEEP                (1U << 2)
#define CAN_SR_NORMAL               (1U << 3)
#define CAN_SR_TX_FULL              (1U << 10)

#define CAN_INT_TX_OK               (1U << 1)
#define CAN_INT_RX_UNDERFLOW        (1U << 5)
#define CAN_INT_RX_OVERFLOW         (1U << 6)
#define CAN_INT_RX_NOT_EMPTY        (1U << 7)
#define CAN_INT_ERROR               (1U << 8)
#define CAN_INT_BUS_OFF             (1U << 9)
#define CAN_INT_RX_MASK             (CAN_INT_RX_OVERFLOW | \
                                     CAN_INT_RX_NOT_EMPTY)
#define CAN_INT_ERROR_MASK          (CAN_INT_ERROR | CAN_INT_BUS_OFF)
#define CAN_INT_ALL                 (CAN_INT_TX_OK | CAN_INT_RX_MASK | \
                                     CAN_INT_RX_UNDERFLOW | CAN_INT_ERROR_MASK)

#define CAN_ID_STD_SHIFT            21U
#define CAN_ID_SRR                  (1U << 20)
#define CAN_ID_IDE                  (1U << 19)
#define CAN_ID_EXT_SHIFT            1U
#define CAN_ID_RTR                  (1U << 0)
#define CAN_DLC_SHIFT               28U

#define CAN_BTR_TS1                 14U
#define CAN_BTR_TS2                 3U
#define CAN_BTR_SJW                 0U
#define CAN_MODE_TIMEOUT            100000U

static struct rt_can_device zynq_can0;

static rt_uint32_t zynq_can_pack_data(const rt_uint8_t *data)
{
    return ((rt_uint32_t)data[0] << 24) |
           ((rt_uint32_t)data[1] << 16) |
           ((rt_uint32_t)data[2] << 8) |
           data[3];
}

static void zynq_can_unpack_data(rt_uint32_t value, rt_uint8_t *data)
{
    data[0] = (rt_uint8_t)(value >> 24);
    data[1] = (rt_uint8_t)(value >> 16);
    data[2] = (rt_uint8_t)(value >> 8);
    data[3] = (rt_uint8_t)value;
}

static rt_uint32_t zynq_can_encode_id(const struct rt_can_msg *message)
{
    if (message->ide == RT_CAN_EXTID)
    {
        return ((message->id >> 18) << CAN_ID_STD_SHIFT) |
               CAN_ID_SRR | CAN_ID_IDE |
               ((message->id & 0x3FFFFU) << CAN_ID_EXT_SHIFT) |
               (message->rtr == RT_CAN_RTR ? CAN_ID_RTR : 0U);
    }

    return ((message->id & 0x7FFU) << CAN_ID_STD_SHIFT) |
           (message->rtr == RT_CAN_RTR ? CAN_ID_SRR : 0U);
}

static rt_bool_t zynq_can_wait_status(rt_uint32_t mask)
{
    rt_uint32_t timeout = CAN_MODE_TIMEOUT;

    while (timeout--)
    {
        if (CAN_SR & mask)
        {
            return RT_TRUE;
        }
    }
    return RT_FALSE;
}

static rt_err_t zynq_can_enter_config(void)
{
    CAN_SRR = 0U;
    return zynq_can_wait_status(CAN_SR_CONFIG) ? RT_EOK : -RT_ETIMEOUT;
}

static rt_err_t zynq_can_enter_mode(rt_uint32_t mode)
{
    rt_uint32_t status;

    if (mode == RT_CAN_MODE_NORMAL)
    {
        CAN_MSR = 0U;
        status = CAN_SR_NORMAL;
    }
    else if (mode == RT_CAN_MODE_LISTEN)
    {
        CAN_MSR = CAN_MSR_SNOOP;
        status = CAN_SR_NORMAL;
    }
    else if (mode == RT_CAN_MODE_LOOPBACK ||
             mode == RT_CAN_MODE_LOOPBACKANLISTEN)
    {
        CAN_MSR = CAN_MSR_LOOPBACK;
        status = CAN_SR_LOOPBACK;
    }
    else
    {
        return -RT_EINVAL;
    }

    CAN_SRR = CAN_SRR_ENABLE;
    return zynq_can_wait_status(status) ? RT_EOK : -RT_ETIMEOUT;
}

static rt_err_t zynq_can_configure(struct rt_can_device *can,
                                   struct can_configure *config)
{
    rt_uint32_t prescaler;
    rt_uint32_t interrupts;
    rt_err_t result;

    if (!config || config->baud_rate == 0U ||
        ZYNQ_CAN0_CLOCK_HZ % (config->baud_rate * 20U))
    {
        return -RT_EINVAL;
    }

    prescaler = ZYNQ_CAN0_CLOCK_HZ / (config->baud_rate * 20U);
    if (prescaler == 0U || prescaler > 256U)
    {
        return -RT_EINVAL;
    }

    interrupts = CAN_IER;
    CAN_SRR = CAN_SRR_RESET;
    result = zynq_can_enter_config();
    if (result != RT_EOK)
    {
        return result;
    }

    CAN_BRPR = prescaler - 1U;
    CAN_BTR = (CAN_BTR_SJW << 7) | (CAN_BTR_TS2 << 4) | CAN_BTR_TS1;
    CAN_AFR = 0U;
    CAN_ICR = CAN_INT_ALL;
    result = zynq_can_enter_mode(config->mode);
    if (result == RT_EOK)
    {
        can->config = *config;
        CAN_IER = interrupts;
    }
    return result;
}

static rt_err_t zynq_can_control(struct rt_can_device *can, int command,
                                 void *argument)
{
    struct can_configure config;
    rt_uint32_t interrupt_mask;
    rt_uint32_t flags = (rt_uint32_t)(rt_ubase_t)argument;

    switch (command)
    {
    case RT_DEVICE_CTRL_SET_INT:
    case RT_DEVICE_CTRL_CLR_INT:
        interrupt_mask = 0U;
        if (flags & RT_DEVICE_FLAG_INT_RX)
        {
            interrupt_mask |= CAN_INT_RX_MASK;
        }
        if (flags & RT_DEVICE_FLAG_INT_TX)
        {
            interrupt_mask |= CAN_INT_TX_OK;
        }
        if (flags & RT_DEVICE_CAN_INT_ERR)
        {
            interrupt_mask |= CAN_INT_ERROR_MASK;
        }
        if (command == RT_DEVICE_CTRL_SET_INT)
        {
            CAN_IER |= interrupt_mask;
            rt_hw_interrupt_umask(ZYNQ_CAN0_IRQ);
        }
        else
        {
            CAN_IER &= ~interrupt_mask;
            if (CAN_IER == 0U)
            {
                rt_hw_interrupt_mask(ZYNQ_CAN0_IRQ);
            }
        }
        return RT_EOK;

    case RT_CAN_CMD_SET_MODE:
        config = can->config;
        config.mode = flags;
        return zynq_can_configure(can, &config);

    case RT_CAN_CMD_SET_BAUD:
        config = can->config;
        config.baud_rate = flags;
        return zynq_can_configure(can, &config);

    case RT_CAN_CMD_SET_PRIV:
        if (flags != RT_CAN_MODE_NOPRIV)
        {
            return -RT_ENOSYS;
        }
        can->config.privmode = flags;
        return RT_EOK;

    case RT_CAN_CMD_GET_STATUS:
        if (!argument)
        {
            return -RT_EINVAL;
        }
        can->status.rcverrcnt = (CAN_ECR >> 8) & 0xFFU;
        can->status.snderrcnt = CAN_ECR & 0xFFU;
        rt_memcpy(argument, &can->status, sizeof(can->status));
        return RT_EOK;

    case RT_CAN_CMD_START:
        if (flags)
        {
            return zynq_can_enter_mode(can->config.mode);
        }
        return zynq_can_enter_config();

    case RT_CAN_CMD_SET_FILTER:
        return -RT_ENOSYS;

    default:
        return -RT_ENOSYS;
    }
}

static rt_ssize_t zynq_can_send(struct rt_can_device *can, const void *buffer,
                                rt_uint32_t mailbox)
{
    const struct rt_can_msg *message = buffer;

    RT_UNUSED(can);
    RT_UNUSED(mailbox);
    if (!message || message->len > 8U)
    {
        return -RT_EINVAL;
    }
    if (CAN_SR & CAN_SR_TX_FULL)
    {
        return -RT_EBUSY;
    }

    CAN_TX_ID = zynq_can_encode_id(message);
    CAN_TX_DLC = (rt_uint32_t)message->len << CAN_DLC_SHIFT;
    CAN_TX_DW1 = zynq_can_pack_data(&message->data[0]);
    CAN_TX_DW2 = zynq_can_pack_data(&message->data[4]);
    return RT_EOK;
}

static rt_ssize_t zynq_can_receive(struct rt_can_device *can, void *buffer,
                                   rt_uint32_t fifo)
{
    struct rt_can_msg *message = buffer;
    rt_uint32_t id;
    rt_uint32_t dlc;

    RT_UNUSED(can);
    RT_UNUSED(fifo);
    if (!(CAN_ISR & CAN_INT_RX_NOT_EMPTY))
    {
        return -1;
    }
    id = CAN_RX_ID;
    dlc = CAN_RX_DLC;
    if (id & CAN_ID_IDE)
    {
        message->ide = RT_CAN_EXTID;
        message->id = ((id >> CAN_ID_STD_SHIFT) & 0x7FFU) << 18;
        message->id |= (id >> CAN_ID_EXT_SHIFT) & 0x3FFFFU;
        message->rtr = (id & CAN_ID_RTR) ? RT_CAN_RTR : RT_CAN_DTR;
    }
    else
    {
        message->ide = RT_CAN_STDID;
        message->id = (id >> CAN_ID_STD_SHIFT) & 0x7FFU;
        message->rtr = (id & CAN_ID_SRR) ? RT_CAN_RTR : RT_CAN_DTR;
    }
    message->len = (dlc >> CAN_DLC_SHIFT) & 0xFU;
    if (message->len > 8U)
    {
        message->len = 8U;
    }
    message->hdr_index = -1;
    message->rxfifo = 0;
    message->nonblocking = 0;
    zynq_can_unpack_data(CAN_RX_DW1, &message->data[0]);
    zynq_can_unpack_data(CAN_RX_DW2, &message->data[4]);
    CAN_ICR = CAN_INT_RX_NOT_EMPTY;
    return 0;
}

static rt_ssize_t zynq_can_send_nonblocking(struct rt_can_device *can,
                                            const void *buffer)
{
    return zynq_can_send(can, buffer, 0U);
}

static const struct rt_can_ops zynq_can_ops =
{
    .configure = zynq_can_configure,
    .control = zynq_can_control,
    .sendmsg = zynq_can_send,
    .recvmsg = zynq_can_receive,
    .sendmsg_nonblocking = zynq_can_send_nonblocking,
};

static void zynq_can_isr(int vector, void *parameter)
{
    rt_uint32_t pending = CAN_ISR & CAN_IER;
    rt_uint32_t error;

    RT_UNUSED(vector);
    RT_UNUSED(parameter);
    CAN_ICR = pending;

    if (pending & CAN_INT_RX_OVERFLOW)
    {
        rt_hw_can_isr(&zynq_can0, RT_CAN_EVENT_RXOF_IND);
    }
    else if (pending & CAN_INT_RX_NOT_EMPTY)
    {
        rt_hw_can_isr(&zynq_can0, RT_CAN_EVENT_RX_IND);
    }
    if (pending & CAN_INT_TX_OK)
    {
        rt_hw_can_isr(&zynq_can0, RT_CAN_EVENT_TX_DONE);
    }
    if (pending & CAN_INT_ERROR_MASK)
    {
        error = CAN_ESR;
        CAN_ESR = error;
        zynq_can0.status.lasterrtype = error;
        if (error & (1U << 4))
        {
            zynq_can0.status.ackerrcnt++;
        }
        if (error & (1U << 3))
        {
            zynq_can0.status.biterrcnt++;
        }
        if (error & (1U << 2))
        {
            zynq_can0.status.bitpaderrcnt++;
        }
        if (error & (1U << 1))
        {
            zynq_can0.status.formaterrcnt++;
        }
        if (error & (1U << 0))
        {
            zynq_can0.status.crcerrcnt++;
        }
    }
}

static int zynq_can_register(void)
{
    struct can_configure config = CANDEFAULTCONFIG;

    config.baud_rate = CAN500kBaud;
    config.sndboxnumber = 1U;
    config.mode = RT_CAN_MODE_NORMAL;
    config.ticks = RT_TICK_PER_SECOND;
#ifdef RT_CAN_USING_HDR
    config.maxhdr = 0U;
#endif
    zynq_can0.config = config;
    CAN_IER = 0U;
    CAN_SRR = CAN_SRR_RESET;
    CAN_ICR = CAN_INT_ALL;
    rt_hw_interrupt_install(ZYNQ_CAN0_IRQ, zynq_can_isr, RT_NULL, "can0");
    rt_hw_interrupt_mask(ZYNQ_CAN0_IRQ);
    return rt_hw_can_register(&zynq_can0, "can0", &zynq_can_ops, RT_NULL);
}
INIT_BOARD_EXPORT(zynq_can_register);

#endif /* BSP_USING_CAN0 */
