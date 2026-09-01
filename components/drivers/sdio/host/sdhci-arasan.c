/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2026-09-01     hhhhemeng     first version
 */

#include "../dev_sdhci_dm.h"

static unsigned int sdhci_arasan_get_max_clock(struct rt_sdhci_host *host)
{
    struct rt_sdhci_pltfm_host *pltfm_host = rt_sdhci_priv(host);

    return pltfm_host->clock;
}

static const struct rt_sdhci_ops sdhci_arasan_ops =
{
    .set_clock         = rt_sdhci_set_clock,
    .set_bus_width     = rt_sdhci_set_bus_width,
    .reset             = rt_sdhci_reset,
    .set_uhs_signaling = rt_sdhci_set_uhs,
    .get_max_clock     = sdhci_arasan_get_max_clock,
};

static const struct rt_sdhci_pltfm_data sdhci_arasan_data =
{
    .ops = &sdhci_arasan_ops,
    .quirks = RT_SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
    .quirks2 = RT_SDHCI_QUIRK2_BROKEN_64_BIT_DMA,
};

static rt_err_t sdhci_arasan_probe(struct rt_platform_device *pdev)
{
    rt_err_t err;
    struct rt_sdhci_host *host;
    struct rt_sdhci_pltfm_host *pltfm_host;

    host = rt_sdhci_pltfm_init(pdev, &sdhci_arasan_data, 0);
    if (!host)
    {
        return -RT_ENOMEM;
    }

    rt_sdhci_get_property(pdev);
    pltfm_host = rt_sdhci_priv(host);
    if (!pltfm_host->clock)
    {
        rt_kprintf("%s: missing clock-frequency\n", host->hw_name);
        rt_sdhci_pltfm_free(pdev);
        return -RT_EINVAL;
    }

    err = rt_sdhci_set_and_add_host(host);
    if (err)
    {
        rt_sdhci_pltfm_free(pdev);
    }

    return err;
}

static const struct rt_ofw_node_id sdhci_arasan_ofw_ids[] =
{
    { .compatible = "arasan,sdhci-8.9a" },
    { /* sentinel */ }
};

static struct rt_platform_driver sdhci_arasan_driver =
{
    .name = "sdhci-arasan",
    .ids = sdhci_arasan_ofw_ids,
    .probe = sdhci_arasan_probe,
    .remove = rt_sdhci_pltfm_remove,
};
RT_PLATFORM_DRIVER_EXPORT(sdhci_arasan_driver);
