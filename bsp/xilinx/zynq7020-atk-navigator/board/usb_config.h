/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ZYNQ7020_USB_CONFIG_H__
#define __ZYNQ7020_USB_CONFIG_H__

/* Zynq-7000 PS USB is a ChipIdea controller with EHCI caps at +0x100. */
#define CONFIG_USB_EHCI_HCCR_OFFSET       0x100U
#define CONFIG_USB_DCACHE_ENABLE
#define CONFIG_USB_EHCI_DESC_DCACHE_ENABLE
#define CONFIG_USB_EHCI_CONFIGFLAG

#include <cherryusb_config_template.h>

#endif /* __ZYNQ7020_USB_CONFIG_H__ */
