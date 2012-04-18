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

#define DEVICE_NAME			"mma7660"
#define ONEGVALUE			21
#define	AXIS_MAX			(ONEGVALUE/2)
#define POLL_INTERVAL		20
#define RES_BUFF_LEN		160

#undef DBG
#define DBG(format, arg...) do { if (debug) printk(KERN_DEBUG "%s: " format "\n" , __FILE__ , ## arg); } while (0)

enum {
	AXIS_DIR_X	= 0,
	AXIS_DIR_X_N,
	AXIS_DIR_Y,
	AXIS_DIR_Y_N,
	AXIS_DIR_Z,
	AXIS_DIR_Z_N,
};

enum {
	SLIDER_X_UP	= 0,
	SLIDER_X_DOWN	,
	SLIDER_Y_UP	,
	SLIDER_Y_DOWN	,
};

static char *axis_dir[6] = {
	[AXIS_DIR_X] 		= "x",
	[AXIS_DIR_X_N] 		= "-x",
	[AXIS_DIR_Y]		= "y",
	[AXIS_DIR_Y_N]		= "-y",
	[AXIS_DIR_Z]		= "z",
	[AXIS_DIR_Z_N]		= "-z",
};

enum {
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

enum {
	STANDBY_MODE = 0,
	ACTIVE_MODE,
};

enum {
	INT_1L_2P = 0,
	INT_1P_2L,
	INT_1SP_2P,
};

struct mma7660_status {
	u8 mode;
	u8 ctl1;
	u8 ctl2;
	u8 axis_dir_x;
	u8 axis_dir_y;
	u8 axis_dir_z;
	u8 slider_key[4];
};

static ssize_t mma7660_show(struct device *dev,struct device_attribute *attr, char *buf);
static ssize_t mma7660_store(struct device *dev,struct device_attribute *attr, const char *buf,size_t count);
static int mma7660_suspend(struct i2c_client *client, pm_message_t state);
static int mma7660_resume(struct i2c_client *client);

static void mma7660_poll_work(struct work_struct *work);

static struct i2c_driver mma7660_driver;
static int mma7660_probe(struct i2c_client *client,const struct i2c_device_id *id);
static int mma7660_remove(struct i2c_client *client);
static int mma7660_detect(struct i2c_client *client, struct i2c_board_info *info);

static const struct i2c_device_id mma7660_id[] = {
	{"mma7660", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, mma7660_id);

static struct i2c_client 		*mma7660_client;
static struct input_dev			*mma7660_dev;

static int 			debug = 0;
static int 			poll_int = POLL_INTERVAL;	
static char			res_buff[RES_BUFF_LEN];

static struct delayed_work mma7660_work;
static struct workqueue_struct *mma7660_workqueue;

static struct i2c_driver mma7660_driver = {
	.driver 	= {
				.name = "mma7660",
			},
	.class		= I2C_CLASS_HWMON,
	.probe		= mma7660_probe,
	.remove		= mma7660_remove,
	.id_table	= mma7660_id,
	.detect		= mma7660_detect,
	.suspend 	= mma7660_suspend,
	.resume 	= mma7660_resume,
};

static struct device_attribute mma7660_dev_attr = {
	.attr 	= {
		 .name = "mma7660_ctl",
		 .mode = S_IRUGO | S_IWUGO,
	},
	.show 	= mma7660_show,
	.store 	= mma7660_store,
};

static struct mma7660_status mma_status = {
	.mode 		= 0,
	.ctl1 		= 0,
	.ctl2 		= 0,
	.axis_dir_x	= AXIS_DIR_X,
	.axis_dir_y	= AXIS_DIR_Y,
	.axis_dir_z	= AXIS_DIR_Z,
	.slider_key	= {0,0,0,0},
};

enum {
	MMA_REG_R = 0,		/* read register 	*/
	MMA_REG_W,			/* write register 	*/
	MMA_DUMPREG,		/* dump registers	*/
	MMA_REMAP_AXIS,		/* remap directiron of 3-axis	*/
	MMA_DUMPAXIS,		/* dump current axis mapping	*/
	MMA_ENJOYSTICK,		/* enable joystick	emulation	*/
	MMA_DISJOYSTICK,	/* disable joystick emulation	*/
	MMA_SLIDER_KEY,		/* set slider key		*/
	MMA_DUMP_SLKEY,		/* dump slider key code setting	*/
	MMA_CMD_MAX
};

static char *command[MMA_CMD_MAX] = {
	[MMA_REG_R] 		= "readreg",
	[MMA_REG_W] 		= "writereg",
	[MMA_DUMPREG]		= "dumpreg",
	[MMA_REMAP_AXIS]	= "remapaxis",
	[MMA_DUMPAXIS]		= "dumpaxis",
	[MMA_ENJOYSTICK]	= "enjoystick",
	[MMA_DISJOYSTICK]	= "disjoystick",
	[MMA_SLIDER_KEY]	= "sliderkey",
	[MMA_DUMP_SLKEY]	= "dumpslkey",
};

s32 mma7660_Read_Alert(u8 RegAdd){
	s32 res;

 	do{
		res = i2c_smbus_read_byte_data(mma7660_client, RegAdd);
		if (res < 0){
			return res;
		}
 	} while (res & 0x40);			// while Alert bit = 1;
	
	return res;
}


/* read sensor data from mma7660 */
static int mma7660_read_data(short *x, short *y, short *z, short *tilt) {
	u8	tmp_data[4];
	s32	res;

	res = (i2c_smbus_read_i2c_block_data(mma7660_client,REG_XOUT,REG_TILT-REG_XOUT,tmp_data) < REG_TILT-REG_XOUT);

	if  (res < 0){
		dev_err(&mma7660_client->dev, "i2c block read failed\n");
			return res;
	}

	*x=tmp_data[0];
	*y=tmp_data[1];
	*z=tmp_data[2];
	*tilt=tmp_data[3];

	if (*x & 0x40){
		res = mma7660_Read_Alert(REG_XOUT);
		if (res < 0){
			return res;
		}
		*x = res;
	}

	if (*y & 0x40){	
		res = mma7660_Read_Alert(REG_YOUT);
		if (res < 0){
			return res;
		}
		*y = res;
	}

	if (*z & 0x40){	
		res = mma7660_Read_Alert(REG_ZOUT);
		if (res < 0){
			return res;
		}
		*z = res;
	}

	if (*tilt * 0x40){
		res = mma7660_Read_Alert(REG_TILT);
		if (res < 0){
			return res;
		}
		*tilt = res;
	}

	if (*x & 0x20) {	/* check for the bit 5 */
		*x |= 0xffc0;
	}

	if (*y & 0x20) {	/* check for the bit 5 */
		*y |= 0xffc0;
	}

	if (*z & 0x20) {	/* check for the bit 5 */
		*z |= 0xffc0;
	}

	return 0;
}


/* parse command argument */
static void parse_arg(const char *arg, int *reg, int *value)
{
	const char *p;

	for (p = arg;; p++) {
		if (*p == ' ' || *p == '\0')
			break;
	}

	p++;

	*reg 	= simple_strtoul(arg, NULL, 16);
	*value 	= simple_strtoul(p, NULL, 16);
}

static void cmd_read_reg(const char *arg)
{
	int reg, value, ret;

	parse_arg(arg, &reg, &value);
	ret = i2c_smbus_read_byte_data(mma7660_client, reg);
	dev_info(&mma7660_client->dev, "read reg0x%x = %x\n", reg, ret);
	sprintf(res_buff,"OK:read reg 0x%02x = 0x%02x\n",reg,ret);
}

/* write register command */
static void cmd_write_reg(const char *arg)
{
	int reg, value, ret, modereg;

	parse_arg(arg, &reg, &value);

	modereg = i2c_smbus_read_byte_data(mma7660_client, REG_MODE);

	ret = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg & (~0x01));
	ret = i2c_smbus_write_byte_data(mma7660_client, reg, value);
	ret = i2c_smbus_write_byte_data(mma7660_client, REG_MODE, modereg);

	dev_info(&mma7660_client->dev, "write reg result %s\n",
		 ret ? "failed" : "success");
	sprintf(res_buff, "OK:write reg 0x%02x data 0x%02x result %s\n",reg,value,
		 ret ? "failed" : "success");
}

/* dump register command */
static void cmd_dumpreg(void) {
	int reg,ret;

	sprintf(res_buff,"OK:dumpreg result:\n");

	for (reg = REG_XOUT; reg <= REG_END; reg++) {
		ret = i2c_smbus_read_byte_data(mma7660_client, reg);
		sprintf(&(res_buff[strlen(res_buff)]),"%02x ",ret);
		if ((reg % 16) == 15) {
			sprintf(&(res_buff[strlen(res_buff)]),"\n");
		}
	}

	sprintf(&(res_buff[strlen(res_buff)]),"\n");
}

/* set the Zslider key mapping */
static void cmd_sliderkey(const char *arg) 
{
	unsigned char key_code[4];
	int i;
	char *endptr,*p;

	p = (char *)arg;

	for ( i = 0; i < 4 ; i++ ) {
		if (*p == '\0') {
			break;
		}

		key_code[i] = (char)simple_strtoul(p,&endptr,16);
		p = endptr +1;

	}

	if (i != 4) {
		sprintf (res_buff,"FAIL:sliderkey command failed,only %d args provided\n",i);
		DBG ("%s: Failed to set slider key, not enough key code in command\n",__FUNCTION__);
		return;
	}


	DBG("%s: set slider key code  %02x %02x %02x %02x \n",__FUNCTION__,
		key_code[0],key_code[1],key_code[2],key_code[3]);


	for (i = 0; i < 4; i++) {
		mma_status.slider_key[i] = key_code[i];
	}

	sprintf(res_buff,"OK:set slider key ok, key code %02x %02x %02x %02x \n",
		key_code[0],key_code[1],key_code[2],key_code[3]);
}

/* remap the axis */
static void cmd_remap_axis(const char *arg,size_t count) 
{
	int 	buff_len; 	
	char	delimiters[] = " ,";

	char 	*token = NULL;
	
	int 	axis_cnt = 0;
	u8	axis_map[3];
	
	if (count > 63) {
		buff_len = 63;
	} else {
		buff_len = count;
	}

	while ((token = strsep((char **)&arg,delimiters)) != NULL) {

		if (strcmp(token,"")) {
			if (strcasecmp(token,"x") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_X;
			} else if (strcasecmp(token,"-x") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_X_N;
			} else if (strcasecmp(token,"y") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_Y;
			} else if (strcasecmp(token,"-y") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_Y_N;
			} else if (strcasecmp(token,"z") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_Z;
			} else if (strcasecmp(token,"-z") == 0) {
				axis_map[axis_cnt] = AXIS_DIR_Z_N;
			} else {
				sprintf (res_buff,"FAIL:remapaxis error,wrong argument\n");
				return;
			}

			axis_cnt ++;

			if (axis_cnt == 3) {
				break;
			}
		}
	}

	if (axis_cnt != 3) {
		sprintf(res_buff,"FAIL:remapaxis error, not enough parameters(%d)\n",axis_cnt);
		return;
	}


	if (((axis_map[0] & 0xfe) == (axis_map[1] & 0xfe)) || 
		((axis_map[0] & 0xfe) == (axis_map[2] & 0xfe)) ||
		((axis_map[2] & 0xfe) == (axis_map[1] & 0xfe))) {

		sprintf(res_buff,"FAIL:remapaxis error, duplicate axis mapping\n");
		return;
	}


	mma_status.axis_dir_x	= axis_map[0];
	mma_status.axis_dir_y	= axis_map[1];
	mma_status.axis_dir_z	= axis_map[2];

	sprintf(res_buff,"OK:remapaxis ok, new mapping : %s %s %s\n",axis_dir[mma_status.axis_dir_x],
		axis_dir[mma_status.axis_dir_y],axis_dir[mma_status.axis_dir_z]);
}

/* parse the command passed into driver, and execute it */
static int exec_command(const char *buf, size_t count)
{
	const char *p, *s;
	const char *arg;
	int i, value = 0;

	for (p = buf;; p++) {
		if (*p == ' ' || *p == '\0' || p - buf >= count)
			break;
	}
	arg = p + 1;

	for (i = MMA_REG_R; i < MMA_CMD_MAX; i++) {
		s = command[i];
		if (s && !strncmp(buf, s, p - buf - 1)) {
			dev_info(&mma7660_client->dev, "command %s\n", s);
			goto mma_exec_command;
		}
	}

	dev_err(&mma7660_client->dev, "command not found\n");
	sprintf(res_buff,"FAIL:command [%s] not found\n",s);
	return -1;

mma_exec_command:
	if (i != MMA_REG_R && i != MMA_REG_W)
		value = simple_strtoul(arg, NULL, 16);

	switch (i) {
	case MMA_REG_R:
		cmd_read_reg(arg);
		break;
	case MMA_REG_W:
		cmd_write_reg(arg);
		break;
	case MMA_DUMPREG:				/* dump registers	*/
		cmd_dumpreg();
		break;
	case MMA_REMAP_AXIS:			/* remap 3 axis's direction	*/
		cmd_remap_axis(arg,(count - (arg - buf)));
		break;
	case MMA_DUMPAXIS:				/* remap 3 axis's direction	*/
		sprintf(res_buff,"OK:dumpaxis : %s %s %s\n",axis_dir[mma_status.axis_dir_x],
			axis_dir[mma_status.axis_dir_y],axis_dir[mma_status.axis_dir_z]);
		break;
	case MMA_ENJOYSTICK: 
		sprintf(res_buff,"OK:joystick movement emulation enabled\n");
		break;
	case MMA_DISJOYSTICK:
		sprintf(res_buff,"OK:joystick movement emulation disabled\n");
		break;
	case MMA_SLIDER_KEY:			/* load offset drift registers	*/
		cmd_sliderkey(arg);
		break;
	case MMA_DUMP_SLKEY:
		sprintf(res_buff,"OK:dumpslkey 0x%02x 0x%02x 0x%02x 0x%02x\n",mma_status.slider_key[0],
			mma_status.slider_key[1],mma_status.slider_key[2],mma_status.slider_key[3]);
		break;
	default:
		dev_err(&mma7660_client->dev, "command is not found\n");
		sprintf (res_buff,"FAIL:no such command %d\n",i);
		break;
	}

	return 0;
}

/* show the command result */
static ssize_t mma7660_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%s",res_buff);
}

/* accept the command */
static ssize_t mma7660_store(struct device *dev,struct device_attribute *attr, 
		const char *buf,size_t count)
{
	exec_command(buf, count);

	return count;
}

typedef enum {NOSLIDE, UPWARD, DOWNWARD} SLIDEOUT;

#define NOSLIDE		0
#define SLIDEUPWARD	1
#define SLIDEUPWARD2	2
#define SLIDEDOWNWARD 	3
#define SLIDEDOWNWARD2	4

#define MAXSLIDEWIDTH 	10
#define SLIDERAVCOUNT 	4

#define SLIDE_THR	16
#define SLIDE_THR2	(36)

struct slider {
	int 		wave[SLIDERAVCOUNT];	/* for long term average */
	unsigned char	wave_idx;
	short		ref;
	unsigned char 	dir;
	unsigned char	report;
	unsigned char 	mode;
	unsigned char	cnt;	
};

/* workqueue function to poll mma7660 data */
static void mma7660_poll_work(struct work_struct *work)  
{
    short 	temp;
	short 	x,y,z,tilt;

	if (mma7660_read_data(&x, &y, &z, &tilt) != 0) {
		DBG("mma7660 data read failed\n");
		return;
	}

// namko: The orientation is 90 degrees off the mark. Hence swap
// x and y.
    temp = x;
    x = y;
    y = temp;

/* report the absulate sensor data to input device */
    input_report_abs(mma7660_dev, ABS_X, x);	
    input_report_abs(mma7660_dev, ABS_Y, y);	
    input_report_abs(mma7660_dev, ABS_Z, z);
    input_report_abs(mma7660_dev, ABS_MISC, tilt);
    input_sync(mma7660_dev);

// Schedule next update.
    queue_delayed_work(mma7660_workqueue, &mma7660_work, HZ * POLL_INTERVAL / 1000);

	return;

}

/* detecting mma7660 chip */
static int mma7660_detect(struct i2c_client *client, struct i2c_board_info *info) {
        struct i2c_adapter *adapter = client->adapter;

        if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE
                                     | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
                return -ENODEV;                                        

       strlcpy(info->type, "mma7660", I2C_NAME_SIZE);

        return 0;
}

static int mma7660_probe(struct i2c_client *client,const struct i2c_device_id *id) {
	int 			rc; 
	int 			ret;

	printk("*** %s ***\n", __func__);
	printk(KERN_INFO "try to detect mma7660 chip id %02x\n",client->addr);

    if (!(mma7660_workqueue = create_singlethread_workqueue(DEVICE_NAME))) 
        return -ENOMEM; /* most expected reason */

	mma7660_client = client;

    /* create device file in sysfs as user interface */
	if ((ret = device_create_file(&client->dev, &mma7660_dev_attr))) {
		dev_err(&client->dev, "create device file failed!\n");
		rc = -EINVAL;
		goto err_detach_client;
	}

    /* allocate & register input polling device  */
	mma7660_dev = input_allocate_device();

	if (!mma7660_dev) {
		dev_err(&client->dev, "allocate poll device failed!\n");
		rc = -ENOMEM;
		goto err_free_abs;
	}

	mma7660_dev->name		= DEVICE_NAME;
	mma7660_dev->id.bustype	= BUS_I2C;
	mma7660_dev->dev.parent	= &client->dev;

	set_bit(EV_REL,mma7660_dev->evbit);
	set_bit(EV_KEY,mma7660_dev->evbit);

	set_bit(EV_ABS,mma7660_dev->evbit);
	set_bit(ABS_X,mma7660_dev->absbit);
	set_bit(ABS_Y,mma7660_dev->absbit);
	set_bit(ABS_Z,mma7660_dev->absbit);
	set_bit(ABS_MISC,mma7660_dev->absbit);

	ret = input_register_device(mma7660_dev);
	if (ret) {
		dev_err(&client->dev, "register abs device failed!\n");
		rc = -EINVAL;
		goto err_unreg_abs_input;
	}

	memset(res_buff,0,RES_BUFF_LEN);

/* enable gSensor mode 8g, measure */
	i2c_smbus_write_byte_data(client, REG_MODE, 0x00);		/* standby mode   */
	i2c_smbus_write_byte_data(client, REG_SPCNT, 0x00);		/* no sleep count */
	i2c_smbus_write_byte_data(client, REG_INTSU, 0x00);		/* no interrupt   */
	i2c_smbus_write_byte_data(client, REG_PDET, 0xE0);		/* disable tap    */
	i2c_smbus_write_byte_data(client, REG_SR, 0x6B);		/* 4 measurement, 16 sample  */
	i2c_smbus_write_byte_data(client, REG_MODE, 0x01);		/* active mode   */

    // init delayed work struct and workqueue.
    INIT_DELAYED_WORK(&mma7660_work, mma7660_poll_work);
    queue_delayed_work(mma7660_workqueue, &mma7660_work, 0);

	return 0;
err_unreg_abs_input:
	input_unregister_device(mma7660_dev);
err_free_abs:
	input_free_device(mma7660_dev);

err_detach_client:
	destroy_workqueue(mma7660_workqueue);

	kfree(client);
	return rc; 
}


static int mma7660_remove(struct i2c_client *client)  {
	printk("*** %s ***\n", __func__);

	input_unregister_device(mma7660_dev);
	input_free_device(mma7660_dev);

	device_remove_file(&client->dev, &mma7660_dev_attr);

	kfree(i2c_get_clientdata(client));
	flush_workqueue(mma7660_workqueue);
	destroy_workqueue(mma7660_workqueue);

	DBG("MMA7660 device detatched\n");	
        return 0;
}


static int mma7660_suspend(struct i2c_client *client, pm_message_t state)
{
	printk("*** %s ***\n", __func__);
	mma_status.mode = i2c_smbus_read_byte_data(mma7660_client, REG_MODE);
	i2c_smbus_write_byte_data(mma7660_client, REG_MODE,mma_status.mode & ~0x3);
	return 0;
}

static int mma7660_resume(struct i2c_client *client)
{
	printk("*** %s ***\n", __func__);
	i2c_smbus_write_byte_data(mma7660_client, REG_MODE, mma_status.mode);
	return 0;
}

static int __init init_mma7660(void)
{
/* register driver */
	int res;

	printk("*** %s ***\n", __func__);
	res = i2c_add_driver(&mma7660_driver);
	if (res < 0){
		printk(KERN_INFO "add mma7660 i2c driver failed\n");
		return -ENODEV;
	}
	printk(KERN_INFO "add mma7660 i2c driver\n");

	return (res);
}

static void __exit exit_mma7660(void)
{
	printk("*** %s ***\n", __func__);
	return i2c_del_driver(&mma7660_driver);
}


module_init(init_mma7660);
module_exit(exit_mma7660);

module_param(debug, bool, S_IRUGO | S_IWUSR);
module_param(poll_int, int, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(debug, "1: Enable verbose debugging messages");
MODULE_PARM_DESC(poll_int, "set the poll interval of gSensor data (unit: ms)");

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MMA7660 sensor driver");
MODULE_LICENSE("GPL");
