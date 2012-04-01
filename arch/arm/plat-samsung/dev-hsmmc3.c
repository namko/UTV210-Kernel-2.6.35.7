/* linux/arch/arm/plat-samsung/dev-hsmmc3.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Based on dev-hsmmc.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mmc/host.h>

#include <mach/map.h>
#include <plat/sdhci.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#if defined(CONFIG_BROADCOM_WIFI_BT)
#include <linux/delay.h>
#include <linux/skbuff.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h> 
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <linux/wlan_plat.h>

#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <plat/s5pv210.h>
#include <plat/regs-rtc.h>
#include <plat/map-base.h>
#include <linux/mmc/card.h>
#include <linux/io.h>

#include <linux/regulator/consumer.h>
static  struct regulator *bt_reg;
static  struct regulator *wifi_reg;
extern void set_wifi_power(unsigned int val, struct regulator ** regulators);
extern void set_bt_power(unsigned int val, struct regulator ** regulators);
#endif

#define S3C_SZ_HSMMC	(0x1000)

static struct resource s3c_hsmmc3_resource[] = {
	[0] = {
		.start = S3C_PA_HSMMC3,
		.end   = S3C_PA_HSMMC3 + S3C_SZ_HSMMC - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_MMC3,
		.end   = IRQ_MMC3,
		.flags = IORESOURCE_IRQ,
	}
};
#if defined(CONFIG_BROADCOM_WIFI_BT)
static int bcm4329_wifi_cd = 0;
static void *wifi_status_cb_devid;
static void (*wifi_status_cb)(int card_present, void *dev_id);
int bcm4329_wifi_set_carddetect(int val)
{
	printk("%s: %d\n", __func__, val);
	bcm4329_wifi_cd = val;
	if (wifi_status_cb) {
		wifi_status_cb(val, wifi_status_cb_devid);
	} else
		printk("%s: Nobody to notify\n", __func__);
	return 0;
}
static int bcm4329_wifi_power_state;

int bcm4329_wifi_power(int on)
{
	int ret,err;
	printk(KERN_INFO"wifi power switch %d\n", on);
	if (on){
		set_wifi_power(1,&wifi_reg);
	}else{
		set_wifi_power(0,&wifi_reg);
	}
	bcm4329_wifi_power_state = on;
	return 0;
}

static int bcm4329_wifi_reset_state;
int bcm4329_wifi_reset(int on)
{
	printk("%s: do nothing\n", __func__);
	bcm4329_wifi_reset_state = on;
	return 0;
}

static void *bcm4329_wifi_mem_prealloc(int section, unsigned long size);
int bcm4329_wifi_status_register(
		void (*cb)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EBUSY;
	wifi_status_cb = cb;
	wifi_status_cb_devid = dev_id;
	return 0;
}
#endif

static u64 s3c_device_hsmmc3_dmamask = 0xffffffffUL;

struct s3c_sdhci_platdata s3c_hsmmc3_def_platdata = {
	.max_width	= 4,
	.host_caps	= (MMC_CAP_4_BIT_DATA |
#if defined(CONFIG_MMC_CH3_CLOCK_GATING)
		MMC_CAP_CLOCK_GATING |
#endif
			   MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED),
};

struct platform_device s3c_device_hsmmc3 = {
	.name		= "s3c-sdhci",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(s3c_hsmmc3_resource),
	.resource	= s3c_hsmmc3_resource,
	.dev		= {
		.dma_mask		= &s3c_device_hsmmc3_dmamask,
		.coherent_dma_mask	= 0xffffffffUL,
		.platform_data		= &s3c_hsmmc3_def_platdata,
	},
};

#if defined(CONFIG_BROADCOM_WIFI_BT)
static struct wifi_platform_data bcm4329_wifi_control = {
	.set_power      = bcm4329_wifi_power,
	.set_reset      = bcm4329_wifi_reset,
	.set_carddetect = bcm4329_wifi_set_carddetect,
	.mem_prealloc	= bcm4329_wifi_mem_prealloc,
};

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *bcm4329_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}
int __init bcm4329_init_wifi_mem(void)
{
	int i;
	for(i=0;( i < WLAN_SKB_BUF_NUM );i++) {
		if (i < (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096);
		else
			wlan_static_skb[i] = dev_alloc_skb(8192);
	}
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
}
#endif
void s3c_sdhci3_set_platdata(struct s3c_sdhci_platdata *pd)
{
	struct s3c_sdhci_platdata *set = &s3c_hsmmc3_def_platdata;

	if (pd->max_width)
		set->max_width = pd->max_width;
	if (pd->host_caps)
		set->host_caps |= pd->host_caps;
	if (pd->cfg_gpio)
		set->cfg_gpio = pd->cfg_gpio;
	if (pd->cfg_card)
		set->cfg_card = pd->cfg_card;
	if (pd->cfg_ext_cd)
		set->cfg_ext_cd = pd->cfg_ext_cd;
	if (pd->ext_cd)
		set->ext_cd = pd->ext_cd;
	if (pd->get_ext_cd)
		set->get_ext_cd = pd->get_ext_cd;
	if (pd->cfg_wp)
		set->cfg_wp = pd->cfg_wp;
	if (pd->get_ro)
		set->get_ro = pd->get_ro;
	if (pd->detect_ext_cd)
		set->detect_ext_cd = pd->detect_ext_cd;
        if (pd->built_in)
                set->built_in = pd->built_in;

#if defined(CONFIG_BROADCOM_WIFI_BT)
	   set->register_status_notify	= bcm4329_wifi_status_register;  //gaofeng
#endif
}


#if defined(CONFIG_BROADCOM_WIFI_BT)
static struct resource bcm4329_wifi_resources[] = {
	[0] = {
		.name		= "bcm4329_wlan_irq",
		.start		= IRQ_EINT(21),
		.end		= IRQ_EINT(21),
		.flags          = IORESOURCE_IRQ ,
	},
};

static struct platform_device bcm4329_wifi_device = {
        .name           = "bcm4329_wlan",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(bcm4329_wifi_resources),
        .resource       = bcm4329_wifi_resources,
        .dev            = {
                .platform_data = &bcm4329_wifi_control,
        },
};

/* device_pm.c has init off for wifi/bt, so undef the macro */
#undef INIT_OFF
static int __init bcm4329_wifi_init(void)
{
	int ret,err;
	unsigned int gpio;

	printk(KERN_INFO"wifi and bt power switch init to off\n");


 	/* gph26 regulator get*/
	wifi_reg = regulator_get(NULL, "vdd_wifi");
	if (IS_ERR(wifi_reg)) {
		printk(KERN_ERR "failed to get resource %s\n", "vdd_wifi");
	}else{
#ifdef INIT_OFF	
		regulator_enable(wifi_reg);
#endif
	}
	udelay(50);
	
	bt_reg = regulator_get(NULL, "vdd_bt");
	if (IS_ERR(bt_reg)) {
		printk(KERN_ERR "failed to get resource %s\n", "vdd_bt");
	}else{
#ifdef INIT_OFF
		regulator_enable(bt_reg);
#endif
	}
	udelay(50);
#ifdef INIT_OFF	
	set_bt_power(0,&bt_reg);
	set_wifi_power(0,&wifi_reg);
#endif	
  	bcm4329_init_wifi_mem();
	ret = platform_device_register(&bcm4329_wifi_device);
      return ret;
}

static void __exit bcm4329_wifi_exit(void)
{
	platform_device_unregister(&bcm4329_wifi_device);
	
 	/* gph26 regulator put*/
	if (!IS_ERR(wifi_reg)) {
		regulator_put(wifi_reg);
	}

	if (!IS_ERR(bt_reg)) {
		regulator_put(bt_reg);
	}
}
module_init(bcm4329_wifi_init);
module_exit(bcm4329_wifi_exit);
#endif
