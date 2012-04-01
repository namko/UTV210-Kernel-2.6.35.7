/*

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.


   Copyright (C) 2006-2007 - Motorola
   Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.

   Date         Author           Comment
   -----------  --------------   --------------------------------
   2006-Apr-28	Motorola	 The kernel module for running the Bluetooth(R)
				 Sleep-Mode Protocol from the Host side
   2006-Sep-08  Motorola         Added workqueue for handling sleep work.
   2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.
   2011-Mar-3   Lenovo           Changed for sleep work

*/

#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */
#include "hci_uart.h"
//davied add
#include <mach/regs-gpio.h>
#include <linux/gpio.h>
#include <plat/map-base.h>
#include <plat/gpio-cfg.h>
#include <linux/tty.h>
#include <linux/serial_core.h>
#include <plat/regs-serial.h>
#include <linux/wakelock.h>
//--

#define BT_SLEEP_DBG
#ifndef BT_SLEEP_DBG
#ifdef BT_DBG(fmt, arg...)
#undef BT_DBG(fmt, arg...)
#define BT_DBG(fmt, arg...) //do {printk(KERN_ALERT "%s: " fmt "\n" , __FUNCTION__ , ## arg);} while(0)
#endif
#endif
/*
 * Defines
 */

#define VERSION		"1.1"
#define PROC_DIR	"bluetooth/sleep"

static struct wake_lock bt_uart_lock;
unsigned int bt_power_state;
unsigned int bt_state_for_hostwakeup = 0;

struct bluesleep_info {
	unsigned host_wake;
	unsigned ext_wake;
	unsigned host_wake_irq;
	struct uart_port *uport;
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)

/* 1 second timeout */
#define TX_TIMER_INTERVAL	1

/* state variable names and bit positions */
#define BT_PROTO	0x01
#define BT_TXDATA	0x02
#define BT_ASLEEP	0x04

/* global pointer to a single hci device. */
static struct hci_dev *bluesleep_hdev;

static struct bluesleep_info *bsi;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Local function prototypes
 */

static int bluesleep_hci_event(struct notifier_block *this,
			    unsigned long event, void *data);

/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

/** Transmission timer */
static struct timer_list tx_timer;

/** Lock for state transitions */
static spinlock_t rw_lock;

/** Notifier block for HCI events */
struct notifier_block hci_event_nblock = {
	.notifier_call = bluesleep_hci_event,
};

struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
 * Local functions
 */
extern struct uart_port * s3c24xx_serial_get_port(int index);
static void hsuart_power(int on)
{
	struct uart_port * port;
	port = s3c24xx_serial_get_port(0);
	unsigned int ufcon = read_regl(port, S3C2410_UFCON);
	if (on) { //open uart fifo and change rts from gpio to  UART_0_RTSn 
		s3c_gpio_setpin(S5PV210_GPA0(3),0);
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SFN(2)); //gpio to UART_0_RTSn
		ufcon |= S3C2410_UFCON_FIFOMODE;
                write_regl(port, S3C2410_UFCON, ufcon);
	} else { //close uart fifo and change rts from UART_0_RTSn to gpio
		ufcon &= ~S3C2410_UFCON_FIFOMODE;
                write_regl(port, S3C2410_UFCON, ufcon);
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SFN(1)); //UART_0_RTSn to gpio
		s3c_gpio_setpin(S5PV210_GPA0(3),1);
	}
}


/**
 * @return 0 if the Host can go to sleep, 1 otherwise.
 */
static inline int bluesleep_can_sleep(void)
{
	/* check if MSM_WAKE_BT_GPIO and BT_WAKE_MSM_GPIO are both deasserted */
	return (!gpio_get_value(bsi->ext_wake)) &&
		(!gpio_get_value(bsi->host_wake)) &&
		(bsi->uport != NULL); //
}

void bluesleep_sleep_wakeup(void)
{
	if (test_bit(BT_ASLEEP, &flags)) {
		BT_DBG("waking up...");
		printk(KERN_INFO "bt waking up...");
		/* Start the timer */
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		gpio_set_value(bsi->ext_wake, 1); //
		clear_bit(BT_ASLEEP, &flags);
		/*Activating UART */
	}
}

/**
 :* @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
extern unsigned int s3c24xx_serial_tx_empty(struct uart_port *port);
static void bluesleep_sleep_work(struct work_struct *work)
{
	if (bluesleep_can_sleep()) {
		/* already asleep, this is an error case */
		if (test_bit(BT_ASLEEP, &flags)) {
			BT_DBG("already asleep");
			printk(KERN_INFO"bt already asleep");
			return;
		}

		if (s3c24xx_serial_tx_empty(bsi->uport)) {
			BT_DBG("going to sleep...");
			printk(KERN_INFO"bt going to sleep...");
			set_bit(BT_ASLEEP, &flags);
			/*Deactivating UART */
		} else {

		  mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
			return;
		}
	} else {
		bluesleep_sleep_wakeup();
	}
}

/**
 * A tasklet function that runs in tasklet context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param data Not used.
 */
extern unsigned int bt_state_for_hostwakeup;
static void bluesleep_hostwake_task(unsigned long data)
{
	BT_DBG("hostwake line change");

	spin_lock(&rw_lock);
	

	if (gpio_get_value(bsi->host_wake) && bt_state_for_hostwakeup) 
	{
		printk(KERN_INFO "wake_source: bt \nbt_state_for_hostwakeup = %d\n",bt_state_for_hostwakeup);
		wake_lock_timeout(&bt_uart_lock, 4*HZ);
		bluesleep_sleep_wakeup(); //davied add
	}
	else
	{
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
	}
	spin_unlock(&rw_lock);
}

/**
 * Handles proper timer action when outgoing data is delivered to the
 * HCI line discipline. Sets BT_TXDATA.
 */
static void bluesleep_outgoing_data(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	/* log data passing by */
	set_bit(BT_TXDATA, &flags);

	/* if the tx side is sleeping... */
	if (!gpio_get_value(bsi->ext_wake)) { //

		BT_DBG("tx was sleeping");
		bluesleep_sleep_wakeup();
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Handles HCI device events.
 * @param this Not used.
 * @param event The event that occurred.
 * @param data The HCI device associated with the event.
 * @return <code>NOTIFY_DONE</code>.
 */
static int bluesleep_hci_event(struct notifier_block *this,
				unsigned long event, void *data)
{
	struct hci_dev *hdev = (struct hci_dev *) data;
	struct hci_uart *hu;
	struct uart_state *state;

	if (!hdev)
		return NOTIFY_DONE;

	switch (event) {
	case HCI_DEV_REG:
		if (!bluesleep_hdev) {
			bluesleep_hdev = hdev;
			hu  = (struct hci_uart *) hdev->driver_data;
			state = (struct uart_state *) hu->tty->driver_data;
			bsi->uport = state->uart_port;
		}
		break;
	case HCI_DEV_UNREG:
		bluesleep_hdev = NULL;
		bsi->uport = NULL;
		break;
	case HCI_DEV_WRITE:
		bluesleep_outgoing_data();
		break;
	}

	return NOTIFY_DONE;
}

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(unsigned long data)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	BT_DBG("Tx timer expired");

	/* were we silent during the last timeout? */
	if (!test_bit(BT_TXDATA, &flags)) {
		BT_DBG("Tx has been idle");
		gpio_set_value(bsi->ext_wake, 0); //
		bluesleep_tx_idle();
	} else {
		BT_DBG("Tx data during last period");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
	}

	/* clear the incoming data flag */
	clear_bit(BT_TXDATA, &flags);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Schedules a tasklet to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */
	tasklet_schedule(&hostwake_task);
	return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(void)
{
	int retval;
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return 0;
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	if (!atomic_dec_and_test(&open_count)) {
		atomic_inc(&open_count);
		return -EBUSY;
	}

	/* start the timer */

	mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));

	/* assert BT_WAKE */
	gpio_set_value(bsi->ext_wake, 1); 
	retval = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IORESOURCE_IRQ | IRQF_TRIGGER_RISING | IORESOURCE_IRQ_SHAREABLE,
				"bluetooth hostwake", NULL);
	if (retval  < 0) {
		BT_ERR("Couldn't acquire BT_HOST_WAKE IRQ");
		goto fail;
	}
	set_irq_wake(bsi->host_wake_irq, 1);

	set_bit(BT_PROTO, &flags);
	return 0;
fail:
	del_timer(&tx_timer);
	atomic_inc(&open_count);

	return retval;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (!test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return;
	}

	/* assert BT_WAKE */
	gpio_set_value(bsi->ext_wake, 1);//
	del_timer(&tx_timer);
	clear_bit(BT_PROTO, &flags);

	if (test_bit(BT_ASLEEP, &flags)) {
		clear_bit(BT_ASLEEP, &flags);
		hsuart_power(1);
	}

	atomic_inc(&open_count);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
	if (disable_irq_wake(bsi->host_wake_irq))
		BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");
	free_irq(bsi->host_wake_irq, NULL);
}
/**
 * Read the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the
 * pin is high, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluepower_read_proc_btwake(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	*eof = 1;
	return sprintf(page, "btwake:%u\n", gpio_get_value(bsi->ext_wake));
}

/**
 * Write the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int bluepower_write_proc_btwake(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	char *buf;

	if (count < 1)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}

	if (buf[0] == '0') {
		gpio_set_value(bsi->ext_wake, 0); //
	} else if (buf[0] == '1') {
		gpio_set_value(bsi->ext_wake, 1); //
	} else {
		kfree(buf);
		return -EINVAL;
	}

	kfree(buf);
	return count;
}

/**
 * Read the <code>BT_HOST_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the pin
 * is high, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluepower_read_proc_hostwake(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	*eof = 1;
	return sprintf(page, "hostwake: %u \n", gpio_get_value(bsi->host_wake));
}


/**
 * Read the low-power status of the Host via the proc interface.
 * When this function returns, <code>page</code> contains a 1 if the Host
 * is asleep, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluesleep_read_proc_asleep(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	unsigned int asleep;

	asleep = test_bit(BT_ASLEEP, &flags) ? 1 : 0;
	*eof = 1;
	return sprintf(page, "asleep: %u\n", asleep);
}

/**
 * Read the low-power protocol being used by the Host via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the Host
 * is using the Sleep Mode Protocol, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluesleep_read_proc_proto(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	unsigned int proto;

	proto = test_bit(BT_PROTO, &flags) ? 1 : 0;
	*eof = 1;
	return sprintf(page, "proto: %u\n", proto);
}

/**
 * Modify the low-power protocol used by the Host via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int bluesleep_write_proc_proto(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	char proto;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&proto, buffer, 1))
		return -EFAULT;

	if (proto == '0'){
		bluesleep_stop();
		bt_power_state = 0;	
	}
	else{
		bluesleep_start();
		bt_power_state = 1;
	}

	/* claim that we wrote everything */
	return count;
}

static int __init bluesleep_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_host_wake");
	if (!res) {
		BT_ERR("couldn't find host_wake gpio\n");
		ret = -ENODEV;
		goto free_bsi;
	}
	bsi->host_wake = res->start;

	bt_power_state = 0;	

	ret = gpio_request(bsi->host_wake, "bt_host_wake");
	if (ret){
                printk(KERN_INFO "gpio request bluetooth host_wakeup error : %d\n", ret);
		goto free_bsi;
        }
        else {
                s3c_gpio_cfgpin(bsi->host_wake, S3C_GPIO_SFN(0xf)); //input
//                s3c_gpio_setpull(bsi->host_wake, S3C_GPIO_PULL_NONE); //pull NONE because  wakeup active is high
        }

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_ext_wake");
	if (!res) {
		BT_ERR("couldn't find ext_wake gpio\n");
		ret = -ENODEV;
		goto free_bt_host_wake;
	}
	bsi->ext_wake = res->start;

	ret = gpio_request(bsi->ext_wake, "bt_ext_wake");
	if (ret){
		printk(KERN_INFO "gpio request bluetooth bt_wakeup error : %d\n", ret);
		goto free_bt_host_wake;
	} else {
                s3c_gpio_cfgpin(bsi->ext_wake, S3C_GPIO_OUTPUT);
                s3c_gpio_setpull(bsi->ext_wake, S3C_GPIO_PULL_NONE);
        }	

	/* assert bt wake */ //
	gpio_set_value(bsi->ext_wake, 1);

	bsi->host_wake_irq = platform_get_irq_byname(pdev, "host_wake");
	if (bsi->host_wake_irq < 0) {
		BT_ERR("couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}

	return 0;

free_bt_ext_wake:
	gpio_free(bsi->ext_wake);
free_bt_host_wake:
	gpio_free(bsi->host_wake);
free_bsi:
	kfree(bsi);
	return ret;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	/* assert bt wake */ //
	gpio_set_value(bsi->ext_wake, 1);
	if (test_bit(BT_PROTO, &flags)) {
		if (disable_irq_wake(bsi->host_wake_irq))
			BT_ERR("Couldn't disable hostwake IRQ wakeup mode \n");
		free_irq(bsi->host_wake_irq, NULL);
		del_timer(&tx_timer);
		if (test_bit(BT_ASLEEP, &flags))
			hsuart_power(1);
	}

	gpio_free(bsi->host_wake);
	gpio_free(bsi->ext_wake);
	kfree(bsi);
	return 0;
}

static int bt_suspend_mode(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int bt_resume_back(struct platform_device *dev)
{
	if(bt_power_state){
		printk(KERN_INFO"bt resume back !!!!!\n");
		bt_state_for_hostwakeup = 0;	
	}
	return 0;
}

static struct platform_driver bluesleep_driver = {
	.probe = bluesleep_probe,
	.remove = bluesleep_remove,
	.driver = {
		.name = "bluesleep",
		.owner = THIS_MODULE,
	},
	.suspend = bt_suspend_mode,
	.resume = bt_resume_back,
};
/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
	int retval;
	struct proc_dir_entry *ent;

	BT_INFO("MSM Sleep Mode Driver Ver %s", VERSION);

	retval = platform_driver_register(&bluesleep_driver);
	if (retval)
		return retval;

	bluesleep_hdev = NULL;

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bluetooth_dir == NULL) {
		BT_ERR("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (sleep_dir == NULL) {
		BT_ERR("Unable to create /proc/%s directory", PROC_DIR);
		return -ENOMEM;
	}

	/* Creating read/write "btwake" entry */
	ent = create_proc_entry("btwake", 0, sleep_dir);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/btwake entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}
	ent->read_proc = bluepower_read_proc_btwake;
	ent->write_proc = bluepower_write_proc_btwake;

	/* read only proc entries */
	if (create_proc_read_entry("hostwake", 0, sleep_dir,
				bluepower_read_proc_hostwake, NULL) == NULL) {
		BT_ERR("Unable to create /proc/%s/hostwake entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write proc entries */
	ent = create_proc_entry("proto", 0, sleep_dir);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/proto entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}
	ent->read_proc = bluesleep_read_proc_proto;
	ent->write_proc = bluesleep_write_proc_proto;

	/* read only proc entries */
	if (create_proc_read_entry("asleep", 0,
			sleep_dir, bluesleep_read_proc_asleep, NULL) == NULL) {
		BT_ERR("Unable to create /proc/%s/asleep entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	flags = 0; /* clear all status bits */

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* Initialize timer */
	init_timer(&tx_timer);
	tx_timer.function = bluesleep_tx_timer_expire;
	tx_timer.data = 0;

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	hci_register_notifier(&hci_event_nblock);

	wake_lock_init(&bt_uart_lock, WAKE_LOCK_SUSPEND, "bt_uart");

	return 0;

fail:
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
	hci_unregister_notifier(&hci_event_nblock);
	platform_driver_unregister(&bluesleep_driver);

	wake_lock_destroy(&bt_uart_lock);

	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
