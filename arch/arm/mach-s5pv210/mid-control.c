/* linux/arch/arm/mach-s5pv210/mid-control.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <plat/gpio-cfg.h>
#include <mach/hardware.h>

#define DEVICE_NAME                 "mid-control"

#define GPIO_USB_OTG_SWITCH         S5PV210_GPH1(1)

#define GPIO_RO_ATTR(name, gpio_number)                                             \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr,       \
        char *buf)                                                                  \
{                                                                                   \
    return sprintf(buf, "%d\n", gpio_get_value(gpio_number));                       \
}                                                                                   \
static DEVICE_ATTR(name, S_IRUGO | S_IWUSR, name##_show, NULL);

#define GPIO_RW_ATTR(name, gpio_number)                                             \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr,       \
        char *buf)                                                                  \
{                                                                                   \
    return sprintf(buf, "%d\n", gpio_get_value(gpio_number));                       \
}                                                                                   \
static ssize_t name##_store(struct device *dev, struct device_attribute *attr,      \
        const char *buf, size_t size)                                               \
{                                                                                   \
    int value;                                                                      \
    if (sscanf(buf, "%d", &value) == 1) {                                           \
        gpio_direction_output(gpio_number, !!value);                                \
        return size;                                                                \
    }                                                                               \
    return -1;                                                                      \
}                                                                                   \
static DEVICE_ATTR(name, S_IRUGO | S_IWUSR, name##_show, name##_store);

GPIO_RW_ATTR(otg_switch,            GPIO_USB_OTG_SWITCH)

#define GPIO_RW_DECLARE(decl_name)                                                  \
struct { int num; char *name; } decl_name[] = {                                     \
    {GPIO_USB_OTG_SWITCH,           "usb-otg-switch"}                               \
};

static struct device_attribute *mid_control_attrs[] = {
    &dev_attr_otg_switch,           // gpio [output]
};

static int mid_control_attr_create(struct device *dev) {
    int i, rc;

    for (i = 0; i < ARRAY_SIZE(mid_control_attrs); i++)
        if ((rc = device_create_file(dev, mid_control_attrs[i])))
            goto attrs_failed;

    goto succeed;

attrs_failed:
    while (i--)
        device_remove_file(dev, mid_control_attrs[i]);

succeed:
    return rc;
}

static void mid_control_attr_remove(struct device *dev) {
    int i;

    for (i = ARRAY_SIZE(mid_control_attrs) - 1; i >= 0; i--)
        device_remove_file(dev, mid_control_attrs[i]);
}

static int mid_control_gpio_request(void) {
    int i;

    GPIO_RW_DECLARE(gpio_rw_list);

    for (i = 0; i < ARRAY_SIZE(gpio_rw_list); i++)
        if (gpio_request(gpio_rw_list[i].num, gpio_rw_list[i].name) != 0)
            printk(KERN_WARNING "%s: Coudn't request gpio 0x%02x!\n", __func__, 
                gpio_rw_list[i].num);

    return 0;
}

static void mid_control_gpio_free(void) {
    int i;

    GPIO_RW_DECLARE(gpio_rw_list);

    for (i = ARRAY_SIZE(gpio_rw_list) - 1; i >= 0; i--)
        gpio_free(gpio_rw_list[i].num);
}

static int __devinit mid_control_probe(struct platform_device *pdev) {
    int ret;

    if ((ret = mid_control_gpio_request())) {
        printk(KERN_ERR "%s: Coudn't request all gpios!\n", __func__);
        return ret;
    }

    if ((ret = mid_control_attr_create(&pdev->dev))) {
        mid_control_gpio_free();
        printk(KERN_ERR "%s: Coudn't create attributes!\n", __func__);
        return ret;
    }

    return 0;
}

static int __devexit mid_control_remove(struct platform_device *pdev) {
    mid_control_attr_remove(&pdev->dev);
    mid_control_gpio_free();

    return 0;
}

static struct platform_driver mid_control_driver = {
    .probe          = mid_control_probe,
    .remove         = __devexit_p(mid_control_remove),
    .driver = {
        .name       = DEVICE_NAME,
        .owner      = THIS_MODULE
    }
};

static int __init mid_control_init(void) {
    int ret;

    if ((ret = platform_driver_register(&mid_control_driver))) {
        printk(KERN_ERR "Error registering """ DEVICE_NAME """ platform driver!\n");
        return ret;
    }

    return 0;
}

static void __exit mid_control_exit(void) {
    platform_driver_unregister(&mid_control_driver);
}

module_init(mid_control_init);
module_exit(mid_control_exit);

MODULE_AUTHOR("namko");
MODULE_DESCRIPTION("MID-control utility code");
MODULE_LICENSE("GPL");

