#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/regulator/act8937.h>
#include <plat/gpio_regulator.h>

#ifdef CONFIG_REGULATOR_ACT8937

static struct regulator_init_data act8937_buck1_data = {
	.constraints	= {
		.name		= "VCC_MEM_IO1",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.uV	= 1800000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 0,
		},
	},
};

static struct regulator_consumer_supply buck2_consumers[] = {
	{
		.supply		= "vddint",
	},
};

static struct regulator_init_data act8937_buck2_data = {
	.constraints	= {
		.name		= "VCC_INTERNAL",
		.min_uV		= 950000,
		.max_uV		= 1200000,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 0,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck2_consumers),
	.consumer_supplies	= buck2_consumers,
};


static struct regulator_consumer_supply buck3_consumers[] = {
	{
		.supply		= "vddarm",
	},
};
static struct regulator_init_data act8937_buck3_data = {
	.constraints	= {
		.name		= "VCC_ARM",
		.min_uV		= 1250000,
		.max_uV		= 1250000,
		.apply_uV	= 1,
		.state_mem	= {
			.uV	= 1250000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck3_consumers),
	.consumer_supplies	= buck3_consumers,
};

static struct regulator_init_data act8937_ldo4_data = {
	.constraints	= {
		.name		= "VDD_PLL",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.state_mem = {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0,  //?
		},
	},
};

static struct regulator_init_data act8937_ldo5_data = {
	.constraints	= {
		.name		= "VALIVE_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.state_mem = {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 1,	/* LDO4 should be OFF in sleep mode */
		},
	},
};

static struct regulator_consumer_supply ldo6_consumers[] = {
	[0] = {
		.supply		= "vdd_hdmi_1.1",
	},
	[1] = {
		.supply		= "vdd_mipi",
	},
	[2] = {
		.supply		= "vdd_uhost_1.1",
	},
	[3] = {
		.supply		= "pd_io",//vdd_uotg_1.1
	},
};
static struct regulator_init_data act8937_ldo6_data = {
	.constraints	= {
		.name		= "VDD_IO2/VDD_OTD_D",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0, /*TBD*/
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo6_consumers),
	.consumer_supplies	= ldo6_consumers,
};

static struct regulator_consumer_supply ldo7_consumers[] = {
	[0] = {
		.supply		= "vdd_dac",
	},
	[1] = {
		.supply		= "vdd_hdmi_3.3",
	},
	[2] = {
		.supply		= "vdd_uhost_3.3",
	},
	[3] = {
		.supply		= "pd_core",//vdd_uotg_3.3
	},
};
static struct regulator_init_data act8937_ldo7_data = {
	.constraints	= {
		.name		=  "VDD_IO3/VDD_OTD_A",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.uV		= 3300000,
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled	= 0,   /*TBD*/
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo7_consumers),
	.consumer_supplies	= ldo7_consumers,
};


static struct act8937_regulator_subdev t34_v210_regulators[] = {
	{ ACT8937_DCDC1, &act8937_buck1_data },
	{ ACT8937_DCDC2, &act8937_buck2_data },
	{ ACT8937_DCDC3, &act8937_buck3_data },
	{ ACT8937_LDO4,   &act8937_ldo4_data },
	{ ACT8937_LDO5,   &act8937_ldo5_data },
	{ ACT8937_LDO6,   &act8937_ldo6_data },
	{ ACT8937_LDO7,   &act8937_ldo7_data },
};


struct act8937_platform_data act8937_platform_data = {
	.num_regulators = ARRAY_SIZE(t34_v210_regulators),
	.regulators     = t34_v210_regulators,
};
#endif



#if 1 //jeff, T34, GPIO controlled external LDOs, for some mix usage case

/* 
 *T34 GPIO controlled Power. We init mix used GPIO as regulator, 
 *leave others single power to drivers.  
 *
GPIO_LDOs_name  GPIO			
SD_PWREN	      GPG1_0	VDD_SD		
GPS_PWREN   	      GPA0_7	VDD_GPS12	VDD_GPS28	
CAM_PWREN	      GPB3  	VDD_CAM		
BB_PWREN	      GPH1_3	BB_EN		
WLANBT_PWREN	GPH2_6	VDD_BT ,VDD_WIFI		
TP_PWR_EN	      GPG1_5	VDD_TP_33		
5V_PWREN	      GPG1_6	VDD_5V_HDMI	VDD_5V_USB_HUB	VDD_5V_SPEAKER
LCD_BL_PWM	      GPD0_0	VDD_PWM		
BL_PWREN	      GPD0_1	VDD_LCD_BL		
LCD_PWREN	       GPD0_2	VDD_LCD		
*/
static struct regulator_consumer_supply gpio_reg_gpa07_consumers[] = {
	{
		.supply		= "vdd_gps",
	},
};
static struct regulator_init_data gpio_reg_gpa07 = {
	.constraints	= {
		.name		= "VDD_GPS12/VDD_GPS28",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 0,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gpio_reg_gpa07_consumers),
	.consumer_supplies	= gpio_reg_gpa07_consumers,
};

static struct regulator_consumer_supply gpio_reg_gpb3_consumers[] = {
	{
		.supply		= "vdd_cam",
	},
};
static struct regulator_init_data gpio_reg_gpb3 = {
	.constraints	= {
		.name		= "VDD_CAM",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 0,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gpio_reg_gpb3_consumers),
	.consumer_supplies	= gpio_reg_gpb3_consumers,
};

static struct regulator_consumer_supply gpio_reg_gpg16_consumers[] = {
	{
		.supply		= "vdd_5v_hdmi",
	},
	{
		.supply		= "vdd_5v_usb",
	},
	{
		.supply		= "vdd_5v_speaker",
	},
	
};
static struct regulator_init_data gpio_reg_gpg16 = {
	.constraints	= {
		.name		= "VDD_5V",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 0,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gpio_reg_gpg16_consumers),
	.consumer_supplies	= gpio_reg_gpg16_consumers,
};


static struct regulator_consumer_supply gpio_reg_gph26_consumers[] = {
	{
		.supply		= "vdd_bt",
	},
	{
		.supply		= "vdd_wifi",
	},
};
static struct regulator_init_data gpio_reg_gph26 = {
	.constraints	= {
		.name		= "VDD_BT/VDD_WLAN",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 0,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(gpio_reg_gph26_consumers),
	.consumer_supplies	= gpio_reg_gph26_consumers,
};

static struct gpio_regulator v210_gpio_regulators [] = {
	[0] = {  /*"VDD_GPS",*/
		.gpio =  S5PV210_GPA0(7),
		.name = "LDO_GPA0(7)",
		.type = GPIO_REGULATOR_VOLTAGE,
		.initdata = &gpio_reg_gpa07,
	},
	[1] = { /*"VDD_CAM",*/
		.gpio =  S5PV210_GPB(3),
		.name = "LDO_GPB(3)",
		.type = GPIO_REGULATOR_VOLTAGE,
		.initdata = &gpio_reg_gpb3,
	},		
	[2] = { /*"VDD_5V",*/
		.gpio =  S5PV210_GPG1(6),
		.name = "LDO_GPG1(6)",
		.type = GPIO_REGULATOR_VOLTAGE,
		.initdata = &gpio_reg_gpg16,
	},	
	[3] = { /*"VDD_BT/VDD_WLAN",*/
		.gpio =  S5PV210_GPH2(6),
		.pull = GPIO_PULL_DOWN,
		.name = "LDO_GPH2(6)",
		.type = GPIO_REGULATOR_VOLTAGE,
		.initdata = &gpio_reg_gph26,
	},		
};

struct gpio_regulator_platform_data v210_gpio_regs_platform_data  = {
	.num_regulators = ARRAY_SIZE(v210_gpio_regulators),
	.regulators = v210_gpio_regulators,
};


#endif
