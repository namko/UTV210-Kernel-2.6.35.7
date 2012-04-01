/* Copyright (c) 2009-2010, Samsung SLSI. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/*
 * Bluetooth Power Switch Module
 * controls power to external Bluetooth device
 * with interface to power management device
 */
 
#include <linux/sysdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/serial_core.h>
#include <linux/io.h>

#include <plat/map-base.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-gpio.h>
#include <linux/gpio.h>

#include <mach/regs-irq.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <plat/regs-rtc.h>

#include <plat/gpio-cfg.h>
#include <mach/gpio-bank.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <linux/gpio.h>

#include <plat/s5pv210.h>


static const char enable_string[]="enable";
static const char disable_string[]="disable";



static ssize_t bt_wake_status_show(struct sysdev_class *class, char *buf)
{
	printk("%s:GPH2(7) CON:0x%x DATA:0x%x PULL:0x%x  DRV:0x%x\n",__FUNCTION__,
	__raw_readl(S5PV210_GPH2_BASE + 0x00),
	__raw_readl(S5PV210_GPH2_BASE + 0x04),
	__raw_readl(S5PV210_GPH2_BASE + 0x08),
	__raw_readl(S5PV210_GPH2_BASE + 0x0c));
	
	if((1 == gpio_get_value(S5PV210_GPH2(7)) && ( (0x1<<28) ==( __raw_readl(S5PV210_GPH2_BASE + 0x00) & (0xF<<28))) ))
        	{printk("%s\n",enable_string);return sprintf(buf, "%s\n", enable_string);}
        else
            {printk("%s\n",disable_string);return sprintf(buf, "%s\n", disable_string);}
}

static ssize_t bt_wake_status_store(struct sysdev_class *class,
                                const char *buf, size_t count)
{
	int len = count;
	char * cp;

	cp =memchr(buf,'\n',count);
	if(cp)
		len = cp -buf;

	if(len == sizeof enable_string -1 && 
			strncmp(buf, enable_string, len)==0){
		s3c_gpio_cfgpin(S5PV210_GPH2(7), S3C_GPIO_OUTPUT);  
		gpio_set_value(S5PV210_GPH2(7), 1);
		udelay(50);	
	}

	if(len == sizeof disable_string -1 && 
			strncmp(buf, disable_string, len)==0){
		s3c_gpio_cfgpin(S5PV210_GPH2(7), S3C_GPIO_OUTPUT);  
		gpio_set_value(S5PV210_GPH2(7), 0);
		udelay(50);	
	}

	return count;
}

		
		
static ssize_t sdio_status_show(struct sysdev_class *class, char *buf)
{
	printk("%s:GPG3(0-6,ex 2) CON:0x%x DATA:0x%x PULL:0x%x  DRV:0x%x\n",__FUNCTION__,
	__raw_readl(S5PV210_GPG3_BASE + 0x00),
	__raw_readl(S5PV210_GPG3_BASE + 0x04),
	__raw_readl(S5PV210_GPG3_BASE + 0x08),
	__raw_readl(S5PV210_GPG3_BASE + 0x0c));
	
       if ((__raw_readl(S5PV210_GPG3_BASE) & 0xFFFF0FF )== 0x2222022)
        	{printk("%s\n",enable_string);return sprintf(buf, "%s\n", enable_string);}
        else
            {printk("%s\n",disable_string);return sprintf(buf, "%s\n", disable_string);}
}

static ssize_t sdio_status_store(struct sysdev_class *class,
                                const char *buf, size_t count)
{
	int len = count;
	char * cp;

	cp =memchr(buf,'\n',count);
	if(cp)
		len = cp -buf;

	if(len == sizeof enable_string -1 && 
			strncmp(buf, enable_string, len)==0){
		int gpio;
		for (gpio = S5PV210_GPG3(0); gpio <= S5PV210_GPG3(6); gpio++) {
                         if (gpio == S5PV210_GPG3(2))
                             continue;   
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
		writel(0x3fff, S5PV210_GPG3DRV);
		udelay(50);	
	}

	if(len == sizeof disable_string -1 && 
			strncmp(buf, disable_string, len)==0){
		int gpio;
		for (gpio = S5PV210_GPG3(0); gpio <= S5PV210_GPG3(6); gpio++) {
                         if (gpio == S5PV210_GPG3(2))
                             continue;   
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
		writel(0x0, S5PV210_GPG3DRV);
		udelay(50);	
	}

	return count;
}



static ssize_t uart_status_show(struct sysdev_class *class, char *buf)
{
	printk("%s:GPA0(0-3) CON:0x%x DATA:0x%x PULL:0x%x  DRV:0x%x\n",__FUNCTION__,
	__raw_readl(S5PV210_GPA0_BASE + 0x00),
	__raw_readl(S5PV210_GPA0_BASE + 0x04),
	__raw_readl(S5PV210_GPA0_BASE + 0x08),
	__raw_readl(S5PV210_GPA0_BASE + 0x0c));

       if ((__raw_readl(S5PV210_GPA0_BASE) & 0xFFFF )== 0x2222)
        	{printk("%s\n",enable_string);return sprintf(buf, "%s\n", enable_string);}
        else
            {printk("%s\n",disable_string);return sprintf(buf, "%s\n", disable_string);}
}

static ssize_t uart_status_store(struct sysdev_class *class,
                                const char *buf, size_t count)
{
	int len = count;
	char * cp;

	cp =memchr(buf,'\n',count);
	if(cp)
		len = cp -buf;

	if(len == sizeof enable_string -1 && 
			strncmp(buf, enable_string, len)==0){
		//uart, use RTS/CTS as auto flow control
		s3c_gpio_setpull(S5PV210_GPA0(0), S3C_GPIO_PULL_NONE);  
		s3c_gpio_cfgpin(S5PV210_GPA0(0), S3C_GPIO_SFN(2));   
		s3c_gpio_setpull(S5PV210_GPA0(1), S3C_GPIO_PULL_NONE);  
		s3c_gpio_cfgpin(S5PV210_GPA0(1), S3C_GPIO_SFN(2));   
		s3c_gpio_setpull(S5PV210_GPA0(2), S3C_GPIO_PULL_NONE);  
		s3c_gpio_cfgpin(S5PV210_GPA0(2), S3C_GPIO_SFN(2));   
		s3c_gpio_setpull(S5PV210_GPA0(3), S3C_GPIO_PULL_NONE);  
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SFN(2));   
		udelay(50);	
	}

	if(len == sizeof disable_string -1 && 
			strncmp(buf, disable_string, len)==0){
		//uart, input
		s3c_gpio_setpull(S5PV210_GPA0(0), S3C_GPIO_PULL_NONE);  
		s3c_gpio_cfgpin(S5PV210_GPA0(0), S3C_GPIO_SFN(0));   
		s3c_gpio_setpull(S5PV210_GPA0(1), S3C_GPIO_PULL_NONE);  
		s3c_gpio_cfgpin(S5PV210_GPA0(1), S3C_GPIO_SFN(0));   
		s3c_gpio_setpull(S5PV210_GPA0(2), S3C_GPIO_PULL_NONE);  
		s3c_gpio_cfgpin(S5PV210_GPA0(2), S3C_GPIO_SFN(0));   
		s3c_gpio_setpull(S5PV210_GPA0(3), S3C_GPIO_PULL_NONE);  
		s3c_gpio_cfgpin(S5PV210_GPA0(3), S3C_GPIO_SFN(0));   
		udelay(50);	
	}

	return count;
}




static ssize_t clock_status_show(struct sysdev_class *class, char *buf)
{
		printk("%s\n",__FUNCTION__);

       if (__raw_readl(S3C_VA_RTC + S3C2410_RTCCON) & S3C2410_RTCCON_CLKEN)
        	{printk("%s\n",enable_string);return sprintf(buf, "%s\n", enable_string);}
        else
            {printk("%s\n",disable_string);return sprintf(buf, "%s\n", disable_string);}
}

static ssize_t clock_status_store(struct sysdev_class *class,
                                const char *buf, size_t count)
{
	int len = count;
	char * cp;

	cp =memchr(buf,'\n',count);
	if(cp)
		len = cp -buf;

	if(len == sizeof enable_string -1 && 
			strncmp(buf, enable_string, len)==0){
		{unsigned int tmp;
		tmp = __raw_readl(S3C_VA_RTC + S3C2410_RTCCON) & ~S3C2410_RTCCON_CLKEN;
		tmp |= S3C2410_RTCCON_CLKEN;
		__raw_writel(tmp, S3C_VA_RTC + S3C2410_RTCCON);}
		udelay(50);	
		return count;
	}

	if(len == sizeof disable_string -1 && 
			strncmp(buf, disable_string, len)==0){
		{unsigned int tmp;
		tmp = __raw_readl(S3C_VA_RTC + S3C2410_RTCCON) & ~S3C2410_RTCCON_CLKEN;
		__raw_writel(tmp, S3C_VA_RTC + S3C2410_RTCCON);}
		udelay(50);	
		return count;
	}

	return -EINVAL;
}



static ssize_t btreset_status_show(struct sysdev_class *class, char *buf)
{
		printk("%s:GPG1(4) CON:0x%x DATA:0x%x PULL:0x%x  DRV:0x%x\n",__FUNCTION__,
	__raw_readl(S5PV210_GPG1_BASE + 0x00),
	__raw_readl(S5PV210_GPG1_BASE + 0x04),
	__raw_readl(S5PV210_GPG1_BASE + 0x08),
	__raw_readl(S5PV210_GPG1_BASE + 0x0c));	
	if((1 == gpio_get_value(S5PV210_GPG1(4)) && ( (0x1<<16) == (__raw_readl(S5PV210_GPG1_BASE + 0x00) & (0xF<<16))) ))
        	{printk("%s\n",enable_string);return sprintf(buf, "%s\n", enable_string);}
        else
            {printk("%s\n",disable_string);return sprintf(buf, "%s\n", disable_string);}
}

static ssize_t btreset_status_store(struct sysdev_class *class,
                                const char *buf, size_t count)
{
	int len = count;
	char * cp;

	cp =memchr(buf,'\n',count);
	if(cp)
		len = cp -buf;

	if(len == sizeof enable_string -1 && 
			strncmp(buf, enable_string, len)==0){
		s3c_gpio_setpull(S5PV210_GPG1(4), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPG1(4), S3C_GPIO_OUTPUT); 
		gpio_set_value(S5PV210_GPG1(4), 1); 	
		udelay(50);	
		return count;
	}

	if(len == sizeof disable_string -1 && 
			strncmp(buf, disable_string, len)==0){
		s3c_gpio_setpull(S5PV210_GPG1(4), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPG1(4), S3C_GPIO_OUTPUT); 
		gpio_set_value(S5PV210_GPG1(4), 0); 	
		udelay(50);	
		return count;
	}

	return -EINVAL;
}



static ssize_t shutdown_status_show(struct sysdev_class *class, char *buf)
{
		printk("%s:GPG1(2) CON:0x%x DATA:0x%x PULL:0x%x  DRV:0x%x\n",__FUNCTION__,
	__raw_readl(S5PV210_GPG1_BASE + 0x00),
	__raw_readl(S5PV210_GPG1_BASE + 0x04),
	__raw_readl(S5PV210_GPG1_BASE + 0x08),
	__raw_readl(S5PV210_GPG1_BASE + 0x0c));	
	if((1 == gpio_get_value(S5PV210_GPG1(2)) && ( (0x1<<8) == (__raw_readl(S5PV210_GPG1_BASE + 0x00) & (0xF<<8))) ))
        	{printk("%s\n",enable_string);return sprintf(buf, "%s\n", enable_string);}
        else
            {printk("%s\n",disable_string);return sprintf(buf, "%s\n", disable_string);}
}

static ssize_t shutdown_status_store(struct sysdev_class *class,
                                const char *buf, size_t count)
{
	int len = count;
	char * cp;

	cp =memchr(buf,'\n',count);
	if(cp)
		len = cp -buf;

	if(len == sizeof enable_string -1 && 
			strncmp(buf, enable_string, len)==0){
		s3c_gpio_setpull(S5PV210_GPG1(2), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPG1(2), S3C_GPIO_OUTPUT); 
		gpio_set_value(S5PV210_GPG1(2), 1); 	
		udelay(50);	
		return count;
	}

	if(len == sizeof disable_string -1 && 
			strncmp(buf, disable_string, len)==0){
		s3c_gpio_setpull(S5PV210_GPG1(2), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPG1(2), S3C_GPIO_OUTPUT); 
		gpio_set_value(S5PV210_GPG1(2), 0); 	
		udelay(50);	
		return count;
	}

	return -EINVAL;
}


static ssize_t pwren_status_show(struct sysdev_class *class, char *buf)
{
		printk("%s:GPH2(6) CON:0x%x DATA:0x%x PULL:0x%x  DRV:0x%x\n",__FUNCTION__,
	__raw_readl(S5PV210_GPH2_BASE + 0x00),
	__raw_readl(S5PV210_GPH2_BASE + 0x04),
	__raw_readl(S5PV210_GPH2_BASE + 0x08),
	__raw_readl(S5PV210_GPH2_BASE + 0x0c));	

	if((1 == gpio_get_value(S5PV210_GPH2(6)) && ( (0x1<<24) == (__raw_readl(S5PV210_GPH2_BASE + 0x00) & (0xF<<24))) ))
        	{printk("%s\n",enable_string);return sprintf(buf, "%s\n", enable_string);}
        else
            {printk("%s\n",disable_string);return sprintf(buf, "%s\n", disable_string);}
}

static ssize_t pwren_status_store(struct sysdev_class *class,
                                const char *buf, size_t count)
{
	int len = count;
	char * cp;

	cp =memchr(buf,'\n',count);
	if(cp)
		len = cp -buf;

	if(len == sizeof enable_string -1 && 
			strncmp(buf, enable_string, len)==0){
		s3c_gpio_setpull(S5PV210_GPH2(6), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPH2(6), S3C_GPIO_OUTPUT); 
		gpio_set_value(S5PV210_GPH2(6), 1); 	
		udelay(50);	
		return count;
	}

	if(len == sizeof disable_string -1 && 
			strncmp(buf, disable_string, len)==0){
		s3c_gpio_setpull(S5PV210_GPH2(6), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPH2(6), S3C_GPIO_OUTPUT); 
		gpio_set_value(S5PV210_GPH2(6), 0); 	
		udelay(50);	
		return count;
	}

	return -EINVAL;
}


static ssize_t wifireset_status_show(struct sysdev_class *class, char *buf)
{
		printk("%s:GPH3(7) CON:0x%x DATA:0x%x PULL:0x%x  DRV:0x%x\n",__FUNCTION__,
	__raw_readl(S5PV210_GPH3_BASE + 0x00),
	__raw_readl(S5PV210_GPH3_BASE + 0x04),
	__raw_readl(S5PV210_GPH3_BASE + 0x08),
	__raw_readl(S5PV210_GPH3_BASE + 0x0c));	

	if((1 == gpio_get_value(S5PV210_GPH3(7)) && ( (0x1<<28) == (__raw_readl(S5PV210_GPH3_BASE + 0x00) & (0xF<<28))) ))
        	{printk("%s\n",enable_string);return sprintf(buf, "%s\n", enable_string);}
        else
            {printk("%s\n",disable_string);return sprintf(buf, "%s\n", disable_string);}
}

static ssize_t wifireset_status_store(struct sysdev_class *class,
                                const char *buf, size_t count)
{
	int len = count;
	char * cp;

	cp =memchr(buf,'\n',count);
	if(cp)
		len = cp -buf;

	if(len == sizeof enable_string -1 && 
			strncmp(buf, enable_string, len)==0){
		s3c_gpio_setpull(S5PV210_GPH3(7), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPH3(7), S3C_GPIO_OUTPUT); 
		gpio_set_value(S5PV210_GPH3(7), 1); 	
		udelay(50);	
		return count;
	}

	if(len == sizeof disable_string -1 && 
			strncmp(buf, disable_string, len)==0){
		s3c_gpio_setpull(S5PV210_GPH3(7), S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(S5PV210_GPH3(7), S3C_GPIO_OUTPUT); 
		gpio_set_value(S5PV210_GPH3(7), 0); 
		udelay(50);	
		return count;
	}

	return -EINVAL;
}

static SYSDEV_CLASS_ATTR(bt_wake, 0644, bt_wake_status_show, bt_wake_status_store);
static SYSDEV_CLASS_ATTR(sdio, 0644, sdio_status_show, sdio_status_store);
static SYSDEV_CLASS_ATTR(uart, 0644, uart_status_show, uart_status_store);
static SYSDEV_CLASS_ATTR(clock, 0644, clock_status_show, clock_status_store);
static SYSDEV_CLASS_ATTR(btreset, 0644, btreset_status_show, btreset_status_store);
static SYSDEV_CLASS_ATTR(shutdown, 0644, shutdown_status_show, shutdown_status_store);
static SYSDEV_CLASS_ATTR(pwren, 0644, pwren_status_show, pwren_status_store);
static SYSDEV_CLASS_ATTR(wifireset, 0644, wifireset_status_show, wifireset_status_store);

static struct sysdev_class_attribute *mbluetooth_attributes[] = {
       &attr_bt_wake,
       &attr_sdio,
       &attr_uart,
       &attr_clock,
       &attr_btreset,
	&attr_shutdown,
	&attr_pwren,
	&attr_wifireset,		
        NULL
};

static struct sysdev_class modulebluetooth_class = {
        .name = "bluetooth",
};

static int __init modulebluetooth_sys(void)
{
	struct sysdev_class_attribute **attr;
        int res;

        res=sysdev_class_register(&modulebluetooth_class);
        if (unlikely(res)) {
                return res;
        }

        for (attr = mbluetooth_attributes; *attr; attr++) {
                res = sysdev_class_create_file(&modulebluetooth_class, *attr);
                if (res)
                        goto out_unreg;
        }
/*
* sys user interface
* cd /sys/devices/system/bluetooth/; echo disable > btreset; echo disable> pwren; echo disable >wifireset ;echo disable > shutdown
* cd /sys/devices/system/bluetooth/; echo enable > btreset; echo enable> pwren; echo enable >wifireset ;echo enable > shutdown
* cd /sys/devices/system/bluetooth/; cat btreset;cat pwren;cat wifireset;cat shutdown
*/
	return 0;

out_unreg:
        for (; attr >= mbluetooth_attributes; attr--)
                sysdev_class_remove_file(&modulebluetooth_class, *attr);
        sysdev_class_unregister(&modulebluetooth_class);

        return res;
}
module_init(modulebluetooth_sys);
