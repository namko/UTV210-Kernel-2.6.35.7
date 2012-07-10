/*
 * linux/drivers/power/utv210_battery.c
 *
 * Battery measurement code for UTV210-based tablets.
 * (based on s3c_fake_battery.c)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <mach/utv210-cfg.h>

#define DRIVER_NAME             "utv210-battery"
#define DRIVER_ADC_CHANNEL      0
#define POLL_INTERVAL_IN_SECS   10
#define NUMBER_OF_SAMPLES       16

// namko: To prevent rapidly-varying/spurious battery percentages the
// ADC's value should be integrated over an interval of time. Currently
// this is set to a minute and appears to provide reasonable data.
#define NUMBER_OF_VALUES_AVG    25

// NOTE battery-full is valid only when AC is connected!
static const int nGPIO_AC_Connected = S5PV210_GPH3(6);
static const int nGPIO_Battery_Full = S5PV210_GPH0(2);

static struct wake_lock vbus_wake_lock;
static struct device *dev;
static int utv210_battery_initial;

/* Prototypes */
extern int s3c_adc_get_adc_data(int channel);

static ssize_t utv210_bat_show_property(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf);

static ssize_t utv210_bat_store(struct device *dev, 
                                struct device_attribute *attr,
                                const char *buf, size_t count);

static char *status_text[] = {
    [POWER_SUPPLY_STATUS_UNKNOWN] =         "Unknown",
    [POWER_SUPPLY_STATUS_CHARGING] =        "Charging",
    [POWER_SUPPLY_STATUS_DISCHARGING] =     "Discharging",
    [POWER_SUPPLY_STATUS_NOT_CHARGING] =    "Not Charging",
    [POWER_SUPPLY_STATUS_FULL] =            "Full",
};

typedef enum {
    CHARGER_BATTERY = 0,
    CHARGER_AC,
    CHARGER_DISCHARGE
} charger_type_t;

struct battery_info {
    u32 batt_id;                /* Battery ID from ADC */
    u32 batt_vol;               /* Battery voltage from ADC */
    u32 batt_vol_adc;           /* Battery ADC value */
    u32 batt_vol_adc_cal;       /* Battery ADC value (calibrated)*/
    u32 batt_temp;              /* Battery Temperature (C) from ADC */
    u32 batt_temp_adc;          /* Battery Temperature ADC value */
    u32 batt_temp_adc_cal;      /* Battery Temperature ADC value (calibrated) */
    u32 batt_current;           /* Battery current from ADC */
    u32 level;                  /* formula */
    u32 charging_source;        /* 0: no cable, 1:AC */
    u32 charging_enabled;       /* 0: Disable, 1: Enable */
    u32 batt_health;            /* Battery Health (Authority) */
    u32 batt_is_full;           /* 0 : Not full 1: Full */
};

/* lock to protect the battery info */
static DEFINE_MUTEX(work_lock);
static struct battery_info utv210_bat_info;

static int utv210_get_bat_temp(struct power_supply *bat_ps) {
    return utv210_bat_info.batt_temp_adc + 
        utv210_bat_info.batt_temp_adc_cal;
}

static u32 utv210_get_bat_health(void) {
    return utv210_bat_info.batt_health;
}

struct capacity_entry {
    int mv;
    int p;
};

static const struct capacity_entry captable_7024[] = {
    {6700, 0}, {6900, 2}, {7120, 6}, {7260, 16},
    {7320, 28}, {7420, 44}, {7500, 53}, {7580, 63},
    {7700, 72}, {7780, 80}, {7880, 92}, {7980, 100}
};

static const struct capacity_entry captable_703[] = {
    {6800, 0}, {7150, 4}, {7300, 10}, {7400, 23},
    {7530, 38}, {7620, 50}, {7720, 65}, {7800, 78},
    {7870, 86}, {7920, 92}, {8000, 96}, {8150, 100}
};

static const struct capacity_entry captable_712[] = {
    {6800, 0}, {6900, 2}, {7120, 6}, {7220, 11},
    {7320, 20}, {7350, 28}, {7450, 46}, {7500, 56},
    {7580, 65}, {7700, 74}, {7800, 82}, {7900, 90},
    {8000, 100}
};

static const struct capacity_entry captable_generic[] = {
    {6700, 0}, {6900, 2}, {7120, 6}, {7260, 16},
    {7320, 28}, {7420, 44}, {7500, 53}, {7580, 63},
    {7700, 72}, {7780, 80}, {7880, 92}, {7980, 100}
};

// "utv210_bat_info.batt_vol" must be set before calling this function.
static int utv210_get_bat_level(struct power_supply *bat_ps) {
    int i, captable_start, level;
    const struct capacity_entry *captable;

    // Read current battery voltage.
    const int v = utv210_bat_info.batt_vol;

    // Decide on the battery capacity table.
    if (!strcmp(g_Model, "7024"))
        captable = captable_7024;
    else if (!strcmp(g_Model, "703"))
        captable = captable_703;
    else if (!strcmp(g_Model, "712"))
        captable = captable_712;
    else
        captable = captable_generic;

    // Find the capacity table size.
    for (captable_start = 0; captable[captable_start + 1].p < 100; captable_start++)
        ;

    // Find two suitable levels for interpolation.
    for (i = captable_start; v < captable[i].mv && i > 0; i--)
        ;

    // Interpolate.
    level = (captable[i + 1].p - captable[i].p) * (v - captable[i].mv) / 
        (captable[i + 1].mv - captable[i].mv) + captable[i].p;

    if (level < 0)
        level = 0;

    if (level > 100)
        level = 100;

    return level;
}

// "utv210_bat_info.batt_vol_adc" must be set before calling this function.
static int utv210_get_bat_vol(struct power_supply *bat_ps) {
    int i, level;

    // Get raw adc value, calibrated using the calibration parameter.
    const int adc = utv210_bat_info.batt_vol_adc + 
        utv210_bat_info.batt_vol_adc_cal;

    // The ADC calibration table.
    const struct { int adc; int mv; } caltable[] = {
        {2874, 6800}, {2890, 7042}, {2900, 7133},
        {2930, 7281}, {2945, 7314}, {2985, 7375},
        {3000, 7451}, {3010, 7549}, {3015, 7580},
        {3040, 7647}, {3075, 7708}, {3100, 7740},
        {3215, 7930}, {3230, 7981}, {3264, 8150}};

    // Find two suitable levels for interpolation.
    for (i = 13; adc < caltable[i].adc && i > 0; i--)
        ;

    // Interpolate.
    level = (caltable[i + 1].mv - caltable[i].mv) * (adc - caltable[i].adc) / 
        (caltable[i + 1].adc - caltable[i].adc) + caltable[i].mv;

    if (level < 6600)
        level = 6600;

    if (level > 8500)
        level = 8500;

    return level;

}

static int utv210_bat_get_charging_status(void) {
    charger_type_t charger = CHARGER_BATTERY; 
    int ret = 0;
        
    charger = utv210_bat_info.charging_source;
        
    switch (charger) {
    case CHARGER_BATTERY:
        ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
        break;
    case CHARGER_AC:
        if (utv210_bat_info.batt_is_full)
            ret = POWER_SUPPLY_STATUS_FULL;
        else
            ret = POWER_SUPPLY_STATUS_CHARGING;
        break;
    case CHARGER_DISCHARGE:
        ret = POWER_SUPPLY_STATUS_DISCHARGING;
        break;
    default:
        ret = POWER_SUPPLY_STATUS_UNKNOWN;
    }
    dev_dbg(dev, "%s : %s\n", __func__, status_text[ret]);

    return ret;
}

static int utv210_bat_get_property(struct power_supply *bat_ps, 
                                   enum power_supply_property psp,
                                   union power_supply_propval *val) {
    dev_dbg(bat_ps->dev, "%s : psp = %d\n", __func__, psp);

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        val->intval = utv210_bat_get_charging_status();
        break;
    case POWER_SUPPLY_PROP_HEALTH:
        val->intval = utv210_get_bat_health();
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = 1;
        break;
    case POWER_SUPPLY_PROP_TECHNOLOGY:
        val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        /* battery is always online */
        val->intval = 1;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        val->intval = utv210_bat_info.batt_vol;
        break;
    case POWER_SUPPLY_PROP_CAPACITY:
        val->intval = utv210_bat_info.level;
        dev_dbg(dev, "%s : level = %d\n", __func__, 
                val->intval);
        break;
    case POWER_SUPPLY_PROP_TEMP:
        val->intval = utv210_bat_info.batt_temp;
        dev_dbg(bat_ps->dev, "%s : temp = %d\n", __func__, 
                val->intval);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static int utv210_power_get_property(struct power_supply *bat_ps, 
                                     enum power_supply_property psp, 
                                     union power_supply_propval *val) {
    charger_type_t charger;
    
    dev_dbg(bat_ps->dev, "%s : psp = %d\n", __func__, psp);

    charger = utv210_bat_info.charging_source;

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        if (bat_ps->type == POWER_SUPPLY_TYPE_MAINS)
            val->intval = (charger == CHARGER_AC ? 1 : 0);
        else
            val->intval = 0;
        break;
    default:
        return -EINVAL;
    }
    
    return 0;
}

#define BATTERY_ATTR(_name)                                                         \
{                                                                                   \
    .attr = { .name = #_name, .mode = S_IRUGO | S_IWUGO, .owner = THIS_MODULE },    \
    .show = utv210_bat_show_property,                                               \
    .store = utv210_bat_store,                                                      \
}

static struct device_attribute utv210_battery_attrs[] = {
    BATTERY_ATTR(batt_vol),
    BATTERY_ATTR(batt_vol_adc),
    BATTERY_ATTR(batt_vol_adc_cal),
    BATTERY_ATTR(batt_temp),
    BATTERY_ATTR(batt_temp_adc),
    BATTERY_ATTR(batt_temp_adc_cal),
};

enum {
    BATT_VOL = 0,
    BATT_VOL_ADC,
    BATT_VOL_ADC_CAL,
    BATT_TEMP,
    BATT_TEMP_ADC,
    BATT_TEMP_ADC_CAL,
};

static int utv210_bat_create_attrs(struct device * dev) {
    int i, rc;

    for (i = 0; i < ARRAY_SIZE(utv210_battery_attrs); i++)
        if ((rc = device_create_file(dev, &utv210_battery_attrs[i])))
            goto attrs_failed;

    goto succeed;

attrs_failed:
    while (i--)
        device_remove_file(dev, &utv210_battery_attrs[i]);

succeed:
    return rc;
}

static ssize_t utv210_bat_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf) {
    int i = 0;
    const ptrdiff_t off = attr - utv210_battery_attrs;

    switch (off) {
    case BATT_VOL:
        i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                       utv210_bat_info.batt_vol);
    break;
    case BATT_VOL_ADC:
        i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                       utv210_bat_info.batt_vol_adc);
        break;
    case BATT_VOL_ADC_CAL:
        i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                       utv210_bat_info.batt_vol_adc_cal);
        break;
    case BATT_TEMP:
        i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                       utv210_bat_info.batt_temp);
        break;
    case BATT_TEMP_ADC:
        i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                       utv210_bat_info.batt_temp_adc);
        break;    
    case BATT_TEMP_ADC_CAL:
        i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                       utv210_bat_info.batt_temp_adc_cal);
        break;
    default:
        i = -EINVAL;
    }       

    return i;
}

static ssize_t utv210_bat_store(struct device *dev, 
                 struct device_attribute *attr,
                 const char *buf, size_t count) {
    int x = 0;
    int ret = 0;
    const ptrdiff_t off = attr - utv210_battery_attrs;

    switch (off) {
    case BATT_VOL_ADC_CAL:
        if (sscanf(buf, "%d\n", &x) == 1) {
            utv210_bat_info.batt_vol_adc_cal = x;
            ret = count;
        }
        dev_info(dev, "%s : batt_vol_adc_cal = %d\n", __func__, x);
        break;
    case BATT_TEMP_ADC_CAL:
        if (sscanf(buf, "%d\n", &x) == 1) {
            utv210_bat_info.batt_temp_adc_cal = x;
            ret = count;
        }
        dev_info(dev, "%s : batt_temp_adc_cal = %d\n", __func__, x);
        break;
    default:
        ret = -EINVAL;
    }       

    return ret;
}

static enum power_supply_property utv210_battery_properties[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_TECHNOLOGY,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property utv210_power_properties[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
    "battery",
};

static struct power_supply utv210_power_supplies[] = {
    {
        .name = "battery",
        .type = POWER_SUPPLY_TYPE_BATTERY,
        .properties = utv210_battery_properties,
        .num_properties = ARRAY_SIZE(utv210_battery_properties),
        .get_property = utv210_bat_get_property,
    },
    {
        .name = "ac",
        .type = POWER_SUPPLY_TYPE_MAINS,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = utv210_power_properties,
        .num_properties = ARRAY_SIZE(utv210_power_properties),
        .get_property = utv210_power_get_property,
    },
};

static int s3c_cable_status_update(int status) {
    int ret = 0;
    charger_type_t old_source = utv210_bat_info.charging_source;

    dev_dbg(dev, "%s\n", __func__);

    if(!utv210_battery_initial)
        return -EPERM;

    switch(status) {
    case CHARGER_BATTERY:
        dev_dbg(dev, "%s : cable NOT PRESENT\n", __func__);
        utv210_bat_info.charging_source = CHARGER_BATTERY;
        break;
    case CHARGER_AC:
        dev_dbg(dev, "%s : cable AC\n", __func__);
        utv210_bat_info.charging_source = CHARGER_AC;
        break;
    case CHARGER_DISCHARGE:
        dev_dbg(dev, "%s : Discharge\n", __func__);
        utv210_bat_info.charging_source = CHARGER_DISCHARGE;
        break;
    default:
        dev_err(dev, "%s : Nat supported status\n", __func__);
        ret = -EINVAL;
    }

    /* if the power source changes, all power supplies may change state */
    if (old_source != utv210_bat_info.charging_source)
        power_supply_changed(&utv210_power_supplies[CHARGER_AC]);

    power_supply_changed(&utv210_power_supplies[CHARGER_BATTERY]);

    dev_dbg(dev, "%s : call power_supply_changed\n", __func__);
    return ret;
}

static void utv210_bat_status_update(struct power_supply *bat_ps) {
    int old_level, old_temp, old_is_full;
    dev_dbg(dev, "++ %s ++\n", __func__);

    if(!utv210_battery_initial)
        return;

    mutex_lock(&work_lock);

    old_temp = utv210_bat_info.batt_temp;
    old_level = utv210_bat_info.level;
    old_is_full = utv210_bat_info.batt_is_full;

    utv210_bat_info.batt_temp = utv210_get_bat_temp(bat_ps);
    utv210_bat_info.batt_vol = utv210_get_bat_vol(bat_ps);
    utv210_bat_info.level = utv210_get_bat_level(bat_ps);

    if (old_level != utv210_bat_info.level ||
            old_temp != utv210_bat_info.batt_temp ||
            old_is_full != utv210_bat_info.batt_is_full) {
        power_supply_changed(bat_ps);
        dev_dbg(dev, "%s : call power_supply_changed\n", __func__);
    }

    mutex_unlock(&work_lock);
    dev_dbg(dev, "-- %s --\n", __func__);
}

void s3c_cable_check_status(int flag) {
    // Nothing here.
}
EXPORT_SYMBOL(s3c_cable_check_status);

#ifdef CONFIG_PM
static int utv210_bat_suspend(struct platform_device *pdev, pm_message_t state) {
    dev_info(dev, "*** %s ***\n", __func__);
    return 0;
}

static int utv210_bat_resume(struct platform_device *pdev) {
    dev_info(dev, "*** %s ***\n", __func__);
    return 0;
}
#else
#define utv210_bat_suspend NULL
#define utv210_bat_resume NULL
#endif /* CONFIG_PM */

static int last_voltage_ptr, num_active_voltages;
static unsigned int last_voltages[NUMBER_OF_VALUES_AVG];
static struct delayed_work battery_watcher_work;

static void battery_watcher(struct work_struct *work) {
    int i;
    unsigned int sum, min, max;
    bool ac_connected, battery_full;
    static unsigned int readings[NUMBER_OF_SAMPLES];

    dev_dbg(dev, "*** %s: ***\n", __func__);

    // Read GPIOs.
    ac_connected = gpio_get_value(nGPIO_AC_Connected) ? true : false;
    battery_full = gpio_get_value(nGPIO_Battery_Full) ? false : true;

    // Read ADC.
    for (i = 0, sum = 0; i < NUMBER_OF_SAMPLES; i++) {
        readings[i] = s3c_adc_get_adc_data(DRIVER_ADC_CHANNEL);
        sum += readings[i];
        udelay(25);

        if (i) {
            if (readings[i] < min)
                min = readings[i];

            if (readings[i] > max)
                max = readings[i];
        } else {
            min = readings[i];
            max = readings[i];
        }
    }
    
    // Update history.
    last_voltages[last_voltage_ptr++] = (sum - min - max) / (NUMBER_OF_SAMPLES - 2);

    if (num_active_voltages < last_voltage_ptr)
        num_active_voltages = last_voltage_ptr;

    last_voltage_ptr %= NUMBER_OF_VALUES_AVG;

    for (i = 0, sum = 0; i < num_active_voltages; i++)
        sum += last_voltages[i];

    // Update battery.
    utv210_bat_info.batt_vol_adc = sum / num_active_voltages;

    // full-battery GPIO is only valid if AC is connected.
    if (ac_connected && battery_full)
        utv210_bat_info.batt_is_full = 1;
    else
        utv210_bat_info.batt_is_full = 0;

    // Update cable and battery status(es).
    utv210_bat_status_update(&utv210_power_supplies[CHARGER_BATTERY]);
    s3c_cable_status_update(ac_connected ? CHARGER_AC : CHARGER_BATTERY);
    schedule_delayed_work(&battery_watcher_work, HZ * POLL_INTERVAL_IN_SECS);
}

static int __devinit utv210_bat_probe(struct platform_device *pdev) {
    int i;
    int ret = 0;

    dev = &pdev->dev;
    printk("*** %s ***\n", __func__);

    utv210_bat_info.batt_id = 0;
    utv210_bat_info.batt_vol = 0;
    utv210_bat_info.batt_vol_adc = 0;
    utv210_bat_info.batt_vol_adc_cal = 0;
    utv210_bat_info.batt_temp = 0;
    utv210_bat_info.batt_temp_adc = 0;
    utv210_bat_info.batt_temp_adc_cal = 200;
    utv210_bat_info.batt_current = 0;
    utv210_bat_info.level = 0;
    utv210_bat_info.charging_source = CHARGER_BATTERY;
    utv210_bat_info.charging_enabled = 0;
    utv210_bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;

    /* init power supplier framework */
    for (i = 0; i < ARRAY_SIZE(utv210_power_supplies); i++)
        if ((ret = power_supply_register(&pdev->dev, &utv210_power_supplies[i]))) {
            dev_err(dev, "Failed to register power supply %d,%d\n", i, ret);
            goto __end__;
        }

    // namko: Initialize voltage history variables.
    last_voltage_ptr = 0;
    num_active_voltages = 0;
    memset(last_voltages, 0, sizeof(last_voltages));

    /* create detail attributes */
    utv210_bat_create_attrs(utv210_power_supplies[CHARGER_BATTERY].dev);
    utv210_battery_initial = 1;
    utv210_bat_status_update(&utv210_power_supplies[CHARGER_BATTERY]);

    // namko: Schedule battery updates.
    INIT_DELAYED_WORK(&battery_watcher_work, battery_watcher);
    schedule_delayed_work(&battery_watcher_work, 0);

__end__:
    return ret;
}

static int __devexit utv210_bat_remove(struct platform_device *pdev) {
    int i;

    printk("*** %s ***\n", __func__);
    cancel_delayed_work(&battery_watcher_work);

    for (i = 0; i < ARRAY_SIZE(utv210_power_supplies); i++)
        power_supply_unregister(&utv210_power_supplies[i]);
 
    return 0;
}

static struct platform_driver utv210_bat_driver = {
    .driver.name    = DRIVER_NAME,
    .driver.owner   = THIS_MODULE,
    .probe          = utv210_bat_probe,
    .remove         = __devexit_p(utv210_bat_remove),
    .suspend        = utv210_bat_suspend,
    .resume         = utv210_bat_resume,
};

static int __init utv210_bat_init(void) {
    printk("*** %s ***\n", __func__);

    // namko: Request GPIOs for polling battery status and configure them.
    gpio_request(nGPIO_AC_Connected, "ac-connected");
    gpio_request(nGPIO_Battery_Full, "battery-full");
    gpio_direction_input(nGPIO_AC_Connected);
    gpio_direction_input(nGPIO_Battery_Full);
    s3c_gpio_setpull(nGPIO_AC_Connected, S3C_GPIO_PULL_NONE);
    s3c_gpio_setpull(nGPIO_Battery_Full, S3C_GPIO_PULL_NONE);

    // namko: Get a wake-lock (TEMPORARY work-around until solution
    // for freeze-on-suspend is found).
    wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
    wake_lock(&vbus_wake_lock);

    return platform_driver_register(&utv210_bat_driver);
}

static void __exit utv210_bat_exit(void) {
    printk("*** %s ***\n", __func__);
    platform_driver_unregister(&utv210_bat_driver);

    wake_unlock(&vbus_wake_lock);
    wake_lock_destroy(&vbus_wake_lock);

    gpio_free(nGPIO_AC_Connected);
    gpio_free(nGPIO_Battery_Full);
}

module_init(utv210_bat_init);
module_exit(utv210_bat_exit);

MODULE_AUTHOR("namko");
MODULE_DESCRIPTION("Battery driver for UTV210-based tablets");
MODULE_LICENSE("GPL");

