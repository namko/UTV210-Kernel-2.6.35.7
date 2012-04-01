#ifndef _F_MASS_STORAGE_H
#define _F_MASS_STORAGE_H
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/limits.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/switch.h>
#include <linux/freezer.h>
#include <linux/utsname.h>
#include <linux/wakelock.h>
#include <linux/types.h>
#include <linux/platform_device.h>

typedef bool (*GetCurLunDeviceStatus)(void);
struct  mode_data{
	char		*file;/*file name*/
	int		removable;/*can be removed or not*/
};
extern struct mode_data DefaultModeData[8];
#endif
