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

#define DRIVER_NAME             "utv210-battery"
#define DRIVER_ADC_CHANNEL      0
#define POLL_INTERVAL_IN_SECS   3
#define NUMBER_OF_SAMPLES       10

// namko: Experiments reveal that maximum value of ADCIN0 is about 3300
// for full battery and reduces by approximately 6 for every percent.
static const int ADC_MAX_MV = 3300, ADC_MIN_MV = 2600;

// mg3100: The battery is made of 2 separate batteries wired in series.
// The standard voltage for 1 cell is 3.6(empty) to 4.2 or 4.3 (Li-Ion or Li-Poly)
// that makes 7.2(empty) and 8.4 or 8.6(full) for 2 cells.
static const int BATTERY_MAX_MV = 8500, BATTERY_MIN_MV = 7200;

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
    CHARGER_USB,
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
    u32 charging_source;        /* 0: no cable, 1:usb, 2:AC */
    u32 charging_enabled;       /* 0: Disable, 1: Enable */
    u32 batt_health;            /* Battery Health (Authority) */
    u32 batt_is_full;           /* 0 : Not full 1: Full */
};

/* lock to protect the battery info */
static DEFINE_MUTEX(work_lock);
static struct battery_info utv210_bat_info;

static int utv210_get_bat_temp(struct power_supply *bat_ps) {
    // namko: HACK: Fixed, this is.
    int temp = 200;
    return temp;
}

static u32 utv210_get_bat_health(void) {
    // namko: HACK: Fixed, this is, too.
    return utv210_bat_info.batt_health;
}

// "utv210_bat_info.vol_adc" must be set before calling this function.
static int utv210_get_bat_level(struct power_supply *bat_ps) {
    // mg3100: Battery is connected to ADCIN0. Max count for ADC is reached
    // at 3.3 volts. Battery is connected using divider resistors to lower
    // the voltage to 2.350 volts for an emtpy battery to 2.730 volts for
    // a completely charged battery.
    int value = (utv210_bat_info.batt_vol_adc - ADC_MIN_MV) * 
            100 / (ADC_MAX_MV - ADC_MIN_MV);

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    return value;
}

// "utv210_bat_info.vol_adc" must be set before calling this function.
static int utv210_get_bat_vol(struct power_supply *bat_ps) {
    return BATTERY_MIN_MV + (BATTERY_MAX_MV - BATTERY_MIN_MV) * 
            (ADC_MAX_MV - ADC_MIN_MV) / (utv210_bat_info.batt_vol_adc - ADC_MIN_MV);
}

static int utv210_bat_get_charging_status(void) {
    charger_type_t charger = CHARGER_BATTERY; 
    int ret = 0;
        
    charger = utv210_bat_info.charging_source;
        
    switch (charger) {
    case CHARGER_BATTERY:
        ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
        break;
    case CHARGER_USB:
    case CHARGER_AC:
        if (utv210_bat_info.level == 100 
            && utv210_bat_info.batt_is_full) {
            ret = POWER_SUPPLY_STATUS_FULL;
        } else {
            ret = POWER_SUPPLY_STATUS_CHARGING;
        }
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
        else if (bat_ps->type == POWER_SUPPLY_TYPE_USB)
            val->intval = (charger == CHARGER_USB ? 1 : 0);
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

    for (i = 0; i < ARRAY_SIZE(utv210_battery_attrs); i++) {
        rc = device_create_file(dev, &utv210_battery_attrs[i]);
        if (rc)
        goto attrs_failed;
    }
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
    POWER_SUPPLY_PROP_CAPACITY,
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
        .name = "usb",
        .type = POWER_SUPPLY_TYPE_USB,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = utv210_power_properties,
        .num_properties = ARRAY_SIZE(utv210_power_properties),
        .get_property = utv210_power_get_property,
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
    charger_type_t source = CHARGER_BATTERY;

    dev_dbg(dev, "%s\n", __func__);

    if(!utv210_battery_initial)
        return -EPERM;

    switch(status) {
    case CHARGER_BATTERY:
        dev_dbg(dev, "%s : cable NOT PRESENT\n", __func__);
        utv210_bat_info.charging_source = CHARGER_BATTERY;
        break;
    case CHARGER_USB:
        dev_dbg(dev, "%s : cable USB\n", __func__);
        utv210_bat_info.charging_source = CHARGER_USB;
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
    source = utv210_bat_info.charging_source;

    if (source == CHARGER_USB || source == CHARGER_AC) {
        wake_lock(&vbus_wake_lock);
    } else {
        /* give userspace some time to see the uevent and update
         * LED state or whatnot...
         */
        wake_lock_timeout(&vbus_wake_lock, HZ / 2);
    }

    /* if the power source changes, all power supplies may change state */
    power_supply_changed(&utv210_power_supplies[CHARGER_BATTERY]);
    /*
    power_supply_changed(&utv210_power_supplies[CHARGER_USB]);
    power_supply_changed(&utv210_power_supplies[CHARGER_AC]);
    */
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
    utv210_bat_info.level = utv210_get_bat_level(bat_ps);
    utv210_bat_info.batt_vol = utv210_get_bat_vol(bat_ps);

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
    charger_type_t status = 0;

    if (flag == 0)  // Battery
        status = CHARGER_BATTERY;
    else            // USB
        status = CHARGER_USB;
    s3c_cable_status_update(status);
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

static struct delayed_work battery_watcher_work;

static void battery_watcher(struct work_struct *work) {
    int i;
    unsigned int sum, min, max;
    static unsigned int readings[NUMBER_OF_SAMPLES];

    dev_dbg(dev, "*** %s: ***\n", __func__);

    // Read ADC.
    for (i = 0, sum = 0; i < NUMBER_OF_SAMPLES; i++) {
        readings[i] = s3c_adc_get_adc_data(DRIVER_ADC_CHANNEL);
        sum += readings[i];

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
    
    // Update battery.
    utv210_bat_info.batt_vol_adc = (sum - min - max) / (NUMBER_OF_SAMPLES - 2);
    utv210_bat_status_update(&utv210_power_supplies[CHARGER_BATTERY]);
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
    utv210_bat_info.batt_temp_adc_cal = 0;
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
    wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
    return platform_driver_register(&utv210_bat_driver);
}

static void __exit utv210_bat_exit(void) {
    printk("*** %s ***\n", __func__);
    platform_driver_unregister(&utv210_bat_driver);
}

module_init(utv210_bat_init);
module_exit(utv210_bat_exit);

MODULE_AUTHOR("namko");
MODULE_DESCRIPTION("Battery driver for UTV210-based tablets");
MODULE_LICENSE("GPL");

