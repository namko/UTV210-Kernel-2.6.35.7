/*
 * Driver for keys on UTV210-based tablets.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/pm.h>
#include <linux/gpio.h>

#define DEVICE_NAME                     "s3c-button"
#define POLL_INTERVAL_IN_MS             80
#define BUTTON_COUNT                    6

static const unsigned int nGPIOs[BUTTON_COUNT] = {
    S5PV210_GPH0(4),
    S5PV210_GPH0(1),
    S5PV210_GPH3(4),
    S5PV210_GPH3(5),
    S5PV210_GPH1(3),
    S5PV210_GPH1(4)
};

static const int nButtonCodes[BUTTON_COUNT] = {
    KEY_BACK,
    KEY_END,
    KEY_MENU,
    KEY_HOME,
    KEY_VOLUMEUP,
    KEY_VOLUMEDOWN
};

static bool bButtonWasPressed[BUTTON_COUNT] = {false};

static struct timer_list timer;
static struct input_dev *input_dev;

static void s3c_button_timer_handler(unsigned long arg) {
    int i;

    for (i = 0; i < BUTTON_COUNT; i++) {
        bool pressed = !gpio_get_value(nGPIOs[i]);

        if (pressed != bButtonWasPressed[i])
            input_report_key(input_dev, nButtonCodes[i], pressed);

        bButtonWasPressed[i] = pressed;
    }

    mod_timer(&timer, jiffies + msecs_to_jiffies(POLL_INTERVAL_IN_MS));
}

static int s3c_button_probe(struct platform_device *pdev) {
    int i, j, ret;
    printk("== %s ==\n", __func__);

    for (i = 0; i < BUTTON_COUNT; i++)
        if ((ret = gpio_request(nGPIOs[i], DEVICE_NAME))) {
            printk("*** ERROR coudn't request GPIO 0x%x for %s! ***",  
                nGPIOs[i], DEVICE_NAME);

            for (j = i - 1; j >= 0; j--)
                gpio_free(nGPIOs[j]);

            return ret;            
        }

    for (i = 0; i < BUTTON_COUNT; i++) {
        if ((ret = gpio_direction_input(nGPIOs[i]))) {
            printk("*** ERROR coudn't set direction of GPIO 0x%x for %s! ***", 
                nGPIOs[i], DEVICE_NAME);
            goto err_gpio_config;
        }

        if ((ret = s3c_gpio_setpull(nGPIOs[i], S3C_GPIO_PULL_UP))) {
            printk("*** ERROR coudn't set pull of GPIO 0x%x for %s! ***", 
                nGPIOs[i], DEVICE_NAME);
            goto err_gpio_config;
        }
    }

    if (!(input_dev = input_allocate_device())) {
        printk("*** ERROR coudn't allocate input device for " DEVICE_NAME "! ***");
        ret = -ENOMEM;
        goto err_input_allocate_device;
    }

	input_dev->name = DEVICE_NAME;
	input_dev->phys = DEVICE_NAME "/input0";
	input_dev->dev.parent = &pdev->dev;

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

    for (i = 0; i < BUTTON_COUNT; i++)
        set_bit(nButtonCodes[i], input_dev->keybit);

    set_bit(EV_KEY, input_dev->evbit);

    if ((ret = input_register_device(input_dev))) {
        printk("*** ERROR coudn't register input device for " DEVICE_NAME "! ***");
        goto err_input_register_device;
    }

    init_timer(&timer);
    timer.function = s3c_button_timer_handler;
    timer.expires = jiffies + msecs_to_jiffies(POLL_INTERVAL_IN_MS);
    add_timer(&timer);

    printk("== %s initialized! ==\n", DEVICE_NAME);
    return 0;

err_input_register_device:
    input_free_device(input_dev);

err_input_allocate_device:
err_gpio_config:
    for (i = 0; i < BUTTON_COUNT; i++)
        gpio_free(nGPIOs[i]);

    return ret;
}

static int __devexit s3c_button_remove(struct platform_device *pdev) {
    int i;
    printk("== %s ==\n", __func__);

    del_timer(&timer);
    input_unregister_device(input_dev);

    for (i = 0; i < BUTTON_COUNT; i++)
        gpio_free(nGPIOs[i]);

    return 0;
}

#ifdef CONFIG_PM
static int s3c_button_suspend(struct platform_device *dev, pm_message_t state) {
    printk("== %s ==\n", __func__);
    return 0;
}

static int s3c_button_resume(struct platform_device *dev) {
    printk("== %s ==\n", __func__);
    return 0;
}
#else
#define s3c_keypad_suspend NULL
#define s3c_keypad_resume  NULL
#endif /* CONFIG_PM */

static struct platform_driver s3c_button_device_driver = {
	.probe		= s3c_button_probe,
	.remove		= s3c_button_remove,
	.suspend	= s3c_button_suspend,
	.resume		= s3c_button_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DEVICE_NAME,
	},
};

static struct platform_device s3c_device_button = {
    .name = DEVICE_NAME,
    .id = -1,
};

static int __init s3c_button_init(void) {
	int ret;
    printk("== %s ==\n", __func__);

    memset(bButtonWasPressed, 0, sizeof(bButtonWasPressed));

    if ((ret = platform_device_register(&s3c_device_button))) {
        printk("*** ERROR [%d] coudn't register %s device! ***\n", ret, DEVICE_NAME);
        return ret;
    }

    if ((ret = platform_driver_register(&s3c_button_device_driver))) {
        printk("*** ERROR [%d] coudn't register %s driver! ***\n", ret, DEVICE_NAME);
        platform_device_unregister(&s3c_device_button);
        return ret;
    }

	return 0;
}

static void __exit s3c_button_exit(void) {
    printk("== %s ==\n", __func__);
    platform_driver_register(&s3c_button_device_driver);
    platform_device_register(&s3c_device_button);
}

module_init(s3c_button_init);
module_exit(s3c_button_exit);

MODULE_DESCRIPTION("s3c-button interface for UTV210");
MODULE_AUTHOR("namko");
MODULE_LICENSE("GPL");

