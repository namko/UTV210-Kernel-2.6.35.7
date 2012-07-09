/*
 * mma7660.c
 *
 * Copyright 2009 Freescale Semiconductor Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/hwmon.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#define DEVICE_NAME             "mma7660"
#define SAMPLE_RATE_DEFAULT     16

enum MMA7660_REGISTERS {
    REG_XOUT = 0x00,
    REG_YOUT,
    REG_ZOUT,
    REG_TILT,
    REG_SRST,
    REG_SPCNT,
    REG_INTSU,
    REG_MODE,
    REG_SR,
    REG_PDET,
    REG_PD,
    REG_END,
};

enum MMA7660_MODES {
    STANDBY_MODE = 0,
    ACTIVE_MODE,
};

enum MMA7660_SAMPLE_RATES_REGS {
    SR_120 = 0,
    SR_64,
    SR_32,
    SR_16,
    SR_8,
    SR_4,
    SR_2,
    SR_1
};

enum mma7660_attributes {
    MMA_ATTR_ENABLE = 0,
    MMA_ATTR_POLL_DELAY,

    MMA_ATTR_FIRST = MMA_ATTR_ENABLE,
    MMA_ATTR_LAST = MMA_ATTR_POLL_DELAY,
};

struct mma7660_sample_rate {
    int reg;
    int value;
};

struct mma7660_status {
    int enable;
    struct mma7660_sample_rate sr;
};

static const struct mma7660_sample_rate MMA7660_SAMPLE_RATES[] = {
    { .reg = SR_120, .value = 120 },
    { .reg = SR_64, .value = 64 },
    { .reg = SR_32, .value = 32 },
    { .reg = SR_16, .value = 16 },
    { .reg = SR_8, .value = 8 },
    { .reg = SR_4, .value = 4 },
    { .reg = SR_2, .value = 2 },
    { .reg = SR_1, .value = 1 },

    // Last entry
    { .reg = 0, .value = 0 }
};

static struct mma7660_status mma_status = {
    .enable     = 0,
    .sr = {
        .reg    = SR_16,
        .value  = 16
    }
};

static ssize_t mma7660_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t mma7660_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

static struct device_attribute mma7660_dev_attrs[] = {
    [MMA_ATTR_ENABLE] = {
        .attr = {
            .name = "enable",
            .mode = S_IRUGO | S_IWUGO,
            .owner = THIS_MODULE
        },
        .show = mma7660_show,
        .store = mma7660_store,
    },
    [MMA_ATTR_POLL_DELAY] = {
        .attr = {
            .name = "poll_delay",
            .mode = S_IRUGO | S_IWUGO,
            .owner = THIS_MODULE
        },
        .show = mma7660_show,
        .store = mma7660_store,
    }
};

s32 mma7660_Read_Alert(struct i2c_client *client, u8 RegAdd) {
    s32 res;

     do{
        res = i2c_smbus_read_byte_data(client, RegAdd);
        if (res < 0){
            return res;
        }
     } while (res & 0x40);            // while Alert bit = 1;
    
    return res;
}

/* read sensor data from mma7660 */
static int mma7660_read_data(struct i2c_client *client , short *x, short *y, short *z) {
    u8    tmp_data[4];
    s32   res;
    short temp;

    res = (i2c_smbus_read_i2c_block_data(client, REG_XOUT, REG_ZOUT - REG_XOUT + 1, tmp_data) < 
            REG_ZOUT - REG_XOUT + 1);

    if  (res < 0) {
        dev_err(&client->dev, "i2c block read failed\n");
            return res;
    }

    *x = tmp_data[0];
    *y = tmp_data[1];
    *z = tmp_data[2];

    if (*x & 0x40){
        if ((res = mma7660_Read_Alert(client, REG_XOUT)) < 0)
            return res;

        *x = res;
    }

    if (*y & 0x40){    
        if ((res = mma7660_Read_Alert(client, REG_YOUT)) < 0)
            return res;

        *y = res;
    }

    if (*z & 0x40){    
        if ((res = mma7660_Read_Alert(client, REG_ZOUT)) < 0)
            return res;

        *z = res;
    }

    if (*x & 0x20)      /* check for the bit 5 */
        *x |= 0xffc0;

    if (*y & 0x20)      /* check for the bit 5 */
        *y |= 0xffc0;

    if (*z & 0x20)      /* check for the bit 5 */
        *z |= 0xffc0;

    // namko: The orientation is 90 degrees off the mark. Hence swap x and y.
    temp = *x;
    *x = *y;
    *y = temp;

    return 0;
}

static struct i2c_client       *mma7660_client;
static struct input_dev        *mma7660_dev;
static struct workqueue_struct *mma7660_workqueue;
static struct delayed_work      mma7660_work;
static struct mutex             mma7660_lock;

/* workqueue function to poll mma7660 data */
static void mma7660_poll_work(struct work_struct *work) {
    short x, y, z;

    // Schedule next update.
    queue_delayed_work(mma7660_workqueue, &mma7660_work, HZ / mma_status.sr.value);

    if (!mma_status.enable)
        return;

    if (mma7660_read_data(mma7660_client, &x, &y, &z) != 0) {
        dev_dbg(&mma7660_client->dev, "mma7660 data read failed\n");
        return;
    }

    /* report the absulate sensor data to input device */
    input_report_abs(mma7660_dev, ABS_X, x);
    input_report_abs(mma7660_dev, ABS_Y, y);
    input_report_abs(mma7660_dev, ABS_Z, z);
    input_sync(mma7660_dev);

    return;
}

// Sets the nearest (largest) sample rate.
// NOTE reset the sensor after calling this function.
static void mma7660_setSampleRate(struct i2c_client *client, int rate) {
    int i, diff, chosen;

    chosen = 0;
    diff = 0x7FFFFFFF;

    for (i = 0; MMA7660_SAMPLE_RATES[i].value; i++) {
        int curr = MMA7660_SAMPLE_RATES[i].value - rate;

        if (curr < 0)
            curr = -curr;

        if (curr < diff) {
            diff = curr;
            chosen = i;
        }
    }

    mma_status.sr = MMA7660_SAMPLE_RATES[chosen];
}

// Enables/disables the sensor.
static void mma7660_enable(struct i2c_client *client, int enable) {
    if ((mma_status.enable && enable) ||
            (!mma_status.enable && !enable))
        return;

    mma_status.enable = (enable ? 1 : 0);

    if (enable) {
        int reg = mma_status.sr.reg & 0x07;

        /* enable gSensor mode 8g, measure */
        i2c_smbus_write_byte_data(client, REG_MODE, STANDBY_MODE);  /* standby mode */
        i2c_smbus_write_byte_data(client, REG_SPCNT, 0x00);         /* no sleep count */
        i2c_smbus_write_byte_data(client, REG_INTSU, 0x00);         /* no interrupt */
        i2c_smbus_write_byte_data(client, REG_PDET, 0xE0);          /* disable tap */
        i2c_smbus_write_byte_data(client, REG_SR, 0xF0 | reg);      /* sample rate */
        i2c_smbus_write_byte_data(client, REG_MODE, ACTIVE_MODE);   /* active mode */
    } else {
        // Disable sensor.
        i2c_smbus_write_byte_data(client, REG_MODE, STANDBY_MODE);  /* standby mode */        
    }
}

static int mma7660_create_attrs(struct device *dev) {
    int i, rc;

    for (i = 0; i < ARRAY_SIZE(mma7660_dev_attrs); i++)
        if ((rc = device_create_file(dev, &mma7660_dev_attrs[i])))
            goto attrs_failed;

    goto succeed;

attrs_failed:
    while (i--)
        device_remove_file(dev, &mma7660_dev_attrs[i]);

succeed:
    return rc;
}

static ssize_t mma7660_show(struct device *dev, struct device_attribute *attr, 
        char *buf) {

    const ptrdiff_t off = attr - mma7660_dev_attrs;

    switch (off) {
    case MMA_ATTR_ENABLE:
        return scnprintf(buf, PAGE_SIZE, "%d\n", mma_status.enable);

    case MMA_ATTR_POLL_DELAY:
        return scnprintf(buf, PAGE_SIZE, "%d\n", (int)(1E+9) / mma_status.sr.value);

    default:
        return -EINVAL;
    }
}

static ssize_t mma7660_store(struct device *dev, struct device_attribute *attr, 
        const char *buf, size_t count) {

    int x = 0;
    const ptrdiff_t off = attr - mma7660_dev_attrs;

    switch (off) {
    case MMA_ATTR_ENABLE:
        if (sscanf(buf, "%d\n", &x) == 1) {
            dev_info(dev, "%s : enable = %d\n", __func__, x);
            mutex_lock(&mma7660_lock);
                mma7660_enable(mma7660_client, x);
            mutex_unlock(&mma7660_lock);
        }
        break;

    case MMA_ATTR_POLL_DELAY:
        if (sscanf(buf, "%d\n", &x) == 1) {
            const int enabled = mma_status.enable;

            if (!x)
                x = 1;

            dev_info(dev, "%s : s_poll_int = %d\n", __func__, x);
            mutex_lock(&mma7660_lock);
                mma7660_enable(mma7660_client, 0);
                mma7660_setSampleRate(mma7660_client, (int)(1E+9) / x);
                mma7660_enable(mma7660_client, enabled);
            mutex_unlock(&mma7660_lock);
        }
        break;

    default:
        return -EINVAL;
    }

    return count;
}

/* detecting mma7660 chip */
static int mma7660_detect(struct i2c_client *client, struct i2c_board_info *info) {
    struct i2c_adapter *adapter = client->adapter;

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
        return -ENODEV;                                        

    strlcpy(info->type, DEVICE_NAME, I2C_NAME_SIZE);
    return 0;
}

static int mma7660_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int rc, ret;

    printk("*** %s ***\n", __func__);
    printk(KERN_INFO "try to detect mma7660 chip id %02x\n", client->addr);
    mutex_init(&mma7660_lock);

    if (!(mma7660_workqueue = create_singlethread_workqueue(DEVICE_NAME))) {
        rc = -ENOMEM; /* most expected reason */
        goto err_create_wq;
    }

    /* allocate & register input polling device  */
    if (!(mma7660_dev = input_allocate_device())) {
        dev_err(&client->dev, "allocate poll device failed!\n");
        rc = -ENOMEM;
        goto err_free_abs;
    }

    mma7660_dev->name = DEVICE_NAME;
    mma7660_dev->id.bustype = BUS_I2C;
    mma7660_dev->dev.parent = &client->dev;

    set_bit(EV_ABS, mma7660_dev->evbit);
    set_bit(ABS_X, mma7660_dev->absbit);
    set_bit(ABS_Y, mma7660_dev->absbit);
    set_bit(ABS_Z, mma7660_dev->absbit);

    if ((ret = input_register_device(mma7660_dev))) {
        dev_err(&client->dev, "register abs device failed!\n");
        rc = -EINVAL;
        goto err_unreg_abs_input;
    }

    /* create device file in sysfs as user interface */
    if ((ret = mma7660_create_attrs(&mma7660_dev->dev))) {
        dev_err(&client->dev, "create device file failed!\n");
        rc = -EINVAL;
        goto err_detach_client;
    }

    // Enable the chip.
    mma7660_client = client;
    mutex_lock(&mma7660_lock);
        mma7660_setSampleRate(client, SAMPLE_RATE_DEFAULT);
        mma7660_enable(client, 1);
    mutex_unlock(&mma7660_lock);

    // Queue work.
    INIT_DELAYED_WORK(&mma7660_work, mma7660_poll_work);
    queue_delayed_work(mma7660_workqueue, &mma7660_work, 0);

    return 0;

err_unreg_abs_input:
    input_unregister_device(mma7660_dev);

err_free_abs:
    input_free_device(mma7660_dev);

err_detach_client:
    destroy_workqueue(mma7660_workqueue);

err_create_wq:
	mutex_destroy(&mma7660_lock);

    return rc; 
}

static int mma7660_remove(struct i2c_client *client) {
    int i;
    printk("*** %s ***\n", __func__);

    for (i = MMA_ATTR_LAST; i >= MMA_ATTR_FIRST; i--)
        device_remove_file(&mma7660_dev->dev, &mma7660_dev_attrs[i]);

    mma7660_enable(client, 0);
    input_unregister_device(mma7660_dev);
    input_free_device(mma7660_dev);
    flush_workqueue(mma7660_workqueue);
    destroy_workqueue(mma7660_workqueue);
	mutex_destroy(&mma7660_lock);

    return 0;
}

static int mma7660_suspend(struct i2c_client *client, pm_message_t state) {
    printk("*** %s ***\n", __func__);
    mma7660_enable(client, 0);
    return 0;
}

static int mma7660_resume(struct i2c_client *client) {
    printk("*** %s ***\n", __func__);
    mma7660_enable(client, 1);
    return 0;
}

static const struct i2c_device_id mma7660_id[] = {
    {DEVICE_NAME, 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, mma7660_id);

static struct i2c_driver mma7660_driver = {
    .class      = I2C_CLASS_HWMON,
    .probe      = mma7660_probe,
    .remove     = mma7660_remove,
    .id_table   = mma7660_id,
    .detect     = mma7660_detect,
    .suspend    = mma7660_suspend,
    .resume     = mma7660_resume,
    .driver = {
        .name   = DEVICE_NAME,
        .owner  = THIS_MODULE,
    },
};

/* register driver */
static int __init init_mma7660(void) {
    int res;
    printk("*** %s ***\n", __func__);

    if ((res = i2c_add_driver(&mma7660_driver)) < 0){
        printk(KERN_INFO "add mma7660 i2c driver failed\n");
        return -ENODEV;
    }

    printk(KERN_INFO "add mma7660 i2c driver\n");
    return (res);
}

static void __exit exit_mma7660(void) {
    printk("*** %s ***\n", __func__);
    return i2c_del_driver(&mma7660_driver);
}

module_init(init_mma7660);
module_exit(exit_mma7660);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MMA7660 sensor driver");
MODULE_LICENSE("GPL");

