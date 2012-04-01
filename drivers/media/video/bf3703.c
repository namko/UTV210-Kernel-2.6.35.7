/*
 * A V4L2 driver for OmniVision bf3703 cameras.
 *
 * Copyright 2006 One Laptop Per Child Association, Inc.  Written
 * by Jonathan Corbet with substantial inspiration from Mark
 * McClelland's ovcamchip code.
 *
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 *
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h> //lfj 0509,for kzalloc and kfree 
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-i2c-drv.h>
#ifdef CONFIG_VIDEO_SAMSUNG_V4L2  //lfj 0510
#include <linux/videodev2_samsung.h>
#endif


MODULE_AUTHOR("Jonathan Corbet <corbet@lwn.net>");
MODULE_DESCRIPTION("A low-level driver for BYD bf3703 sensors");
MODULE_LICENSE("GPL");

static int debug;
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

//Add debug info control
#if 0//by lfj, for 7670 debug
#define bf3703_dbg(fmt, ...)		printk(fmt, ##__VA_ARGS__)
#define bf3703_info(fmt, ...)		printk(fmt, ##__VA_ARGS__)
#define bf3703_err(fmt, ...)		printk(fmt, ##__VA_ARGS__)
#else
#define bf3703_dbg(fmt, ...)		printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#define bf3703_info(fmt, ...)		printk(KERN_INFO fmt, ##__VA_ARGS__)
#define bf3703_err(fmt, ...)		printk(KERN_ERR fmt, ##__VA_ARGS__)
#endif
/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define VGA_WIDTH	640
#define VGA_HEIGHT	480
#define QVGA_WIDTH	320
#define QVGA_HEIGHT	240
#define CIF_WIDTH	352
#define CIF_HEIGHT	288
#define QCIF_WIDTH	176
#define	QCIF_HEIGHT	144

/*
 * Our nominal (default) frame rate.
 */
#define bf3703_FRAME_RATE 30

/*
 * The 7670 sits on i2c with ID 0x42
 */
#define bf3703_I2C_ADDR 0xdc

/* Registers */
#define REG_GAIN	0x86	/* Gain lower 8 bits (rest in vref) */
#define REG_BLUE	0x01	/* blue gain */
#define REG_RED		0x02	/* red gain */
#define REG_VREF	0x03	/* Pieces of GAIN, VSTART, VSTOP */
#define REG_COM1	0x04	/* Control 1 */
#define  COM1_CCIR656	  0x40  /* CCIR656 enable */
#define REG_BAVE	0x05	/* U/B Average level */
#define REG_GbAVE	0x06	/* Y/Gb Average level */
#define REG_AECHH	0x07	/* AEC MS 5 bits */
#define REG_RAVE	0x08	/* V/R Average level */
#define REG_COM2	0x09	/* Control 2 */
#define  COM2_SSLEEP	  0x10	/* Soft sleep mode */
#define REG_PID		0x0a	/* Product ID MSB */
#define REG_VER		0x0b	/* Product ID LSB */
#define REG_COM3	0x0c	/* Control 3 */
#define  COM3_SWAP	  0x40	  /* Byte swap */
#define  COM3_SCALEEN	  0x08	  /* Enable scaling */
#define  COM3_DCWEN	  0x04	  /* Enable downsamp/crop/window */
#define REG_COM4	0x0d	/* Control 4 */
#define REG_COM5	0x0e	/* All "reserved" */
#define REG_COM6	0x0f	/* Control 6 */
#define REG_AECH	0x10	/* More bits of AEC value */
#define REG_CLKRC	0x11	/* Clocl control */
#define   CLK_EXT	  0x40	  /* Use external clock directly */
#define   CLK_SCALE	  0x3f	  /* Mask for internal clock scale */
#define REG_COM7	0x12	/* Control 7 */
#define   COM7_RESET	  0x80	  /* Register reset */
#define   COM7_FMT_MASK	  0x38
#define   COM7_FMT_VGA	  0x00
#define	  COM7_FMT_CIF	  0x20	  /* CIF format */
#define   COM7_FMT_QVGA	  0x10	  /* QVGA format */
#define   COM7_FMT_QCIF	  0x08	  /* QCIF format */
#define	  COM7_RGB	  0x04	  /* bits 0 and 2 - RGB format */
#define	  COM7_YUV	  0x00	  /* YUV */
#define	  COM7_BAYER	  0x01	  /* Bayer format */
#define	  COM7_PBAYER	  0x05	  /* "Processed bayer" */
#define REG_COM8	0x13	/* Control 8 */
#define   COM8_FASTAEC	  0x80	  /* Enable fast AGC/AEC */
#define   COM8_AECSTEP	  0x40	  /* Unlimited AEC step size */
#define   COM8_BFILT	  0x20	  /* Band filter enable */
#define   COM8_AGC	  0x04	  /* Auto gain enable */
#define   COM8_AWB	  0x02	  /* White balance enable */
#define   COM8_AEC	  0x01	  /* Auto exposure enable */
#define REG_COM9	0x14	/* Control 9  - gain ceiling */
#define REG_COM10	0x15	/* Control 10 */
#define   COM10_HSYNC	  0x40	  /* HSYNC instead of HREF */
#define   COM10_PCLK_HB	  0x20	  /* Suppress PCLK on horiz blank */
#define   COM10_HREF_REV  0x08	  /* Reverse HREF */
#define   COM10_VS_LEAD	  0x04	  /* VSYNC on clock leading edge */
#define   COM10_VS_NEG	  0x02	  /* VSYNC negative */
#define   COM10_HS_NEG	  0x01	  /* HSYNC negative */
#define REG_HSTART	0x17	/* Horiz start high bits */
#define REG_HSTOP	0x18	/* Horiz stop high bits */
#define REG_VSTART	0x19	/* Vert start high bits */
#define REG_VSTOP	0x1a	/* Vert stop high bits */
#define REG_PSHFT	0x1b	/* Pixel delay after HREF */
#define REG_MIDH	0x1c	/* Manuf. ID high */
#define REG_MIDL	0x1d	/* Manuf. ID low */
#define REG_MVFP	0x1e	/* Mirror / vflip */
#define   MVFP_MIRROR	  0x20	  /* Mirror image */
#define   MVFP_FLIP	  0x10	  /* Vertical flip */

#define REG_AEW		0x24	/* AGC upper limit */
#define REG_AEB		0x25	/* AGC lower limit */
#define REG_VPT		0x26	/* AGC/AEC fast mode op region */
#define REG_HSYST	0x30	/* HSYNC rising edge delay */
#define REG_HSYEN	0x31	/* HSYNC falling edge delay */
#define REG_HREF	0x32	/* HREF pieces */
#define REG_TSLB	0x3a	/* lots of stuff */
#define   TSLB_YLAST	  0x04	  /* UYVY or VYUY - see com13 */
#define REG_COM11	0x3b	/* Control 11 */
#define   COM11_NIGHT	  0x80	  /* NIght mode enable */
#define   COM11_NMFR	  0x60	  /* Two bit NM frame rate */
#define   COM11_HZAUTO	  0x10	  /* Auto detect 50/60 Hz */
#define	  COM11_50HZ	  0x08	  /* Manual 50Hz select */
#define   COM11_EXP	  0x02
#define REG_COM12	0x3c	/* Control 12 */
#define   COM12_HREF	  0x80	  /* HREF always */
#define REG_COM13	0x3d	/* Control 13 */
#define   COM13_GAMMA	  0x80	  /* Gamma enable */
#define	  COM13_UVSAT	  0x40	  /* UV saturation auto adjustment */
#define   COM13_UVSWAP	  0x01	  /* V before U - w/TSLB */
#define REG_COM14	0x3e	/* Control 14 */
#define   COM14_DCWEN	  0x10	  /* DCW/PCLK-scale enable */
#define REG_EDGE	0x3f	/* Edge enhancement factor */
#define REG_COM15	0x40	/* Control 15 */
#define   COM15_R10F0	  0x00	  /* Data range 10 to F0 */
#define	  COM15_R01FE	  0x80	  /*            01 to FE */
#define   COM15_R00FF	  0xc0	  /*            00 to FF */
#define   COM15_RGB565	  0x10	  /* RGB565 output */
#define   COM15_RGB555	  0x30	  /* RGB555 output */
#define REG_COM16	0x41	/* Control 16 */
#define   COM16_AWBGAIN   0x08	  /* AWB gain enable */
#define REG_COM17	0x42	/* Control 17 */
#define   COM17_AECWIN	  0xc0	  /* AEC window - must match COM4 */
#define   COM17_CBAR	  0x08	  /* DSP Color bar */

/*
 * This matrix defines how the colors are generated, must be
 * tweaked to adjust hue and saturation.
 *
 * Order: v-red, v-green, v-blue, u-red, u-green, u-blue
 *
 * They are nine-bit signed quantities, with the sign bit
 * stored in 0x58.  Sign for v-red is bit 0, and up from there.
 */
#define	REG_CMATRIX_BASE 0x4f
#define   CMATRIX_LEN 6
#define REG_CMATRIX_SIGN 0x58


#define REG_BRIGHT	0x55	/* Brightness */
#define REG_CONTRAS	0x56	/* Contrast control */

#define REG_GFIX	0x69	/* Fix gain control */

#define REG_REG76	0x76	/* OV's name */
#define   R76_BLKPCOR	  0x80	  /* Black pixel correction enable */
#define   R76_WHTPCOR	  0x40	  /* White pixel correction enable */

#define REG_RGB444	0x8c	/* RGB 444 control */
#define   R444_ENABLE	  0x02	  /* Turn on RGB444, overrides 5x5 */
#define   R444_RGBX	  0x01	  /* Empty nibble at end */

#define REG_HAECC1	0x9f	/* Hist AEC/AGC control 1 */
#define REG_HAECC2	0xa0	/* Hist AEC/AGC control 2 */

#define REG_BD50MAX	0xa5	/* 50hz banding step limit */
#define REG_HAECC3	0xa6	/* Hist AEC/AGC control 3 */
#define REG_HAECC4	0xa7	/* Hist AEC/AGC control 4 */
#define REG_HAECC5	0xa8	/* Hist AEC/AGC control 5 */
#define REG_HAECC6	0xa9	/* Hist AEC/AGC control 6 */
#define REG_HAECC7	0xaa	/* Hist AEC/AGC control 7 */
#define REG_BD60MAX	0xab	/* 60hz banding step limit */


/*
 * Information we maintain about a known sensor.
 */
struct bf3703_format_struct;  /* coming later */
struct bf3703_info {
	struct v4l2_subdev sd;
	struct bf3703_format_struct *fmt;  /* Current format */
	unsigned char sat;		/* Saturation value */
	int hue;			/* Hue value */
};

static inline struct bf3703_info *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct bf3703_info, sd);
}



/*
 * The default register settings, as obtained from OmniVision.  There
 * is really no making sense of most of these - lots of "reserved" values
 * and such.
 *
 * These settings give VGA YUYV.
 */

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list bf3703_default_regs[] = {
	{ REG_COM7, COM7_RESET },
/*
 * Clock scale: 3 = 15fps
 *              2 = 20fps
 *              1 = 30fps
 */
	{ REG_CLKRC, 0x1 },	/* OV: clock scale (30 fps) */
	{ REG_TSLB,  0x04 },	/* OV */
	{ REG_COM7, 0 },	/* VGA */
	/*
	 * Set the hardware window.  These values from OV don't entirely
	 * make sense - hstop is less than hstart.  But they work...
	 */
	{ REG_HSTART, 0x13 },	{ REG_HSTOP, 0x01 },
	{ REG_HREF, 0xb6 },	{ REG_VSTART, 0x02 },
	{ REG_VSTOP, 0x7a },	{ REG_VREF, 0x0a },

	{ REG_COM3, 0 },	{ REG_COM14, 0 },
	/* Mystery scaling numbers */
	{ 0x70, 0x3a },		{ 0x71, 0x35 },
	{ 0x72, 0x11 },		{ 0x73, 0xf0 },
	{ 0xa2, 0x02 },		{ REG_COM10, 0x0 },

	/* Gamma curve values */
	{ 0x7a, 0x20 },		{ 0x7b, 0x10 },
	{ 0x7c, 0x1e },		{ 0x7d, 0x35 },
	{ 0x7e, 0x5a },		{ 0x7f, 0x69 },
	{ 0x80, 0x76 },		{ 0x81, 0x80 },
	{ 0x82, 0x88 },		{ 0x83, 0x8f },
	{ 0x84, 0x96 },		{ 0x85, 0xa3 },
	{ 0x86, 0xaf },		{ 0x87, 0xc4 },
	{ 0x88, 0xd7 },		{ 0x89, 0xe8 },

	/* AGC and AEC parameters.  Note we start by disabling those features,
	   then turn them only after tweaking the values. */
	{ REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT },
	{ REG_GAIN, 0 },	{ REG_AECH, 0 },
	{ REG_COM4, 0x40 }, /* magic reserved bit */
	{ REG_COM9, 0x18 }, /* 4x gain + magic rsvd bit */
	{ REG_BD50MAX, 0x05 },	{ REG_BD60MAX, 0x07 },
	{ REG_AEW, 0x95 },	{ REG_AEB, 0x33 },
	{ REG_VPT, 0xe3 },	{ REG_HAECC1, 0x78 },
	{ REG_HAECC2, 0x68 },	{ 0xa1, 0x03 }, /* magic */
	{ REG_HAECC3, 0xd8 },	{ REG_HAECC4, 0xd8 },
	{ REG_HAECC5, 0xf0 },	{ REG_HAECC6, 0x90 },
	{ REG_HAECC7, 0x94 },
	{ REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC },

	/* Almost all of these are magic "reserved" values.  */
	{ REG_COM5, 0x61 },	{ REG_COM6, 0x4b },
	{ 0x16, 0x02 },		{ REG_MVFP, 0x07 },
	{ 0x21, 0x02 },		{ 0x22, 0x91 },
	{ 0x29, 0x07 },		{ 0x33, 0x0b },
	{ 0x35, 0x0b },		{ 0x37, 0x1d },
	{ 0x38, 0x71 },		{ 0x39, 0x2a },
	{ REG_COM12, 0x78 },	{ 0x4d, 0x40 },
	{ 0x4e, 0x20 },		{ REG_GFIX, 0 },
	{ 0x6b, 0x4a },		{ 0x74, 0x10 },
	{ 0x8d, 0x4f },		{ 0x8e, 0 },
	{ 0x8f, 0 },		{ 0x90, 0 },
	{ 0x91, 0 },		{ 0x96, 0 },
	{ 0x9a, 0 },		{ 0xb0, 0x84 },
	{ 0xb1, 0x0c },		{ 0xb2, 0x0e },
	{ 0xb3, 0x82 },		{ 0xb8, 0x0a },

	/* More reserved magic, some of which tweaks white balance */
	{ 0x43, 0x0a },		{ 0x44, 0xf0 },
	{ 0x45, 0x34 },		{ 0x46, 0x58 },
	{ 0x47, 0x28 },		{ 0x48, 0x3a },
	{ 0x59, 0x88 },		{ 0x5a, 0x88 },
	{ 0x5b, 0x44 },		{ 0x5c, 0x67 },
	{ 0x5d, 0x49 },		{ 0x5e, 0x0e },
	{ 0x6c, 0x0a },		{ 0x6d, 0x55 },
	{ 0x6e, 0x11 },		{ 0x6f, 0x9f }, /* "9e for advance AWB" */
	{ 0x6a, 0x40 },		{ REG_BLUE, 0x40 },
	{ REG_RED, 0x60 },
	{ REG_COM8, COM8_FASTAEC|COM8_AECSTEP|COM8_BFILT|COM8_AGC|COM8_AEC|COM8_AWB },

	/* Matrix coefficients */
	{ 0x4f, 0x80 },		{ 0x50, 0x80 },
	{ 0x51, 0 },		{ 0x52, 0x22 },
	{ 0x53, 0x5e },		{ 0x54, 0x80 },
	{ 0x58, 0x9e },

	{ REG_COM16, COM16_AWBGAIN },	{ REG_EDGE, 0 },
	{ 0x75, 0x05 },		{ 0x76, 0xe1 },
	{ 0x4c, 0 },		{ 0x77, 0x01 },
	{ REG_COM13, 0xc3 },	{ 0x4b, 0x09 },
	{ 0xc9, 0x60 },		{ REG_COM16, 0x38 },
	{ 0x56, 0x40 },

	{ 0x34, 0x11 },		{ REG_COM11, COM11_EXP|COM11_HZAUTO },
	{ 0xa4, 0x88 },		{ 0x96, 0 },
	{ 0x97, 0x30 },		{ 0x98, 0x20 },
	{ 0x99, 0x30 },		{ 0x9a, 0x84 },
	{ 0x9b, 0x29 },		{ 0x9c, 0x03 },
	{ 0x9d, 0x4c },		{ 0x9e, 0x3f },
	{ 0x78, 0x04 },

	/* Extra-weird stuff.  Some sort of multiplexor register */
	{ 0x79, 0x01 },		{ 0xc8, 0xf0 },
	{ 0x79, 0x0f },		{ 0xc8, 0x00 },
	{ 0x79, 0x10 },		{ 0xc8, 0x7e },
	{ 0x79, 0x0a },		{ 0xc8, 0x80 },
	{ 0x79, 0x0b },		{ 0xc8, 0x01 },
	{ 0x79, 0x0c },		{ 0xc8, 0x0f },
	{ 0x79, 0x0d },		{ 0xc8, 0x20 },
	{ 0x79, 0x09 },		{ 0xc8, 0x80 },
	{ 0x79, 0x02 },		{ 0xc8, 0xc0 },
	{ 0x79, 0x03 },		{ 0xc8, 0x40 },
	{ 0x79, 0x05 },		{ 0xc8, 0x30 },
	{ 0x79, 0x26 },

	{ 0xff, 0xff },	/* END MARKER */
};


/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 * RGB656 and YUV422 come from OV; RGB444 is homebrewed.
 *
 * IMPORTANT RULE: the first entry must be for COM7, see bf3703_s_fmt for why.
 */


static struct regval_list bf3703_fmt_yuv422[] = {
	//OV7673 setting, June 6, 2005
	//YCbCr, VGA
	//15fps @ 24MHz input clock 
	//output format: UYVY(CbY0CrY1) (0x3a[3]=1, 0x3d[0]=1) spec is wrong.
	
	  0x11,0x80,
	  0x09,0x03, 
	  0x13,0x00, 
	  0x01,0x13, 
	  0x02,0x25, 
	  0x8c,0x02, //01 :devided by 2  02 :devided by 1
	  0x8d,0xfd, //cb: devided by 2  fd :devided by 1
	  0x87,0x1a, 
	  0x13,0x07, 

	//POLARITY of Signal
	  0x15,0x12,
	 0x3a,0x02, 		

	//black level ,对上电偏绿有改善,如果需要请选择使用
	/*
	  0x05,0x1f, 
	  0x06,0x60, 
	  0x14,0x1f 
	  0x27,0x03 
	  0x06,0xe0 
	*/

	//lens shading
	  0x35,0x68, 
	  0x65,0x68, 
	  0x66,0x62, 
	  0x36,0x05, 
	  0x37,0xf6, 
	  0x38,0x46, 
	  0x9b,0xf6, 
	  0x9c,0x46, 
	  0xbc,0x01, 
	  0xbd,0xf6, 
	  0xbe,0x46, 

	//AE
	  0x82,0x14, 
	  0x83,0x23, 
	  0x9a,0x23, //the same as 0x83
	  0x84,0x1a, 
	  0x85,0x20, 
	  0x89,0x04, //02 :devided by 2  04 :devided by 1
	  0x8a,0x08, //04: devided by 2  05 :devided by 1
	  0x86,0x28, //the same as 0x7b
	  0x96,0xa6, //AE speed
	  0x97,0x0c, //AE speed
	  0x98,0x18, //AE speed
	//AE target
	  0x24,0x7a, //灯箱测试  0x6a
	  0x25,0x8a, //灯箱测试  0x7a
	  0x94,0x0a, //INT_OPEN  
	  0x80,0x55, 

	//denoise 
	  0x70,0x6f, //denoise
	  0x72,0x4f, //denoise
	  0x73,0x2f, //denoise
	  0x74,0x27, //denoise
	  0x77,0x90, //去除格子噪声
	  0x7a,0x0e, //denoise in	  low light,0x8e\0x4e\0x0e
	  0x7b,0x28, //the same as 0x86

	//black level
	  0X1F,0x20, //G target
	  0X22,0x20, //R target
	  0X26,0x20, //B target
	//模拟部分参数
	  0X16,0x00, //如果觉得黑色物体不够黑，有点偏红，将0x16写为0x03会有点改善		
	  0xbb,0x20, 	// deglitch  对xclk整形
	  0xeb,0x30, 
	  0xf5,0x21, 
	  0xe1,0x3c, 
	  0xbb,0x20, 
	  0X2f,0X66, 
	  0x06,0xe0, 



	//anti black sun spot
	  0x61,0xd3, //0x61[3]=0 black sun disable
	  0x79,0x48, //0x79[7]=0 black sun disable

	//contrast
	  0x56,0x40, 

	//Gamma

	  0x3b,0x60, //auto gamma offset adjust in  low light	  
	  0x3c,0x20, //auto gamma offset adjust in  low light	  

	  0x39,0x80, 	  
	/*//gamma1
	  0x3f,0xb0 
	  0X40,0X88 
	  0X41,0X74 
	  0X42,0X5E 
	  0X43,0X4c 
	  0X44,0X44 
	  0X45,0X3E 
	  0X46,0X39 
	  0X47,0X35 
	  0X48,0X31 
	  0X49,0X2E 
	  0X4b,0X2B 
	  0X4c,0X29 
	  0X4e,0X25 
	  0X4f,0X22 
	  0X50,0X1F */

	/*gamma2	过曝过度好，高亮度
	  0x3f,0xb0 
	  0X40,0X9b 
	  0X41,0X88 
	  0X42,0X6e 
	  0X43,0X59 
	  0X44,0X4d 
	  0X45,0X45 
	  0X46,0X3e 
	  0X47,0X39 
	  0X48,0X35 
	  0X49,0X31 
	  0X4b,0X2e 
	  0X4c,0X2b 
	  0X4e,0X26 
	  0X4f,0X23 
	  0X50,0X1F 
	*/
	/*//gamma3 清晰亮丽 灰阶分布好
	  0X3f,0Xb0 
	  0X40,0X60 
	  0X41,0X60 
	  0X42,0X66 
	  0X43,0X57 
	  0X44,0X4c 
	  0X45,0X43 
	  0X46,0X3c 
	  0X47,0X37 
	  0X48,0X33 
	  0X49,0X2f 
	  0X4b,0X2c 
	  0X4c,0X29 
	  0X4e,0X25 
	  0X4f,0X22 
	  0X50,0X20 */

	//gamma 4   low noise   
	  0X3f,0Xa8, 
	  0X40,0X48, 
	  0X41,0X54, 
	  0X42,0X4E, 
	  0X43,0X44, 
	  0X44,0X3E, 
	  0X45,0X39, 
	  0X46,0X35, 
	  0X47,0X31, 
	  0X48,0X2E, 
	  0X49,0X2B, 
	  0X4b,0X29, 
	  0X4c,0X27, 
	  0X4e,0X23, 
	  0X4f,0X20, 
	  0X50,0X20, 


	//color matrix
	  0x51,0x0d, 
	  0x52,0x21, 
	  0x53,0x14, 
	  0x54,0x15, 
	  0x57,0x8d, 
	  0x58,0x78, 
	  0x59,0x5f, 
	  0x5a,0x84, 
	  0x5b,0x25, 
	  0x5D,0x95, 
	  0x5C,0x0e, 

	/* 

	// color	艳丽
	  0x51,0x0e 
	  0x52,0x16 
	  0x53,0x07 
	  0x54,0x1a 
	  0x57,0x9d 
	  0x58,0x82 
	  0x59,0x71 
	  0x5a,0x8d 
	  0x5b,0x1c 
	  0x5D,0x95 
	  0x5C,0x0e 
	//  

	//适中
	  0x51,0x08 
	  0x52,0x0E 
	  0x53,0x06 
	  0x54,0x12 
	  0x57,0x82 
	  0x58,0x70 
	  0x59,0x5C 
	  0x5a,0x77 
	  0x5b,0x1B 
	  0x5c,0x0e //0x5c[3:0] low light color coefficient，smaller ,lower noise
	  0x5d,0x95 


	//color 淡
	  0x51,0x03 
	  0x52,0x0d 
	  0x53,0x0b 
	  0x54,0x14 
	  0x57,0x59 
	  0x58,0x45 
	  0x59,0x41 
	  0x5a,0x5f 
	  0x5b,0x1e 
	  0x5c,0x0e //0x5c[3:0] low light color coefficient，smaller ,lower noise
	  0x5d,0x95 
	*/

	  0x60,0x20, //color open in low light 
	//AWB
	  0x6a,0x01, //如果肤色偏色，将0x6a写为0x81.
	  0x23,0x66, //Green gain
	  0xa0,0x07, //0xa0写0x03，黑色物体更红；0xa0写0x07，黑色物体更黑；

	  0xa1,0X41, //
	  0xa2,0X0e, 
	  0xa3,0X26, 
	  0xa4,0X0d, 
	//冷色调
	  0xa5,0x28, //The upper limit of red gain 

	/*暖色调
	  0xa5,0x2d 
	*/
	  0xa6,0x04, 
	  0xa7,0x80, //BLUE Target
	  0xa8,0x80, //RED Target
	  0xa9,0x28, 
	  0xaa,0x28, 
	  0xab,0x28, 
	  0xac,0x3c, 
	  0xad,0xf0, 
	  0xc8,0x18, 
	  0xc9,0x20, 
	  0xca,0x17, 
	  0xcb,0x1f, 
	  0xaf,0x00, 			
	  0xc5,0x18, 		
	  0xc6,0x00, 
	  0xc7,0x20, 		
	  0xae,0x83, //如果照户外偏蓝，将此寄存器0xae写为0x81。
	  0xcc,0x30, 
	  0xcd,0x70, 
	  0xee,0x4c, // P_TH

	// color saturation
	  0xb0,0xd0, 
	  0xb1,0xc0, 
	  0xb2,0xb0, 

	/* // 饱和度艳丽
	  0xb1,0xd0 
	  0xb2,0xc0 		
	*/
	  0xb3,0x88, 

	//anti webcamera banding
	  0x9d,0x7c, 

	   //switch direction
          0x1e,0x20,/*10 30;20 00 modified by huangyuxiang 2011.2.14*/

        0x8e,0x02, //20=26d   15=33d
	  0x8f,0x6d,	

	
	0xff, 0xff,

};


static struct regval_list bf3703_fmt_rgb565[] = {
	{ REG_COM7, COM7_RGB },	/* Selects RGB mode */
	{ REG_RGB444, 0 },	/* No RGB444 please */
	{ REG_COM1, 0x0 },
	{ REG_COM15, COM15_RGB565 },
	{ REG_COM9, 0x38 }, 	/* 16x gain ceiling; 0x8 is reserved bit */
	{ 0x4f, 0xb3 }, 	/* "matrix coefficient 1" */
	{ 0x50, 0xb3 }, 	/* "matrix coefficient 2" */
	{ 0x51, 0    },		/* vb */
	{ 0x52, 0x3d }, 	/* "matrix coefficient 4" */
	{ 0x53, 0xa7 }, 	/* "matrix coefficient 5" */
	{ 0x54, 0xe4 }, 	/* "matrix coefficient 6" */
	{ REG_COM13, COM13_GAMMA|COM13_UVSAT },
	{ 0xff, 0xff },
};

static struct regval_list bf3703_fmt_rgb444[] = {
	{ REG_COM7, COM7_RGB },	/* Selects RGB mode */
	{ REG_RGB444, R444_ENABLE },	/* Enable xxxxrrrr ggggbbbb */
	{ REG_COM1, 0x40 },	/* Magic reserved bit */
	{ REG_COM15, COM15_R01FE|COM15_RGB565 }, /* Data range needed? */
	{ REG_COM9, 0x38 }, 	/* 16x gain ceiling; 0x8 is reserved bit */
	{ 0x4f, 0xb3 }, 	/* "matrix coefficient 1" */
	{ 0x50, 0xb3 }, 	/* "matrix coefficient 2" */
	{ 0x51, 0    },		/* vb */
	{ 0x52, 0x3d }, 	/* "matrix coefficient 4" */
	{ 0x53, 0xa7 }, 	/* "matrix coefficient 5" */
	{ 0x54, 0xe4 }, 	/* "matrix coefficient 6" */
	{ REG_COM13, COM13_GAMMA|COM13_UVSAT|0x2 },  /* Magic rsvd bit */
	{ 0xff, 0xff },
};

static struct regval_list bf3703_fmt_raw[] = {
	{ REG_COM7, COM7_BAYER },
	{ REG_COM13, 0x08 }, /* No gamma, magic rsvd bit */
	{ REG_COM16, 0x3d }, /* Edge enhancement, denoise */
	{ REG_REG76, 0xe1 }, /* Pix correction, magic rsvd */
	{ 0xff, 0xff },
};



/*
 * Low-level register I/O.
 */
static inline char bf3703_i2c_read(struct i2c_client *client,unsigned char reg_addr, unsigned char *data, unsigned char len) 
{
	s32 dummy;
	if( client == NULL )	/*	No global client pointer?	*/
		return -1;

	while(len--)
	{        
		dummy = i2c_master_send(client, (char*)&reg_addr, 1);
		if(dummy < 0)
			return -1;
		dummy = i2c_master_recv(client, (char*)data, 1);
		if(dummy < 0)
			return -1;

		reg_addr++;
		data++;
	}
	return 0;
}
static inline char bf3703_i2c_write(struct i2c_client *client,unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;
	unsigned char buffer[2];
	if( client == NULL )	/*	No global client pointer?	*/
		return -1;

	while(len--)
	{
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char*)buffer, 2);
		reg_addr++;
		data++;
		if(dummy < 0)
			return -1;
	}
	return 0;
}
static int bf3703_read(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	#if 0//jmq.can't use smbus
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret >= 0) {
		*value = (unsigned char)ret;
		ret = 0;
	}
	#else
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	ret = bf3703_i2c_read(client,reg,value, 1);
	if (ret >= 0) {
		ret = 0;
	}	
	//printk("bf3703_read,reg:0x%x value:0x%x ret:%d\n",reg,*value,ret);
	return ret;
	#endif	
}


static int bf3703_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	#if 0//jmq.can't use smbus
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = i2c_smbus_write_byte_data(client, reg, value);

	if (reg == REG_COM7 && (value & COM7_RESET))
		msleep(2);  /* Wait for reset to run */
	bf3703_dbg("bf3703_write,reg:0x%x value:0x%x ret:%d\n",reg,value,ret);
	return ret;
	#else
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = bf3703_i2c_write(client, reg, &value,1);

	if (reg == REG_COM7 && (value & COM7_RESET))
		msleep(2);  /* Wait for reset to run */
	//printk("bf3703_write,reg:0x%x value:0x%x ret:%d\n",reg,value,ret);
	return ret;
	#endif
}


/*
 * Write a list of register settings; ff/ff stops the process.
 */
static int bf3703_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int i = 0;
	bf3703_dbg("%s\n",__func__);
	while (vals->reg_num != 0xff || vals->value != 0xff) {
		int ret = bf3703_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
		if (i == 0)
			mdelay(5);
		i++;
		
	}
	bf3703_dbg("%s end\n", __func__);
	return 0;
}

#if 0//debug function
static int bf3703_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	unsigned char value;
	bf3703_dbg("%s start\n",__func__);
	while (vals->reg_num != 0xff || vals->value != 0xff) {
		int ret = bf3703_read(sd, vals->reg_num, &value);
		if (ret < 0)
		{
			return ret;
		}
		vals++;
	}
	bf3703_dbg("%s end\n", __func__);
	return 0;
}
#endif
static int detected =0;//only detect once or after resume, will split with resume if need.

/*
 * Stuff that knows about the sensor.
 */
static int bf3703_reset(struct v4l2_subdev *sd, u32 val)
{
	bf3703_dbg("%s\n",__func__);
	detected = 0;
	bf3703_write(sd, REG_COM7, COM7_RESET);
	msleep(1);
	return 0;
}


static int bf3703_init(struct v4l2_subdev *sd, u32 val)
{
	bf3703_dbg("%s\n",__func__);

#if 1 //0228, only for test.origin:1
	return bf3703_write_array(sd, bf3703_fmt_yuv422);
#else
	bf3703_write_array(sd, bf3703_fmt_yuv422);
	mdelay(20);
	uint temp, size;
	unsigned char value;

	size = sizeof(bf3703_fmt_yuv422)/sizeof(struct regval_list);
	for(temp =0; temp<size; temp++)
	{
		if(bf3703_fmt_yuv422[temp].reg_num != 0xff)
		{
			bf3703_read(sd, bf3703_fmt_yuv422[temp].reg_num,&value);
			printk("0x%x = 0x%x\n", bf3703_fmt_yuv422[temp].reg_num, value );	
		}
	}
#endif
}

static int bf3703_detect(struct v4l2_subdev *sd, u32 val)
{
	unsigned char v;
	int ret;
	//static int detected =0;//only detect once
	
	bf3703_dbg("%s\n",__func__);
	bf3703_dbg("detected = %d\n",detected);
	if(detected ==0)
	{	
		bf3703_dbg("%s\n",__func__);
		detected = 1;
		
		ret = bf3703_init(sd, val);
	if (ret < 0)
	{
		bf3703_err("bf3703_init failed!\n");
		return ret;
	}
	//v = 0;
	ret = bf3703_read(sd, REG_MIDH, &v);
	if (ret < 0)
	{
		bf3703_err("get REG_MIDH failed\n");
		return ret;
	}
	if (v != 0x7f) /* OV manuf. id. */
	{
		bf3703_err("get REG_MIDH failed2, v = 0x%x\n",v);
		return -ENODEV;
	}
	//v = 0;
	ret = bf3703_read(sd, REG_MIDL, &v);
	if (ret < 0)
	{
			bf3703_err("get REG_MIDL failed\n");
		return ret;
		}
	if (v != 0xa2)
		{
			bf3703_err("get REG_MIDL failed 2,v = 0x%d\n",v);
		return -ENODEV;
		}
	/*
	 * OK, we know we have an OmniVision chip...but which one?
	 */
	//v = 0;
	ret = bf3703_read(sd, 0xfc, &v);
	if (ret < 0)
		{			bf3703_err("get REG_PID failed \n");
		return ret;
		}
	if (v != 0x37)  /* PID + VER = 0x37 / 0x03 */
		{bf3703_err("get REG_PID failed 2\n");
		return -ENODEV;}
	//v = 0;
	ret = bf3703_read(sd, 0xfd, &v);
	if (ret < 0)
		{bf3703_err("get REG_VER failed \n");
		return ret;}
	if (v != 0x03)  /* PID + VER = 0x37 / 0x03 */
		{bf3703_err("get REG_VER failed 2\n");
		return -ENODEV;}
		detected = 2;
		return 0;
	}	else if(detected == 2)
	{
		return 0;//already detected
	}else
	{
		return -ENODEV;
	}
}


/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
static struct bf3703_format_struct {
	__u8 *desc;
	__u32 pixelformat;
	struct regval_list *regs;
	int cmatrix[CMATRIX_LEN];
	int bpp;   /* Bytes per pixel */
} bf3703_formats[] = {
	{
		.desc		= "YUYV 4:2:2",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.regs 		= bf3703_fmt_yuv422,
		.cmatrix	= { 128, -128, 0, -34, -94, 128 },
		.bpp		= 2,
	},
	{
		.desc		= "RGB 444",
		.pixelformat	= V4L2_PIX_FMT_RGB444,
		.regs		= bf3703_fmt_rgb444,
		.cmatrix	= { 179, -179, 0, -61, -176, 228 },
		.bpp		= 2,
	},
	{
		.desc		= "RGB 565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.regs		= bf3703_fmt_rgb565,
		.cmatrix	= { 179, -179, 0, -61, -176, 228 },
		.bpp		= 2,
	},
	{
		.desc		= "Raw RGB Bayer",
		.pixelformat	= V4L2_PIX_FMT_SBGGR8,
		.regs 		= bf3703_fmt_raw,
		.cmatrix	= { 0, 0, 0, 0, 0, 0 },
		.bpp		= 1
	},
};
#define N_bf3703_FMTS ARRAY_SIZE(bf3703_formats)


/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

/*
 * QCIF mode is done (by OV) in a very strange way - it actually looks like
 * VGA with weird scaling options - they do *not* use the canned QCIF mode
 * which is allegedly provided by the sensor.  So here's the weird register
 * settings.
 */
static struct regval_list bf3703_qcif_regs[] = {
	{ REG_COM3, COM3_SCALEEN|COM3_DCWEN },
	{ REG_COM3, COM3_DCWEN },
	{ REG_COM14, COM14_DCWEN | 0x01},
	{ 0x73, 0xf1 },
	{ 0xa2, 0x52 },
	{ 0x7b, 0x1c },
	{ 0x7c, 0x28 },
	{ 0x7d, 0x3c },
	{ 0x7f, 0x69 },
	{ REG_COM9, 0x38 },
	{ 0xa1, 0x0b },
	{ 0x74, 0x19 },
	{ 0x9a, 0x80 },
	{ 0x43, 0x14 },
	{ REG_COM13, 0xc0 },
	{ 0xff, 0xff },
};

static struct bf3703_win_size {
	int	width;
	int	height;
	unsigned char com7_bit;
	int	hstart;		/* Start/stop values for the camera.  Note */
	int	hstop;		/* that they do not always make complete */
	int	vstart;		/* sense to humans, but evidently the sensor */
	int	vstop;		/* will do the right thing... */
	struct regval_list *regs; /* Regs to tweak */
/* h/vref stuff */
} bf3703_win_sizes[] = {
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.com7_bit	= COM7_FMT_VGA,
		.hstart		= 0,		/* These values from */
		.hstop		=  640,		/* Omnivision */
		.vstart		=  0,
		.vstop		= 480,
		.regs 		= NULL,
	},
	/* CIF */
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
		.com7_bit	= COM7_FMT_CIF,
		.hstart		= 170,		/* Empirically determined */
		.hstop		=  90,
		.vstart		=  14,
		.vstop		= 494,
		.regs 		= NULL,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
		.com7_bit	= COM7_FMT_QVGA,
		.hstart		= 164,		/* Empirically determined */
		.hstop		=  20,
		.vstart		=  14,
		.vstop		= 494,
		.regs 		= NULL,
	},
	/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
		.com7_bit	= COM7_FMT_VGA, /* see comment above */
		.hstart		= 456,		/* Empirically determined */
		.hstop		=  24,
		.vstart		=  14,
		.vstop		= 494,
		.regs 		= NULL//bf3703_qcif_regs,//set it to null temp, QCIF ->VGA has some problem.
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(bf3703_win_sizes))


/*
 * Store a set of start/stop values into the camera.
 */
static int bf3703_set_hw(struct v4l2_subdev *sd, int hstart, int hstop,
		int vstart, int vstop)
{
	int ret;
	unsigned char v;
	bf3703_dbg("%s\n",__func__);
	
/*
 * Horizontal: 11 bits, top 8 live in hstart and hstop.  Bottom 3 of
 * hstart are in href[2:0], bottom 3 of hstop in href[5:3].  There is
 * a mystery "edge offset" value in the top two bits of href.
 */
	ret =  bf3703_write(sd, REG_HSTART, (hstart >> 3) & 0xff);
	ret += bf3703_write(sd, REG_HSTOP, (hstop >> 3) & 0xff);
	ret += bf3703_read(sd, 0x03, &v);
	v = (v & 0xc0) | ((hstop & 0x7) << 3) | (hstart & 0x7);
	msleep(10);
	ret += bf3703_write(sd, 0x03, v);
/*
 * Vertical: similar arrangement, but only 10 bits.
 */
	ret += bf3703_write(sd, REG_VSTART, (vstart >> 2) & 0xff);
	ret += bf3703_write(sd, REG_VSTOP, (vstop >> 2) & 0xff);
	ret += bf3703_read(sd, REG_VREF, &v);
	v = (v & 0xf0) | ((vstop & 0x3) << 2) | (vstart & 0x3);
	msleep(10);
	ret += bf3703_write(sd, REG_VREF, v);
	return ret;
}


static int bf3703_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmt)
{
	struct bf3703_format_struct *ofmt;
	bf3703_dbg("%s\n",__func__);
	if (fmt->index >= N_bf3703_FMTS)
		return -EINVAL;

	ofmt = bf3703_formats + fmt->index;
	bf3703_dbg("fmt->index = %d\n",fmt->index);
	fmt->flags = 0;
	strcpy(fmt->description, ofmt->desc);
	fmt->pixelformat = ofmt->pixelformat;
	return 0;
}


static int bf3703_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_format *fmt,
		struct bf3703_format_struct **ret_fmt,
		struct bf3703_win_size **ret_wsize)
{
	int index;
	struct bf3703_win_size *wsize;
	struct v4l2_pix_format *pix = &fmt->fmt.pix;
	bf3703_dbg("%s\n",__func__);
	
	bf3703_dbg("try fmt = 0x%x\n",pix->pixelformat);

	for (index = 0; index < N_bf3703_FMTS; index++)
		if (bf3703_formats[index].pixelformat == pix->pixelformat)
			break;
	if (index >= N_bf3703_FMTS) {
		/* default to first format */
		index = 0;
		pix->pixelformat = bf3703_formats[0].pixelformat;
	}
	if (ret_fmt != NULL)
		*ret_fmt = bf3703_formats + index;
	/*
	 * Fields: the OV devices claim to be progressive.
	 */
	pix->field = V4L2_FIELD_NONE;
	/*
	 * Round requested image size down to the nearest
	 * we support, but not below the smallest.
	 */
	bf3703_dbg("Input Pix format(w:%d h:%d bytesperline:%d sizeimage:%d)\n"
		,pix->width,pix->height,pix->bytesperline,pix->sizeimage);	 
	for (wsize = bf3703_win_sizes; wsize < bf3703_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (pix->width >= wsize->width && pix->height >= wsize->height)
			break;
	if (wsize >= bf3703_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	/*
	 * Note the size we'll actually handle.
	 */
	pix->width = wsize->width;
	pix->height = wsize->height;
	pix->bytesperline = pix->width*bf3703_formats[index].bpp;
	pix->sizeimage = pix->height*pix->bytesperline;
	bf3703_dbg("Pix format(w:%d h:%d bytesperline:%d sizeimage:%d)\n"
		,pix->width,pix->height,pix->bytesperline,pix->sizeimage);
	return 0;
}

static int bf3703_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	bf3703_dbg("%s\n",__func__);
	return bf3703_try_fmt_internal(sd, fmt, NULL, NULL);
}

/*
 * Set a format.
 */
 #if 1
static int bf3703_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
 {
	int ret = 0;
	struct bf3703_format_struct *ovfmt;
	struct bf3703_win_size *wsize;
	unsigned char temp =0 ;
	
	ret =bf3703_detect(sd,0);
	bf3703_dbg("%s\n",__func__);

	if(ret < 0)
	{
		printk("bf3703_s_fmt detect failed,please check the camera hw\n");
		return ret;
	}

	ret = bf3703_try_fmt_internal(sd, fmt, &ovfmt, &wsize);
	if (ret)
	{
		printk("bf3703_try_fmt_internal failed\n");
		return ret;
	}
	bf3703_write_array(sd, ovfmt->regs);
	
	bf3703_read(sd, 0x12, &temp);
	bf3703_dbg("get com 7 = %x\n",temp);

	bf3703_read(sd, 0x3a, &temp);
	bf3703_dbg("0x3a = %x\n",temp);

	return ret;
}
 #else
static int bf3703_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int ret;
	struct bf3703_format_struct *ovfmt;
	struct bf3703_win_size *wsize;
	struct bf3703_info *info = to_state(sd);
	unsigned char com7, clkrc = 0;
	ret = bf3703_detect(sd,0);
	bf3703_dbg("%s\n",__func__);
	
	if(ret < 0)
	{
		printk("bf3703_s_fmt detect failed, 16:42\n");
		return ret;
	}

	ret = bf3703_try_fmt_internal(sd, fmt, &ovfmt, &wsize);
	if (ret)
	{
		printk("bf3703_s_fmt try fmt internal failed\n");
		return ret;
	}
	/*
	 * HACK: if we're running rgb565 we need to grab then rewrite
	 * CLKRC.  If we're *not*, however, then rewriting clkrc hoses
	 * the colors.
	 */
	if (fmt->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565) {
		ret = bf3703_read(sd, REG_CLKRC, &clkrc);
		if (ret)
		{
			printk("bf3703_s_fmt read failed\n");
			return ret;
		}
	}
	/*
	 * COM7 is a pain in the ass, it doesn't like to be read then
	 * quickly written afterward.  But we have everything we need
	 * to set it absolutely here, as long as the format-specific
	 * register sets list it first.
	 */
	com7 = ovfmt->regs[0].value;
	com7 |= wsize->com7_bit;
	bf3703_write(sd, REG_COM7, com7);
	bf3703_dbg("com 7 = %x\n",com7);
	/*
	 * Now write the rest of the array.  Also store start/stops
	 */
	bf3703_write_array(sd, ovfmt->regs + 1);
	bf3703_set_hw(sd, wsize->hstart, wsize->hstop, wsize->vstart,
			wsize->vstop);
	ret = 0;
	if (wsize->regs)
		ret = bf3703_write_array(sd, wsize->regs);
	info->fmt = ovfmt;

	if (fmt->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565 && ret == 0)
		ret = bf3703_write(sd, REG_CLKRC, clkrc);

	ret = bf3703_read(sd, REG_MIDH, &com7);
	bf3703_dbg("get com 7 = %x\n",com7);
	
	return ret;
}
#endif
/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 */
static int bf3703_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	unsigned char clkrc;
	int ret;

	bf3703_dbg("%s\n",__func__);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	ret = bf3703_read(sd, REG_CLKRC, &clkrc);
	if (ret < 0)
		return ret;
	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = bf3703_FRAME_RATE;
	if ((clkrc & CLK_EXT) == 0 && (clkrc & CLK_SCALE) > 1)
		cp->timeperframe.denominator /= (clkrc & CLK_SCALE);
	return 0;
}

static int bf3703_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct v4l2_fract *tpf = &cp->timeperframe;
	unsigned char clkrc;
	int ret, div;
	bf3703_dbg("%s\n",__func__);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
	{
		printk("parms->type:%d",parms->type);
		return -EINVAL;
	}
	#if 0//don't check
	if (cp->extendedmode != 0)
	{
		printk("cp->extendedmode:%d",cp->extendedmode);
		return -EINVAL;
	}
	#endif
	/*
	 * CLKRC has a reserved bit, so let's preserve it.
	 */
	ret = bf3703_read(sd, REG_CLKRC, &clkrc);
	if (ret < 0)
		return ret;
	if (tpf->numerator == 0 || tpf->denominator == 0)
		div = 1;  /* Reset to full rate */
	else
		div = (tpf->numerator*bf3703_FRAME_RATE)/tpf->denominator;
	if (div == 0)
		div = 1;
	else if (div > CLK_SCALE)
		div = CLK_SCALE;
	clkrc = (clkrc & 0x80) | div;
	tpf->numerator = 1;
	tpf->denominator = bf3703_FRAME_RATE/div;
	return bf3703_write(sd, REG_CLKRC, clkrc);
}



/*
 * Code for dealing with controls.
 */





static int bf3703_store_cmatrix(struct v4l2_subdev *sd,
		int matrix[CMATRIX_LEN])
{
	int i, ret;
	unsigned char signbits = 0;
	bf3703_dbg("%s\n",__func__);

	/*
	 * Weird crap seems to exist in the upper part of
	 * the sign bits register, so let's preserve it.
	 */
	ret = bf3703_read(sd, REG_CMATRIX_SIGN, &signbits);
	signbits &= 0xc0;

	for (i = 0; i < CMATRIX_LEN; i++) {
		unsigned char raw;

		if (matrix[i] < 0) {
			signbits |= (1 << i);
			if (matrix[i] < -255)
				raw = 0xff;
			else
				raw = (-1 * matrix[i]) & 0xff;
		}
		else {
			if (matrix[i] > 255)
				raw = 0xff;
			else
				raw = matrix[i] & 0xff;
		}
		ret += bf3703_write(sd, REG_CMATRIX_BASE + i, raw);
	}
	ret += bf3703_write(sd, REG_CMATRIX_SIGN, signbits);
	return ret;
}


/*
 * Hue also requires messing with the color matrix.  It also requires
 * trig functions, which tend not to be well supported in the kernel.
 * So here is a simple table of sine values, 0-90 degrees, in steps
 * of five degrees.  Values are multiplied by 1000.
 *
 * The following naive approximate trig functions require an argument
 * carefully limited to -180 <= theta <= 180.
 */
#define SIN_STEP 5
static const int bf3703_sin_table[] = {
	   0,	 87,   173,   258,   342,   422,
	 499,	573,   642,   707,   766,   819,
	 866,	906,   939,   965,   984,   996,
	1000
};

static int bf3703_sine(int theta)
{
	int chs = 1;
	int sine;

	if (theta < 0) {
		theta = -theta;
		chs = -1;
	}
	if (theta <= 90)
		sine = bf3703_sin_table[theta/SIN_STEP];
	else {
		theta -= 90;
		sine = 1000 - bf3703_sin_table[theta/SIN_STEP];
	}
	return sine*chs;
}

static int bf3703_cosine(int theta)
{
	theta = 90 - theta;
	if (theta > 180)
		theta -= 360;
	else if (theta < -180)
		theta += 360;
	return bf3703_sine(theta);
}




static void bf3703_calc_cmatrix(struct bf3703_info *info,
		int matrix[CMATRIX_LEN])
{
	int i;
	/*
	 * Apply the current saturation setting first.
	 */
	for (i = 0; i < CMATRIX_LEN; i++)
		matrix[i] = (info->fmt->cmatrix[i]*info->sat) >> 7;
	/*
	 * Then, if need be, rotate the hue value.
	 */
	if (info->hue != 0) {
		int sinth, costh, tmpmatrix[CMATRIX_LEN];

		memcpy(tmpmatrix, matrix, CMATRIX_LEN*sizeof(int));
		sinth = bf3703_sine(info->hue);
		costh = bf3703_cosine(info->hue);

		matrix[0] = (matrix[3]*sinth + matrix[0]*costh)/1000;
		matrix[1] = (matrix[4]*sinth + matrix[1]*costh)/1000;
		matrix[2] = (matrix[5]*sinth + matrix[2]*costh)/1000;
		matrix[3] = (matrix[3]*costh - matrix[0]*sinth)/1000;
		matrix[4] = (matrix[4]*costh - matrix[1]*sinth)/1000;
		matrix[5] = (matrix[5]*costh - matrix[2]*sinth)/1000;
	}
}



static int bf3703_s_sat(struct v4l2_subdev *sd, int value)
{
	struct bf3703_info *info = to_state(sd);
	int matrix[CMATRIX_LEN];
	int ret;
	bf3703_dbg("%s\n",__func__);

	info->sat = value;
	bf3703_calc_cmatrix(info, matrix);
	ret = bf3703_store_cmatrix(sd, matrix);
	return ret;
}

static int bf3703_g_sat(struct v4l2_subdev *sd, __s32 *value)
{
	struct bf3703_info *info = to_state(sd);
	bf3703_dbg("%s\n",__func__);

	*value = info->sat;
	return 0;
}

static int bf3703_s_hue(struct v4l2_subdev *sd, int value)
{
	struct bf3703_info *info = to_state(sd);
	int matrix[CMATRIX_LEN];
	int ret;
	bf3703_dbg("%s\n",__func__);

	if (value < -180 || value > 180)
		return -EINVAL;
	info->hue = value;
	bf3703_calc_cmatrix(info, matrix);
	ret = bf3703_store_cmatrix(sd, matrix);
	return ret;
}


static int bf3703_g_hue(struct v4l2_subdev *sd, __s32 *value)
{
	struct bf3703_info *info = to_state(sd);
	bf3703_dbg("%s\n",__func__);

	*value = info->hue;
	return 0;
}


/*
 * Some weird registers seem to store values in a sign/magnitude format!
 */
static unsigned char bf3703_sm_to_abs(unsigned char v)
{
	bf3703_dbg("%s\n",__func__);

	if ((v & 0x80) == 0)
		return v + 128;
	return 128 - (v & 0x7f);
}


static unsigned char bf3703_abs_to_sm(unsigned char v)
{
	bf3703_dbg("%s\n",__func__);

	if (v > 127)
		return v & 0x7f;
	return (128 - v) | 0x80;
}

static int bf3703_s_brightness(struct v4l2_subdev *sd, int value)
{
	unsigned char com8 = 0, v;
	int ret;
	bf3703_dbg("%s\n",__func__);

	bf3703_read(sd, REG_COM8, &com8);
	com8 &= ~COM8_AEC;
	bf3703_write(sd, REG_COM8, com8);
	v = bf3703_abs_to_sm(value);
	ret = bf3703_write(sd, REG_BRIGHT, v);
	return ret;
}

static int bf3703_g_brightness(struct v4l2_subdev *sd, __s32 *value)
{
	unsigned char v = 0;
	int ret = bf3703_read(sd, REG_BRIGHT, &v);
	bf3703_dbg("%s\n",__func__);

	*value = bf3703_sm_to_abs(v);
	return ret;
}

static int bf3703_s_contrast(struct v4l2_subdev *sd, int value)
{
	bf3703_dbg("%s\n",__func__);

	return bf3703_write(sd, REG_CONTRAS, (unsigned char) value);
}

static int bf3703_g_contrast(struct v4l2_subdev *sd, __s32 *value)
{
	unsigned char v = 0;
	int ret = bf3703_read(sd, REG_CONTRAS, &v);
	bf3703_dbg("%s\n",__func__);

	*value = v;
	return ret;
}

static int bf3703_g_hflip(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	unsigned char v = 0;
	bf3703_dbg("%s\n",__func__);

	ret = bf3703_read(sd, REG_MVFP, &v);
	*value = (v & MVFP_MIRROR) == MVFP_MIRROR;
	return ret;
}


static int bf3703_s_hflip(struct v4l2_subdev *sd, int value)
{
	unsigned char v = 0;
	int ret;
	bf3703_dbg("%s\n",__func__);

	ret = bf3703_read(sd, REG_MVFP, &v);
	if (value)
		v |= MVFP_MIRROR;
	else
		v &= ~MVFP_MIRROR;
	msleep(10);  /* FIXME */
	ret += bf3703_write(sd, REG_MVFP, v);
	return ret;
}



static int bf3703_g_vflip(struct v4l2_subdev *sd, __s32 *value)
{
	int ret;
	unsigned char v = 0;
	bf3703_dbg("%s\n",__func__);

	ret = bf3703_read(sd, REG_MVFP, &v);
	*value = (v & MVFP_FLIP) == MVFP_FLIP;
	return ret;
}


static int bf3703_s_vflip(struct v4l2_subdev *sd, int value)
{
	unsigned char v = 0;
	int ret;
	bf3703_dbg("%s\n",__func__);

	ret = bf3703_read(sd, REG_MVFP, &v);
	if (value)
		v |= MVFP_FLIP;
	else
		v &= ~MVFP_FLIP;
	msleep(10);  /* FIXME */
	ret += bf3703_write(sd, REG_MVFP, v);
	return ret;
}

static int bf3703_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	bf3703_dbg("%s\n",__func__);

	/* Fill in min, max, step and default value for these controls. */
	switch (qc->id) {
	case V4L2_CID_BRIGHTNESS:
		return v4l2_ctrl_query_fill(qc, 0, 255, 1, 128);
	case V4L2_CID_CONTRAST:
		return v4l2_ctrl_query_fill(qc, 0, 127, 1, 64);
	case V4L2_CID_VFLIP:
	case V4L2_CID_HFLIP:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	case V4L2_CID_SATURATION:
		return v4l2_ctrl_query_fill(qc, 0, 256, 1, 128);
	case V4L2_CID_HUE:
		return v4l2_ctrl_query_fill(qc, -180, 180, 5, 0);
	}
	return -EINVAL;
}

static int bf3703_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	bf3703_dbg("%s\n",__func__);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return bf3703_g_brightness(sd, &ctrl->value);
	case V4L2_CID_CONTRAST:
		return bf3703_g_contrast(sd, &ctrl->value);
	case V4L2_CID_SATURATION:
		return bf3703_g_sat(sd, &ctrl->value);
	case V4L2_CID_HUE:
		return bf3703_g_hue(sd, &ctrl->value);
	case V4L2_CID_VFLIP:
		return bf3703_g_vflip(sd, &ctrl->value);
	case V4L2_CID_HFLIP:
		return bf3703_g_hflip(sd, &ctrl->value);
	//lfj 0511, GB code need these param	
	case V4L2_CID_ESD_INT:
	case V4L2_CID_CAMERA_GET_SHT_TIME:
		
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
	case V4L2_CID_CAM_JPEG_MEMSIZE:
	case V4L2_CID_CAM_JPEG_QUALITY:
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
	case V4L2_CID_CAM_DATE_INFO_YEAR:
	case V4L2_CID_CAM_DATE_INFO_MONTH:
	case V4L2_CID_CAM_DATE_INFO_DATE:
	case V4L2_CID_CAM_SENSOR_VER:
	case V4L2_CID_CAM_FW_MINOR_VER:
	case V4L2_CID_CAM_FW_MAJOR_VER:
	case V4L2_CID_CAM_PRM_MINOR_VER:
	case V4L2_CID_CAM_PRM_MAJOR_VER:
	case V4L2_CID_CAMERA_GET_ISO:
	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
	case V4L2_CID_CAMERA_GET_FLASH_ONOFF:
		ctrl->value = 0;
		return 0;
		break;		
	}
	bf3703_err("%s: no such control\n", __func__);
	
	return -EINVAL;
}

static int bf3703_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	bf3703_dbg("%s\n",__func__);
#if 1 //lfj 20110322, origin 1. lfj opened 0510,for GB 
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return bf3703_s_brightness(sd, ctrl->value);
	case V4L2_CID_CONTRAST:
		return bf3703_s_contrast(sd, ctrl->value);
	case V4L2_CID_SATURATION:
		return bf3703_s_sat(sd, ctrl->value);
	case V4L2_CID_HUE:
		return bf3703_s_hue(sd, ctrl->value);
	case V4L2_CID_VFLIP:
		return bf3703_s_vflip(sd, ctrl->value);
	case V4L2_CID_HFLIP:
		return bf3703_s_hflip(sd, ctrl->value);
#if 0
	//by lfj, need to do :
	case V4L2_CID_COLORFX:  //image effect
		
	case V4L2_CID_AUTO_WHITE_BALANCE:  //white balance
	case V4L2_CID_WHITE_BALANCE_PRESET:

	case V4L2_CID_SCENE_MODE:    //scene mode

	case V4L2_CID_EXPOSURE:  //brightness, exposure

	case V4L2_CID_CONTRAST:  
	case V4L2_CID_SHARPNESS:
	case V4L2_CID_SATURATION: //not sure yet  
#endif		
	//lfj 0510
	case V4L2_CID_CAMERA_CHECK_DATALINE:
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		break;
	default:
		bf3703_err("%s: no such control\n", __func__);
		/* err = -EINVAL; */
		break;

	}
#endif
	//return -EINVAL;
	return 0;//by lfj 0510
}

static int bf3703_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	bf3703_dbg("%s\n",__func__);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_BF3703, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int bf3703_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;
	bf3703_dbg("%s\n",__func__);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = bf3703_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int bf3703_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	bf3703_dbg("%s\n",__func__);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	bf3703_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static int bf3703_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct bf3703_platform_data *pdata;
	bf3703_dbg("%s\n",__func__);
	bf3703_dbg( "fetching platform data\n");

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}
	//do nothing, if need defaut setting, get it from pdata
	return 0;
}
static int bf3703_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;
	bf3703_dbg("%s\n",__func__);

	return err;
}
static int bf3703_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;
	bf3703_dbg("%s\n",__func__);

	return err;
}

static int bf3703_enum_framesizes(struct v4l2_subdev *sd, \
					struct v4l2_frmsizeenum *fsize)
{
	int err = 0;
	bf3703_dbg("%s\n",__func__);
#if 1//lfj 0511,for GB 
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = 640;
	fsize->discrete.height = 480;
#endif
	return err;
}
static int bf3703_enum_frameintervals(struct v4l2_subdev *sd,
					struct v4l2_frmivalenum *fival)
{
	int err = 0;
	bf3703_dbg("%s\n",__func__);

	return err;
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_subdev_core_ops bf3703_core_ops = {
	.g_chip_ident = bf3703_g_chip_ident,
	.g_ctrl = bf3703_g_ctrl,
	.s_ctrl = bf3703_s_ctrl,
	.queryctrl = bf3703_queryctrl,
	.reset = bf3703_reset,
	.init = bf3703_detect,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = bf3703_g_register,
	.s_register = bf3703_s_register,
#endif
	.s_config = bf3703_s_config,	/* Fetch platform data */
};

static const struct v4l2_subdev_video_ops bf3703_video_ops = {
	.enum_fmt = bf3703_enum_fmt,
	.try_fmt = bf3703_try_fmt,
	.s_fmt = bf3703_s_fmt,
	.s_parm = bf3703_s_parm,
	.g_parm = bf3703_g_parm,
	//jmq.add needed subdev function
	.s_crystal_freq = bf3703_s_crystal_freq,
	.g_fmt = bf3703_g_fmt,
	.enum_framesizes = bf3703_enum_framesizes,
	.enum_frameintervals = bf3703_enum_frameintervals,
	
};

static const struct v4l2_subdev_ops bf3703_ops = {
	.core = &bf3703_core_ops,
	.video = &bf3703_video_ops,
};

/* ----------------------------------------------------------------------- */

static int bf3703_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct bf3703_info *info;
	int ret;

	bf3703_dbg("bf3703_probe\n");
	info = kzalloc(sizeof(struct bf3703_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	strcpy(sd->name, "0v7670");
	v4l2_i2c_subdev_init(sd, client, &bf3703_ops);

#if 0//jmq.disable here, for fimc's clk haven't on until now, and the bf3703 needs the mclk
	/* Make sure it's an bf3703 */
	ret = bf3703_detect(sd);
	if (ret) {
		v4l_dbg(1, debug, client,
			"chip found @ 0x%x (%s) is not an bf3703 chip.\n",
			client->addr << 1, client->adapter->name);
		kfree(info);
		return ret;
	}
#endif	
	v4l_info(client, "chip found @ 0x%02x (%s)\n",
			client->addr << 1, client->adapter->name);

	info->fmt = &bf3703_formats[0];
	info->sat = 128;	/* Review this */

	return 0;
}


static int bf3703_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}
int bf3703_suspend(struct i2c_client *client, pm_message_t state)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	bf3703_info("%s\n",__func__);
	return 0;
}
int bf3703_resume(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	bf3703_info("%s\n",__func__);	
	//It's better to restore the reg here, but the I2c write error here,why?
	detected = 0;//To detecte the device again after suspend
	return 0;
}
static const struct i2c_device_id bf3703_id[] = {
	{ "bf3703", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bf3703_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "bf3703",
	.probe = bf3703_probe,
	.remove = bf3703_remove,
	.suspend = bf3703_suspend,
	.resume = bf3703_resume,
	.id_table = bf3703_id,
};
