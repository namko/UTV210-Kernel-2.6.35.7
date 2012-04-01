

#define TVP5150_COMPLETE
#ifndef __TVP5150_H__
#define __TVP5150_H__

struct tvp5150_reg {
	unsigned char addr;
	unsigned char val;
};

struct tvp5150_regset_type {
	unsigned char *regset;
	int len;
};

/*
 * Macro
 */
#define REGSET_LENGTH(x)	(sizeof(x)/sizeof(tvp5150_reg))

/*
 * User defined commands
 */
/* S/W defined features for tune */
#define REG_DELAY	0xFF00	/* in ms */
#define REG_CMD		0xFFFF	/* Followed by command */

/* Following order should not be changed */
enum image_size_tvp5150 {
	/* This SoC supports upto UXGA (1600*1200) */
#if 0
	QQVGA,	/* 160*120*/
	QCIF,	/* 176*144 */
	QVGA,	/* 320*240 */
	CIF,	/* 352*288 */
#endif
	VGA,	/* 640*480 */
#if 0
	SVGA,	/* 800*600 */
	HD720P,	/* 1280*720 */
	SXGA,	/* 1280*1024 */
	UXGA,	/* 1600*1200 */
#endif
};

/*
 * Following values describe controls of camera
 * in user aspect and must be match with index of tvp5150_regset[]
 * These values indicates each controls and should be used
 * to control each control
 */
enum tvp5150_control {
	TVP5150_INIT,
	TVP5150_EV,
	TVP5150_AWB,
	TVP5150_MWB,
	TVP5150_EFFECT,
	TVP5150_CONTRAST,
	TVP5150_SATURATION,
	TVP5150_SHARPNESS,
};

#define TVP5150_REGSET(x)	{	\
	.regset = x,			\
	.len = sizeof(x)/sizeof(tvp5150_reg),}


/*
 * User tuned register setting values
 */
u8 tvp5150_init_reg[][2] =
{
  {0x0a, 0xa0}, //color saturation control		//gavin 09-09-26 设置色饱和初值
  {0x0c, 0xBE},//contrast control				//gavin 09-09-26 设置对比度初值
  {0x03, 0x0D},	

  {0x12, 0x00},
  {0x00, 0x00},		// CVBS-CH1	
		
  {0x28, 0x00},				
  {0x0F, 0x0A},		// set pin 27 = GPCL for v02.03
  {0x03, 0x6F},		// GPCL HIGH FOR ANALOG SW to CVBS, YUV output enable
  {0x15, 0x05},		// 0x05: ADI RTC mode
  {0xC8, 0x80},		// BuffThresh			set to trigger int when 1 transaction is stored
	{0xCA, 0x8C},		// IntLineNo			enable odd field, set to line 12
	{0xCE, 0x01},		// VidStandard			set 601 sampling
	{0xCF, 0x00},		// Full field enable    disable
	{0xEE, 0xE7},		// Line21 (Field 1)- CC NTSC, was 0xE7/0xC7
	{0xEF, 0xE7},		// Line21 (Field 2)- CC NTSC, was 0xE7/0xC7 
	{0xCB, 0x4E},		// Set Pixel Alignment [7:0] to 4Eh
	{0xCC, 0x00},		// Set pixel Alignment [9:8] to 0
	{0xCD, 0x00},		// Disable Host access VBI FIFO: FIFO outputs to 656 port
	{0xC9, 0x00},		// Reset FIFO
};


#define TVP5150_INIT_REGS	\
	(sizeof(tvp5150_init_reg) / sizeof(tvp5150_init_reg[0]))

/*
 * EV bias
 */

static const struct tvp5150_reg tvp5150_ev_m6[] = {
};

static const struct tvp5150_reg tvp5150_ev_m5[] = {
};

static const struct tvp5150_reg tvp5150_ev_m4[] = {
};

static const struct tvp5150_reg tvp5150_ev_m3[] = {
};

static const struct tvp5150_reg tvp5150_ev_m2[] = {
};

static const struct tvp5150_reg tvp5150_ev_m1[] = {
};

static const struct tvp5150_reg tvp5150_ev_default[] = {
};

static const struct tvp5150_reg tvp5150_ev_p1[] = {
};

static const struct tvp5150_reg tvp5150_ev_p2[] = {
};

static const struct tvp5150_reg tvp5150_ev_p3[] = {
};

static const struct tvp5150_reg tvp5150_ev_p4[] = {
};

static const struct tvp5150_reg tvp5150_ev_p5[] = {
};

static const struct tvp5150_reg tvp5150_ev_p6[] = {
};

#ifdef TVP5150_COMPLETE
/* Order of this array should be following the querymenu data */
static const unsigned char *tvp5150_regs_ev_bias[] = {
	(unsigned char *)tvp5150_ev_m6, (unsigned char *)tvp5150_ev_m5,
	(unsigned char *)tvp5150_ev_m4, (unsigned char *)tvp5150_ev_m3,
	(unsigned char *)tvp5150_ev_m2, (unsigned char *)tvp5150_ev_m1,
	(unsigned char *)tvp5150_ev_default, (unsigned char *)tvp5150_ev_p1,
	(unsigned char *)tvp5150_ev_p2, (unsigned char *)tvp5150_ev_p3,
	(unsigned char *)tvp5150_ev_p4, (unsigned char *)tvp5150_ev_p5,
	(unsigned char *)tvp5150_ev_p6,
};

/*
 * Auto White Balance configure
 */
static const struct tvp5150_reg tvp5150_awb_off[] = {
};

static const struct tvp5150_reg tvp5150_awb_on[] = {
};

static const unsigned char *tvp5150_regs_awb_enable[] = {
	(unsigned char *)tvp5150_awb_off,
	(unsigned char *)tvp5150_awb_on,
};

/*
 * Manual White Balance (presets)
 */
static const struct tvp5150_reg tvp5150_wb_tungsten[] = {

};

static const struct tvp5150_reg tvp5150_wb_fluorescent[] = {

};

static const struct tvp5150_reg tvp5150_wb_sunny[] = {

};

static const struct tvp5150_reg tvp5150_wb_cloudy[] = {

};

/* Order of this array should be following the querymenu data */
static const unsigned char *tvp5150_regs_wb_preset[] = {
	(unsigned char *)tvp5150_wb_tungsten,
	(unsigned char *)tvp5150_wb_fluorescent,
	(unsigned char *)tvp5150_wb_sunny,
	(unsigned char *)tvp5150_wb_cloudy,
};

/*
 * Color Effect (COLORFX)
 */
static const struct tvp5150_reg tvp5150_color_sepia[] = {
};

static const struct tvp5150_reg tvp5150_color_aqua[] = {
};

static const struct tvp5150_reg tvp5150_color_monochrome[] = {
};

static const struct tvp5150_reg tvp5150_color_negative[] = {
};

static const struct tvp5150_reg tvp5150_color_sketch[] = {
};

/* Order of this array should be following the querymenu data */
static const unsigned char *tvp5150_regs_color_effect[] = {
	(unsigned char *)tvp5150_color_sepia,
	(unsigned char *)tvp5150_color_aqua,
	(unsigned char *)tvp5150_color_monochrome,
	(unsigned char *)tvp5150_color_negative,
	(unsigned char *)tvp5150_color_sketch,
};

/*
 * Contrast bias
 */
static const struct tvp5150_reg tvp5150_contrast_m2[] = {
};

static const struct tvp5150_reg tvp5150_contrast_m1[] = {
};

static const struct tvp5150_reg tvp5150_contrast_default[] = {
};

static const struct tvp5150_reg tvp5150_contrast_p1[] = {
};

static const struct tvp5150_reg tvp5150_contrast_p2[] = {
};

static const unsigned char *tvp5150_regs_contrast_bias[] = {
	(unsigned char *)tvp5150_contrast_m2,
	(unsigned char *)tvp5150_contrast_m1,
	(unsigned char *)tvp5150_contrast_default,
	(unsigned char *)tvp5150_contrast_p1,
	(unsigned char *)tvp5150_contrast_p2,
};

/*
 * Saturation bias
 */
static const struct tvp5150_reg tvp5150_saturation_m2[] = {
};

static const struct tvp5150_reg tvp5150_saturation_m1[] = {
};

static const struct tvp5150_reg tvp5150_saturation_default[] = {
};

static const struct tvp5150_reg tvp5150_saturation_p1[] = {
};

static const struct tvp5150_reg tvp5150_saturation_p2[] = {
};

static const unsigned char *tvp5150_regs_saturation_bias[] = {
	(unsigned char *)tvp5150_saturation_m2,
	(unsigned char *)tvp5150_saturation_m1,
	(unsigned char *)tvp5150_saturation_default,
	(unsigned char *)tvp5150_saturation_p1,
	(unsigned char *)tvp5150_saturation_p2,
};

/*
 * Sharpness bias
 */
static const struct tvp5150_reg tvp5150_sharpness_m2[] = {
};

static const struct tvp5150_reg tvp5150_sharpness_m1[] = {
};

static const struct tvp5150_reg tvp5150_sharpness_default[] = {
};

static const struct tvp5150_reg tvp5150_sharpness_p1[] = {
};

static const struct tvp5150_reg tvp5150_sharpness_p2[] = {
};

static const unsigned char *tvp5150_regs_sharpness_bias[] = {
	(unsigned char *)tvp5150_sharpness_m2,
	(unsigned char *)tvp5150_sharpness_m1,
	(unsigned char *)tvp5150_sharpness_default,
	(unsigned char *)tvp5150_sharpness_p1,
	(unsigned char *)tvp5150_sharpness_p2,
};
#endif /* TVP5150_COMPLETE */

#endif
