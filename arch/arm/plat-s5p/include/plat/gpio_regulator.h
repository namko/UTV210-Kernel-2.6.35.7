#ifndef _GPIO_REGULATOR_H
#define _GPIO_REGULATOR_H

#include <linux/regulator/machine.h>

enum gpio_reg_type
{	
	GPIO_REGULATOR_VOLTAGE,
	GPIO_REGULATOR_CURRENT,
};

enum gpio_pull_mode
{
	GPIO_PULL_NONE,
	GPIO_PULL_UP,
	GPIO_PULL_DOWN,
};

struct gpio_regulator {
	/* Configuration parameters */
	int gpio;
	int active_low;
	char *name;
	enum gpio_reg_type type;		/*0:for valtage, 1: current */
	enum gpio_pull_mode pull;
	int suspend;
	int last_state;
	struct regulator_init_data	*initdata;
};

struct gpio_regulator_platform_data {
	int num_regulators;
	struct gpio_regulator *regulators;
};

#endif
