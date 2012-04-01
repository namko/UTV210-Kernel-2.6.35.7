#
# Makefile for miscellaneous I2C chip drivers.
#
# Do not add new drivers to this directory! It is DEPRECATED.
#
# Device drivers are better grouped according to the functionality they
# implement rather than to the bus they are connected to. In particular:
# * Hardware monitoring chip drivers go to drivers/hwmon
# * RTC chip drivers go to drivers/rtc
# * I/O expander drivers go to drivers/gpio
#

obj-$(CONFIG_DS1682)		+= ds1682.o
obj-$(CONFIG_SENSORS_PCA963X)	+= pca963x.o
obj-$(CONFIG_SENSORS_TSL2550)	+= tsl2550.o

obj-$(CONFIG_PMIC_ACT8937)	+= act8937.o
obj-$(CONFIG_BOSCH_BMA150)	+= bma150_driver.o smb380.o smb380calib.o  
obj-$(CONFIG_SENSORS_MMC31XX)	+= mmc31xx.o
obj-$(CONFIG_ECOMPASS)		+= mecs.o
ifeq ($(CONFIG_I2C_DEBUG_CHIP),y)
EXTRA_CFLAGS += -DDEBUG
endif

ifeq ($(CONFIG_BOSCH_BMA150_ENABLE_IRQ),y)
EXTRA_CFLAGS += -DBMA150_ENABLE_IRQ
endif

ifeq ($(CONFIG_BOSCH_BMA150_USE_SMBUS),y)
EXTRA_CFLAGS += -DBMA150_SMBUS
endif
ifeq ($(CONFIG_BOSCH_BMA150_WITH_DEBUG),y)
EXTRA_CFLAGS += -DBMA150_DEBUG
endif
ifeq ($(CONFIG_BOSCH_BMA150_HAS_EARLYSUSPEND),y)
EXTRA_CFLAGS += -DBMA150_HAS_EARLYSUSPEND
endif
